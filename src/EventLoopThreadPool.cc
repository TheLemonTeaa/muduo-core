#include <memory>

#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "Logger.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0) {
}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 不需要delete EventLoopThread, 因为threads_是unique_ptr
    // 不需要delete EventLoop, 因为EventLoopThread的线程函数中会自动delete
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.emplace_back(t);
        EventLoop *loop = t->startLoop(); // 启动线程，创建EventLoop
        loops_.push_back(loop);
    }

    if (numThreads_ == 0 && cb) {
        cb(baseLoop_); // 如果没有subloop, 则在mainLoop中执行回调
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    EventLoop *loop = baseLoop_;

    if (!loops_.empty()) {
        // 轮询算法选择一个subloop
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if (loops_.empty()) {
        return std::vector<EventLoop *>(1, baseLoop_);
    } else {
        return loops_;
    }
}