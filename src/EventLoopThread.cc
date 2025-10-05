#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb) {
}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_) {
        loop_->quit();
    }
    thread_.join();
}

EventLoop* EventLoopThread::startLoop() {
    thread_.start();

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return loop_ != nullptr; });
        loop = loop_;
    }
    return loop;
}

// 下面的函数在单独的新线程中运行
void EventLoopThread::threadFunc() {
    EventLoop loop; // 创建EventLoop对象, 和上面的线程一一对应, 即 one loop per thread

    if (callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    
    loop.loop(); // 开启事件循环
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
