#include "buffer.h"

/*
 * 基于vector的自动扩容缓冲区，用于非阻塞I/O的高效数据读写管理。
 * 核心设计：读写位置分离，动态空间调整，减少内存拷贝，支持分散读/集中写。
 * 
 * 非线程安全，涉及到缓冲区操作的都是在一个线程中完成，不需要考虑线程安全
 * 
 *  * 双缓冲区结构：

    每个TCP连接维护input和output两个缓冲区，形成生产者-消费者模型
    input缓冲处理TCP粘包问题，output缓冲实现零拷贝发送
    通过readerIndex/writerIndex双指针管理读写位置，避免频繁内存分配
 *
 * 原理：
 * 1. 缓冲区结构：
 *    - 使用vector<char>存储数据，自动管理内存。
 *    - 维护读位置(_readPos)和写位置(_writePos)，将缓冲区划分为：
 *      [已读可回收区域][可读数据区域][可写区域]
 *    - prependableBytes()提供头部预留空间，方便协议处理(如添加消息头)。
 *
 * 2. 自动扩容策略：
 *    - 写入数据时，若剩余空间不足，结合预留空间计算总可用空间：
 *      - 总空间不足时，vector扩容至所需大小。
 *      - 空间足够则移动可读数据到头部，腾出连续写入空间，避免重复扩容。
 *
 * 3. 高效I/O操作：
 *    - readFd()使用readv实现分散读，先填满缓冲区，超额数据存入栈内存再追加。
 *    - writeFd()直接写入可读数据区域，减少内存拷贝。
 *    - 读写指针分离，自动回收已读空间，避免频繁内存搬运。
 *
 * 4. 应用场景：
 *    - 处理TCP粘包/分包问题，提供可读数据区间的访问。
 *    - 高效管理用户态缓冲区，减少系统调用次数。
 *    - 支持数据追加、头部预留等常见网络编程需求。
 */


// 读写下标初始化， vector<char>初始化
Buffer::Buffer(int initBuffSize) : _buffer(initBuffSize), _readPos(0), _writePos(0) {}

// 可写的数量
size_t Buffer::writableBytes() const {
    return _buffer.size() - _writePos;
}

// 可读的数量
size_t Buffer::readableBytes() const {
    return _writePos - _readPos;
}

// 可预留空间
size_t Buffer::prependableBytes() const {
    return _readPos;
}

// 可读数据的起始位置常量指针
const char* Buffer::peek() const {
    return &_buffer[_readPos];
}

// 确保可写的长度
void Buffer::ensureWriteable(size_t len) {
    if(len > writableBytes()) {
        _makeSpace(len);
    }
    assert(len <= writableBytes());
}

// 移动写下标，在append中使用
void Buffer::hasWritten(size_t len) {
    _writePos += len;
}

// 读取len长度，移动读下标
void Buffer::retrieve(size_t len) {
    _readPos += len;
}

// 读取到指定的结束位置
void Buffer::retrieveUntil(const char* end) {
    assert(peek() <= end);  // 确保结束位置合法
    retrieve(end - peek()); // 计算长度，更新读下标
}

// 清空缓冲区，重置读写位置
void Buffer::retrieveAll() {
    memset(&_buffer[0], 0, _buffer.size());
    _readPos = _writePos = 0;
}

// 取出所有可读数据并转换为字符串，同时清空缓冲区
std::string Buffer::retrieveAllToStr() {
    std::string str(peek(), readableBytes()); // 构造字符串
    retrieveAll();
    return str;
}

// 返回当前可写位置的常量指针
const char* Buffer::beginWriteConst() const {
    return &_buffer[_writePos];
}

char* Buffer::beginWrite()  {
    return &_buffer[_writePos];
}

// 将指定长度的字符数据追加到缓冲区
void Buffer::append(const char* str, size_t len) {
    assert(str); // 确保指针非空
    ensureWriteable(len);   // 确保有足够空间
    std::copy(str, str + len, beginWrite()); // 将数据复制到缓冲区
    hasWritten(len); // 更新写下标
}

// 将字符串追加到缓冲区
void Buffer::append(const std::string& str) {
    append(str.c_str(), str.size()); 
}

// 将指定长度的二进制数据追加到缓冲区
void Buffer::append(const void* data, size_t len) {
    append(static_cast<const char*>(data), len);
}

// 将另一个缓冲区的数据追加到当前缓冲区
void Buffer::append(const Buffer& buff) {
    append(buff.peek(), buff.readableBytes());
}

// 从文件描述符读取数据到缓冲区
// 保证一次能够读取完文件描述符里的数据，减少系统调用，具体为啥想想read系统调用传入的参数是啥。
ssize_t Buffer::readFd(int fd, int* Errno) {
    char buff[65535];   // 栈区缓冲区，用于存储额外数据
    struct iovec iov[2];    // 分散读结构体
    size_t writeable = writableBytes();
    // 缓冲区的可写区域
    iov[0].iov_base = beginWrite();
    iov[0].iov_len = writeable;
    // 栈区缓冲区
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    // 使用readv从文件描述符读取数据
    ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *Errno = errno;
    }
    else if(static_cast<size_t>(len) <= writeable) {
        _writePos += len;   // 数据全部写入缓冲区
    }
    else {
        _writePos = _buffer.size(); // 缓冲区写满
        append(buff, static_cast<size_t>(len - writeable)); // 剩余数据追加到缓冲区
    }
    return len;
}

// 将缓冲区的数据写入文件描述符
ssize_t Buffer::writeFd(int fd, int* Errno) {
    ssize_t len = write(fd, peek(), readableBytes());
    if(len < 0) {
        *Errno = errno;
        return len;
    }
    retrieve(len);
    return len;
}

ssize_t Buffer::size() const{
    return _buffer.size();
}

// 返回缓冲区的起始位置(非常量指针)
char* Buffer::_beginPtr() {
    return &_buffer[0];
}

// 返回缓冲区的起始位置(常量指针)
const char* Buffer::_beginPtr() const {
    return &_buffer[0];
}

// 扩展缓冲区空间，确保有足够的空间写入指定长度的数据
void Buffer::_makeSpace(size_t len) {
    if(writableBytes() + prependableBytes() < len) {
        // 如果预留空间不足，扩展缓冲区
        _buffer.resize(_writePos + len + 1);
    }
    else {
        // 如果预留空间足够，移动数据到缓冲区头部
        size_t readable = readableBytes();
        std::copy(_beginPtr() + _readPos, _beginPtr() + _writePos, _beginPtr());
        _readPos = 0;
        _writePos = readable;
        assert(readable = readableBytes());
    }
}