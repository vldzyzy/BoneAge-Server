#pragma once
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <unistd.h>
#include <sys/uio.h>
#include <cassert>

class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

    ssize_t size() const;

private:
    char* _BeginPtr();  // buffer开头
    const char* _BeginPtr() const;
    void _MakeSpace(size_t len);

    std::vector<char> _buffer;
    std::atomic<std::size_t> _readPos;  // 读的下标
    std::atomic<std::size_t> _writePos; // 写的下标
};
