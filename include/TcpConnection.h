#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "NonCopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

class TcpConnection : NonCopyable, public std::enable_shared_from_this<TcpConnection> {
    public:
        TcpConnection(EventLoop *loop,
                      const std::string &name,
                      int sockfd,
                      const InetAddress &localaddr,
                      const InetAddress &peeraddr);
        ~TcpConnection();

        EventLoop* getLoop() const { return loop_; }
        const std::string& name() const { return name_; }
        const InetAddress& localAddress() const { return localaddr_; }
        const InetAddress& peerAddress() const { return peeraddr_; }
        bool connected() const { return state_ == kConnected; }

        // 发送数据
        void send(const std::string &buf);
        // 关闭连接
        void shutdown(); 

        // 设置回调函数
        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
        void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
        void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
        void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
            highWaterMarkCallback_ = cb;
            highWaterMark_ = highWaterMark;
        }

        // 连接建立
        void connectEstablished();
        // 连接销毁
        void connectDestroyed();
    private:
        enum StateE { 
            kDisconnected, // 已断开连接
            kConnecting, // 连接中
            kConnected, // 已连接
            kDisconnecting // 断开连接中
        };
        void setState(StateE state) { state_ = state; }

        void handleRead(Timestamp receiveTime); // 可读事件的回调
        void handleWrite(); // 可写事件的回调
        void handleClose(); // 关闭事件的回调
        void handleError(); // 错误事件的回调

        void sendInLoop(const void *data, size_t len);
        void shutdownInLoop();
        
        EventLoop *loop_; // 该连接属于哪个EventLoop
        const std::string name_; // 连接名称，唯一标识该连接
        std::atomic_int state_; // 连接状态
        bool reading_; // 标识是否正在读

        std::unique_ptr<Socket> socket_; // 该TcpConnection管理一个socket
        std::unique_ptr<Channel> channel_; // 该TcpConnection管理一个channel
        
        const InetAddress localaddr_; // 本地地址
        const InetAddress peeraddr_; // 对端地址

        // 用户通过TcpServer设置的回调函数, 最终会传递给TcpConnection, TcpConnection再将回调注册给channel
        ConnectionCallback connectionCallback_; // 连接建立和断开的回调
        MessageCallback messageCallback_; // 读写消息的回调
        WriteCompleteCallback writeCompleteCallback_; // 消息发送完成的回调
        HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调
        CloseCallback closeCallback_; // 连接关闭的回调
        size_t highWaterMark_; // 高水位标记

        // 读缓冲区
        Buffer inputBuffer_;
        // 写缓冲区
        Buffer outputBuffer_;
};