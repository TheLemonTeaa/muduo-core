#pragma once

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "NonCopyable.h"
#include "TcpConnection.h"
#include "InetAddress.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "Buffer.h"

class TcpServer{
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;
        enum Option {
            kNoReusePort, // 不使用端口复用
            kReusePort, // 端口复用
        };

        TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &nameArg, Option option = kNoReusePort);
        ~TcpServer();

        EventLoop *getLoop() const { return loop_; }

        // 设置底层subloop的个数
        void setThreadNum(int numThreads);
        // 启动服务器
        void start();

        // 设置线程初始化回调
        void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
        // 设置新连接回调
        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
        // 设置消息到来回调
        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
        // 设置写完成回调
        void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    private:
        void newConnection(int sockfd, const InetAddress &peerAddr); // 有新连接到来
        void removeConnection(const TcpConnectionPtr &conn); // 连接关闭，移除连接
        void removeConnectionInLoop(const TcpConnectionPtr &conn); // 在IO线程中移除连接

        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
        
        EventLoop *loop_; // 该TcpServer属于mainReactor，负责监听新连接

        const std::string ipPort_; // 服务器监听的ip:port
        const std::string name_; // 服务器名字

        std::unique_ptr<Acceptor> acceptor_; // 运行在mainReactor，监听新连接

        std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

        ThreadInitCallback threadInitCallback_; // 线程初始化回调
        ConnectionCallback connectionCallback_; // 有新连接时的回调
        MessageCallback messageCallback_; // 有读写消息时的回调
        WriteCompleteCallback writeCompleteCallback_; // 消息发送完成后的回调

        std::atomic_int started_; // 原子操作，记录服务器是否启动
    
        int nextConnId_; // 下一个连接的id
        ConnectionMap connections_; // 保存所有的连接
};