#include "buffer.h"

// 读写下标初始化， vector<char>初始化
Buffer::Buffer(int initBuffSize) : _buffer(initBuffSize), _readPos(0), _writePos(0) {}

// 可写的数量
size_t Buffer::WritableBytes() const {
    return _buffer.size() - _writePos;
}

// 可读的数量
size_t Buffer::ReadableBytes() const {
    return _writePos - _readPos;
}

// 可预留空间
size_t Buffer::PrependableBytes() const {
    return _readPos;
}

// 可读数据的起始位置常量指针
const char* Buffer::Peek() const {
    return &_buffer[_readPos];
}

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len) {
    if(len > WritableBytes()) {
        _MakeSpace(len);
    }
    assert(len <= WritableBytes());
}

// 移动写下标，在Append中使用
void Buffer::HasWritten(size_t len) {
    _writePos += len;
}

// 读取len长度，移动读下标
void Buffer::Retrieve(size_t len) {
    _readPos += len;
}

// 读取到指定的结束位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end);  // 确保结束位置合法
    Retrieve(end - Peek()); // 计算长度，更新读下标
}

// 清空缓冲区，重置读写位置
void Buffer::RetrieveAll() {
    memset(&_buffer[0], 0, _buffer.size());
    _readPos = _writePos = 0;
}

// 取出所有可读数据并转换为字符串，同时清空缓冲区
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes()); // 构造字符串
    RetrieveAll();
    return str;
}

// 返回当前可写位置的常量指针
const char* Buffer::BeginWriteConst() const {
    return &_buffer[_writePos];
}

char* Buffer::BeginWrite()  {
    return &_buffer[_writePos];
}

// 将指定长度的字符数据追加到缓冲区
void Buffer::Append(const char* str, size_t len) {
    assert(str); // 确保指针非空
    EnsureWriteable(len);   // 确保有足够空间
    std::copy(str, str + len, BeginWrite()); // 将数据复制到缓冲区
    HasWritten(len); // 更新写下标
}

// 将字符串追加到缓冲区
void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.size()); 
}

// 将指定长度的二进制数据追加到缓冲区
void Buffer::Append(const void* data, size_t len) {
    Append(static_cast<const char*>(data), len);
}

// 将另一个缓冲区的数据追加到当前缓冲区
void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

// 从文件描述符读取数据到缓冲区
ssize_t Buffer::ReadFd(int fd, int* Errno) {
    char buff[65535];   // 栈区缓冲区，用于存储额外数据
    struct iovec iov[2];    // 分散读结构体
    size_t writeable = WritableBytes();

    // 缓冲区的可写区域
    iov[0].iov_base = BeginWrite();
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
        Append(buff, static_cast<size_t>(len - writeable)); // 剩余数据追加到缓冲区
    }
    return len;
}

// 将缓冲区的数据写入文件描述符
ssize_t Buffer::WriteFd(int fd, int* Errno) {
    ssize_t len = write(fd, Peek(), ReadableBytes());
    if(len < 0) {
        *Errno = errno;
        return len;
    }
    Retrieve(len);
    return len;
}

ssize_t Buffer::size() const{
    return _buffer.size();
}

// 返回缓冲区的起始位置(非常量指针)
char* Buffer::_BeginPtr() {
    return &_buffer[0];
}

// 返回缓冲区的起始位置(常量指针)
const char* Buffer::_BeginPtr() const {
    return &_buffer[0];
}

// 扩展缓冲区空间，确保有足够的空间写入指定长度的数据
void Buffer::_MakeSpace(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {
        // 如果预留空间不足，扩展缓冲区
        _buffer.resize(_writePos + len + 1);
    }
    else {
        // 如果预留空间足够，移动数据到缓冲区头部
        size_t readable = ReadableBytes();
        std::copy(_BeginPtr() + _readPos, _BeginPtr() + _writePos, _BeginPtr());
        _readPos = 0;
        _writePos = readable;
        assert(readable = ReadableBytes());
    }
}