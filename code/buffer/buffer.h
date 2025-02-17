#pragma once
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <unistd.h>
#include <sys/uio.h>
#include <cassert>

/**
 * 1. 动态扩容：
 * - 写入溢出：待写入数据长度超过空间，内部挪腾后仍无法容纳新数据
 * - 外部缓冲区合并：readv读取，栈缓冲数据合并到主缓冲区
 * 
 * 2. 更智能的容量规划：
 * - 初始预留8字节头部空间，方便协议封装
 * - 根据负载渐进式扩容，而非翻倍
 * 
 * 3.零拷贝优化
 * - 优先通过内部挪腾代替扩容
 * - 
 */

class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t writableBytes() const;       
    size_t readableBytes() const ;
    size_t prependableBytes() const;

    const char* peek() const;
    void ensureWriteable(size_t len);
    void hasWritten(size_t len);

    void retrieve(size_t len);
    void retrieveUntil(const char* end);

    void retrieveAll() ;
    std::string retrieveAllToStr();

    const char* beginWriteConst() const;
    char* beginWrite();

    void append(const std::string& str);
    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const Buffer& buff);

    ssize_t readFd(int fd, int* Errno);
    ssize_t writeFd(int fd, int* Errno);

    ssize_t size() const;

private:
    char* _beginPtr();  // buffer开头
    const char* _beginPtr() const;
    void _makeSpace(size_t len);

    std::vector<char> _buffer;
    std::atomic<std::size_t> _readPos;  // 读的下标
    std::atomic<std::size_t> _writePos; // 写的下标
};
