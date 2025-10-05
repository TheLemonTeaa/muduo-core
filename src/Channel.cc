#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0; // 无事件
#ifdef __linux__
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 读事件
const int Channel::kWriteEvent = EPOLLOUT; // 写事件
#elif __APPLE__
const int Channel::kReadEvent = EVFILT_READ; // 读事件
const int Channel::kWriteEvent = EVFILT_WRITE; // 写事件
#endif

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1) // -1表示还未添加到Poller中
    , tied_(false) {}

Channel::~Channel() {}

void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

// update在EventLoop::loop()中调用
void Channel::update() {
    loop_->updateChannel(this);
}
// remove在EventLoop::loop()中调用
void Channel::remove() {
    loop_->removeChannel(this);
}

// fd得到Poller通知之后 处理事件 handleEvent在EventLoop::loop()中调用
void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    // 关闭
    // TcpConnection对应的Channel通过shutdown关闭连接时, 会触发EPOLLHUP事件
#ifdef __linux__
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
#elif __APPLE__
    if ((revents_ & EV_EOF) && !(revents_ & EVFILT_READ)) {
#endif
        if (closeCallback_) closeCallback_();
    }
    // 出错
#ifdef __linux__
    if (revents_ & EPOLLERR) {
#elif __APPLE__
    if (revents_ & EV_ERROR) {
#endif
        if (errorCallback_) errorCallback_();
    }
    // 可读事件
#ifdef __linux__
    if (revents_ & (EPOLLIN | EPOLLPRI)) {
#elif __APPLE__
    if (revents_ & (EVFILT_READ)) {
#endif
        if (readCallback_) readCallback_(receiveTime);
    }
    // 可写事件
#ifdef __linux__    
    if (revents_ & EPOLLOUT) {
#elif __APPLE__
    if (revents_ & EVFILT_WRITE) {
#endif
        if (writeCallback_) writeCallback_();
    }
}