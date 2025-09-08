#include "buffer.h"
#include <string_view>
#include <sys/uio.h>
#include <unistd.h>
#include <cerrno>

namespace net {

Buffer::Buffer(size_t init_size) : buffer_(init_size), read_index_(0), write_index_(0) {}

size_t Buffer::ReadableBytes() const {
    return write_index_ - read_index_;
}

size_t Buffer::WriteableBytes() const {
    return buffer_.size() - write_index_;
}

size_t Buffer::PrependableBytes() const {
    return read_index_;
}

const char* Buffer::Peek() const {
    return Begin_() + read_index_;
}

void Buffer::Retrieve(size_t len) {
    if (len < ReadableBytes()) {
        read_index_ += len;
    } else {
        RetrieveAll();
    }
}

// TODO: 收缩内存
void Buffer::RetrieveAll() {
    read_index_ = 0;
    write_index_ = 0;
}

std::string Buffer::RetrieveAllToString() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

char* Buffer::BeginWrite() {
    return Begin_() + write_index_;
}

const char* Buffer::BeginWriteConst() const {
    return Begin_() + write_index_;
}

void Buffer::HasWritten(size_t len) {
    write_index_ += len;
}

void Buffer::EnsureWriteable(size_t len) {
    if (WriteableBytes() < len) {
        MakeSpace_(len);
    }
}

void Buffer::Append(std::string_view str) {
    Append(str.data(), str.size());
}

void Buffer::Append(const char* data, size_t len) {
    EnsureWriteable(len);
    std::copy(data, data + len, BeginWrite());
    HasWritten(len);   
}

ssize_t Buffer::ReadFd(int fd, int* saved_errno) {
    char extra_buffer[65536];
    struct iovec vec[2];

    const size_t writeable = WriteableBytes();
    vec[0].iov_base = BeginWrite();
    vec[0].iov_len = writeable;
    vec[1].iov_base = extra_buffer;
    vec[1].iov_len = sizeof(extra_buffer);

    ssize_t n = readv(fd, vec, 2);
    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writeable) {
        HasWritten(n);
    } else {
        HasWritten(writeable);
        Append(extra_buffer, n - writeable);
    }
    return n;
}

ssize_t Buffer::WriteFd(int fd, int* saved_errno) {
    ssize_t n = write(fd, Peek(), ReadableBytes());
    if (n < 0) {
        *saved_errno = errno;
    } else {
        Retrieve(n);
    }
    return n;
}

char* Buffer::Begin_() {
    return &buffer_[0];
}

const char* Buffer::Begin_() const {
    return &buffer_[0];
}

void Buffer::MakeSpace_(size_t len) {
    if (PrependableBytes() + WriteableBytes() < len) {
        buffer_.resize(write_index_ + len);
    } else { // 内部挪腾
        size_t readable = ReadableBytes();
        std::copy(Begin_() + read_index_, Begin_() + write_index_, Begin_());
        read_index_ = 0;
        write_index_ = readable;
    }
}

}