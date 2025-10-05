#include <functional>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &name,
                             int sockfd,
                             const InetAddress &localaddr,
                             const InetAddress &peeraddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(name),
      state_(kConnecting), // 连接状态设置为连接中
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localaddr_(localaddr),
      peeraddr_(peeraddr),
      highWaterMark_(64 * 1024 * 1024) { // 64M
    // 设置channel的回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at %p fd=%d\n", name_.c_str(), this, sockfd);
    socket_->setKeepAlive(true); // 开启TCP keep-alive属性
}

TcpConnection::~TcpConnection() {
    LOG_INFO("TcpConnection::dtor[%s] at %p fd=%d state=%d\n",
             name_.c_str(), this, channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) { // 单reactor情况, 发送数据的操作在当前loop所在的线程
            sendInLoop(buf.data(), buf.size());
        } else {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, shared_from_this(), buf.data(), buf.size()));
        }
    }
}
/**
 * 发送数据, 应用写数据快, 内核发送数据慢, 需要将待发送数据写入outputBuffer_缓冲区,
 * 并注册channel的可写事件, 当socket可写时, 通过handleWrite回调函数将数据发送出去
 * 且设置了高水位回调
 */
void TcpConnection::sendInLoop(const void *data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len; // 剩余待发送数据的长度
    bool faultError = false;

    // channel_第一次写数据, 且outputBuffer_中没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len); // 直接写数据到内核发送缓冲区
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                // 数据全部发送完, 就不用给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else { // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK) { // EWOULDBLOCK表示非阻塞没有数据后的正常返回
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) { // SIGPIPE
                    faultError = true;
                }
            }
        }
    }

    // 还有剩余数据没有发送完, 则放入outputBuffer_中, 并注册channel的可写事件
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            // 达到高水位标记, 执行用户注册的高水位回调函数
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((const char *)data + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting(); // 注册channel的可写事件
        }
    }
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}

void TcpConnection::shutdownInLoop() {
    if (!channel_->isWriting()) { // 还没有注册channel的可写事件, 说明outputBuffer_中没有待发送数据
        socket_->shutdownWrite(); // 关闭写端, 触发对端的EPOLLHUP事件
    }
}

// 连接建立
void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 注册channel的可读事件
    connectionCallback_(shared_from_this()); // 执行用户注册的连接建立回调
}

// 连接销毁
void TcpConnection::connectDestroyed() {
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll(); // 禁用channel的所有事件
        connectionCallback_(shared_from_this()); // 执行用户注册的连接断开回调
    }
    channel_->remove(); // 从Poller中删除channel
}

// 可读事件的回调
void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime); // 执行用户注册的读写消息回调
    } else if (n == 0) {
        handleClose(); // 对端关闭连接
    } else {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError(); // 处理错误
    }
}

// 可写事件的回调
void TcpConnection::handleWrite() {
    if (channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n); // 从缓冲区中移除已发送的数据
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting(); // 发送完所有数据, 注销channel的可写事件
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) {
                    shutdownInLoop(); // 连接正在断开, 关闭写端
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    } else {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing\n", channel_->fd());
    }
}

// 关闭事件的回调
void TcpConnection::handleClose() {
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d\n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll(); // 禁用channel的所有事件

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis); // 执行用户注册的连接断开回调
    closeCallback_(guardThis); // 执行用户注册的连接关闭回调
}

// 错误事件的回调
void TcpConnection::handleError() {
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno;
    } else {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n",
              name_.c_str(), err);
}