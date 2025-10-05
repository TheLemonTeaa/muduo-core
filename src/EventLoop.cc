#ifdef __linux__
#include <sys/eventfd.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

#include "EventLoop.h"
#include "Poller.h"
#include "Channel.h"
#include "Logger.h"

__thread EventLoop *t_loopInThisThread = nullptr; // 线程局部变量, 指向当前线程的EventLoop对象

const int kPollTimeMs = 10000; // epoll_wait的超时时间

int createEventfd() {
#ifdef __linux__
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
#elif __APPLE__
    int evtfd = ::kqueue();
    if (evtfd < 0) {
        LOG_FATAL("kqueue error:%d\n", errno);
    }
    return evtfd;
#endif
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    , callingPendingFunctors_(false) {
        LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
        if (t_loopInThisThread) {
            LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
        } else {
            t_loopInThisThread = this;
        }

        // 设置wakeupChannel_的读事件回调函数
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        wakeupChannel_->enableReading(); // 启用wakeupChannel_的读事件
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开始事件循环
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while(!quit_) {
        activeChannels_.clear();
        // 监听IO事件, 返回发生事件的channels
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_) {
            channel->handleEvent(pollReturnTime_); // 调用channel的事件处理函数
        }
        // 执行回调操作
        doPendingFunctors();
    }
}

// 唤醒loop所在线程, 向wakeupFd_写一个数据, wakeupChannel就发生读事件, loop就被唤醒
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup(); // 不是在当前loop所在的线程调用quit, 需要唤醒loop所在的线程
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) { // 在当前loop所在的线程调用
        cb();
    } else {
        queueInLoop(cb); // 放入队列, 唤醒loop所在的线程, 执行cb
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 如果不在当前loop线程中, 或者正在执行回调操作, 则唤醒loop所在的线程
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

// EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); // 交换, 减少临界区时间
    }

    for (const Functor &functor : functors) {
        functor(); // 执行回调操作
    }
    callingPendingFunctors_ = false;
}