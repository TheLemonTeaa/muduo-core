#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// 缓冲区类
class Buffer {
    public:
        static const size_t kCheapPrepend = 8; // 预留空间大小
        static const size_t kInitialSize = 1024; // 初始缓冲区大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readIndex_(kCheapPrepend),
          writeIndex_(kCheapPrepend) {}

    size_t readableBytes() const { return writeIndex_ - readIndex_; } // 可读字节数
    size_t writableBytes() const { return buffer_.size() - writeIndex_; } // 可写字节数
    size_t prependableBytes() const { return readIndex_; } // 可预留字节数

    const char* peek() const { return begin() + readIndex_; } // 返回可读数据的起始位置
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            readIndex_ += len;
        } else {
            retrieveAll();
        }
    }
    void retrieveAll() {
        readIndex_ = kCheapPrepend;
        writeIndex_ = kCheapPrepend;
    }
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len) {
        len = std::min(len, readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, begin() + writeIndex_);
        writeIndex_ += len;
    }

    ssize_t readFd(int fd, int* savedErrno); // 从fd读取数据到缓冲区
    ssize_t writeFd(int fd, int* savedErrno); // 将缓冲区数据写入fd

    private:
        char* begin() { return buffer_.data(); }
        const char* begin() const { return buffer_.data(); }

        void makeSpace(size_t len) {
            if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
                // 重新分配空间
                buffer_.resize(writeIndex_ + len);
            } else {
                // 搬移数据
                size_t readable = readableBytes();
                std::copy(begin() + readIndex_,
                          begin() + writeIndex_,
                          begin() + kCheapPrepend);
                readIndex_ = kCheapPrepend;
                writeIndex_ = readIndex_ + readable;
            }
        }

        std::vector<char> buffer_;
        size_t readIndex_;
        size_t writeIndex_;
};