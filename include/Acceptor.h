#pragma once

#include <functional>

#include "NonCopyable.h"
#include "Channel.h"
#include "Socket.h"

class EventLoop;
class InetAddress;

class Acceptor : NonCopyable {
    public:
        using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

        Acceptor(EventLoop *loop, const InetAddress &listenaddr, bool reuseport);
        ~Acceptor();
        // 设置有新连接到来时的回调
        void setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }
        // 是否正在监听
        bool listenning() const { return listenning_; }
        // 开始监听
        void listen();

    private:
        void handleRead(); // 处理新用户连接

        EventLoop *loop_;  // Acceptor属于mainReactor，监听新连接
        Socket acceptSocket_; // 接收新连接的socket
        Channel acceptChannel_; // 监听新连接的channel
        NewConnectionCallback newConnectionCallback_; // 有新连接到来时的回调
        bool listenning_; // 是否正在监听
};