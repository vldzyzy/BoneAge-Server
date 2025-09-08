#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <atomic>

namespace net {

class Buffer {
public:
    explicit Buffer(size_t init_size = 1024);
    ~Buffer() = default;

    size_t ReadableBytes() const;
    size_t WriteableBytes() const;       
    size_t PrependableBytes() const;

    const char* Peek() const;

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll() ;
    std::string RetrieveAllToString();

    char* BeginWrite();
    const char* BeginWriteConst() const;

    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Append(std::string_view str);
    void Append(const char* data, size_t len);

    ssize_t ReadFd(int fd, int* saved_errno);
    ssize_t WriteFd(int fd, int* saved_errno);

private:
    char* Begin_();  // 整个buffer的起始指针
    const char* Begin_() const;
    void MakeSpace_(size_t len);

private:
    std::vector<char> buffer_;
    size_t read_index_;  // 读的下标
    size_t write_index_; // 写的下标
};

}
