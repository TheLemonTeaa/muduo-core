#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

static int createNonblocking() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n",
                  __FILE__, __FUNCTION__, __LINE__, errno);
    }

    // 设置非阻塞
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) LOG_FATAL("fcntl F_GETFL error: %d\n", errno);
    if (::fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
        LOG_FATAL("fcntl F_SETFL error: %d\n", errno);

    // 设置 close-on-exec
    int fdFlags = ::fcntl(sockfd, F_GETFD, 0);
    if (fdFlags < 0) LOG_FATAL("fcntl F_GETFD error: %d\n", errno);
    if (::fcntl(sockfd, F_SETFD, fdFlags | FD_CLOEXEC) < 0)
        LOG_FATAL("fcntl F_SETFD error: %d\n", errno);

    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenaddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()), // 创建非阻塞socket
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false) {
    acceptSocket_.setReuseAddr(true); // 设置地址重用
    acceptSocket_.setReusePort(reuseport); // 设置端口重用
    acceptSocket_.bindAddress(listenaddr); // 绑定地址

    // 设置有新连接到来时的回调
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll(); // 禁用所有事件
    acceptChannel_.remove(); // 从Poller中删除
}

void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen(); // 监听
    acceptChannel_.enableReading(); // acceptChannel_注册到Poller中，监听读事件
}

// listenfd有读事件发生，表示有新用户连接
void Acceptor::handleRead() {
    InetAddress peeraddr;
    int connfd = acceptSocket_.accept(&peeraddr); // 接受新连接
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peeraddr); // 轮询找到subReactor，唤醒，分发新连接
        } else {
            ::close(connfd); // 关闭连接
        }
    } else {
        LOG_ERROR("%s:%s:%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) {
            LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}