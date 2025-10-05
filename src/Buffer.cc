#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>

#include "Buffer.h"

/** 
 * 从socket读到缓冲区的方法是使用readv先读至buffer_，
 * buffer_空间如果不够会读入到栈上65536个字节大小的空间，然后以append的
 * 方式追加入buffer_。既考虑了避免系统调用带来开销，又不影响数据的接收。
**/

ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char extrabuf[65536] = {0}; // 栈上空间 64k
    /*
    struct iovec {
        ptr_t iov_base; // iov_base指向的缓冲区存放的是readv所接收的数据或是writev将要发送的数据
        size_t iov_len; // iov_len在各种情况下分别确定了接收的最大长度以及实际写入的长度
    };
    */
    struct iovec vec[2]; // 使用iovec开辟两个缓冲区
    const size_t writable = writableBytes();

    // 第一块缓冲区指向可写空间
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区指向栈上空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // 当缓冲区空间足够时，只读入buffer_，否则读入buffer_和栈上空间
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        writeIndex_ += n;
    } else {
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 追加到缓冲区
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno) {
    size_t n = readableBytes();
    ssize_t nwrote = ::write(fd, peek(), n);
    if (nwrote < 0) {
        *savedErrno = errno;
    }
    return nwrote;
}