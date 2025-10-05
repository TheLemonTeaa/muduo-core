#pragma once

#include "NonCopyable.h"

class InetAddress;

// 封装socket fd
class Socket : NonCopyable {
    public:
        explicit Socket(int sockfd) : sockfd_(sockfd) {}
        ~Socket();

        int fd() const { return sockfd_; }

        void bindAddress(const InetAddress &localaddr);
        void listen();
        int accept(InetAddress *peeraddr);
        void shutdownWrite();

        // 设置套接字选项
        void setTcpNoDelay(bool on);
        void setReuseAddr(bool on);
        void setReusePort(bool on);
        void setKeepAlive(bool on);
    private:
        const int sockfd_;
};