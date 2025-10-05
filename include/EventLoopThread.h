#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "NonCopyable.h"
#include "Thread.h"

class EventLoop;

class EventLoopThread : NonCopyable {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const std::string &name = std::string());
        ~EventLoopThread();

        EventLoop* startLoop(); // 启动线程，创建EventLoop

    private:
        void threadFunc(); // 线程函数

        EventLoop *loop_; // 线程创建的EventLoop对象
        bool exiting_; // 标识loop是否退出
        Thread thread_; // 线程对象
        std::mutex mutex_;
        std::condition_variable cond_;
        ThreadInitCallback callback_; // loop创建后，回调该函数
};