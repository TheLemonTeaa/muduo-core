#include <functional>
#include <string>
#include <assert.h>

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenaddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenaddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_() // TcpServer默认没有设置回调
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // 当有新连接时，Acceptor会执行该回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2)
    );
}

TcpServer::~TcpServer() {
    for (auto &item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

// 启动服务器监听
void TcpServer::start() {
    if (started_.fetch_add(1) == 0) {
        threadPool_->start(threadInitCallback_); // 启动底层的subloops
        loop_->runInLoop(
            std::bind(&Acceptor::listen, acceptor_.get()) // 让mainLoop监听listenfd的可读事件
        );
    }
}

// 新用户连接时的回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    // 轮询算法选择一个subLoop管理connfd对应的channel
    EventLoop *ioLoop = threadPool_ -> getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    // 获取该连接的本地地址
    sockaddr_in local;
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0) {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);
    // 创建一个TcpConnection对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn; // 保存连接

    // 下面的回调都是用户设置给TcpServer => TcpConnection
    conn->setConnectionCallback(connectionCallback_); // 设置连接建立和断开的回调
    conn->setMessageCallback(messageCallback_); // 设置读写消息的回调
    conn->setWriteCompleteCallback(writeCompleteCallback_); // 设置消息发送完成后的回调

    conn->setCloseCallback( // 设置连接关闭的回调
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn)); // 让subLoop执行该新连接的回调
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    // 该函数可能在mainLoop也可能在subLoop中调用
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1); // 确保连接存在

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop( // 在subLoop中销毁连接
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}