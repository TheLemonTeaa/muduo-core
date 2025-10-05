#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <string>

#include "NonCopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : NonCopyable {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
        ~EventLoopThreadPool();

        void setThreadNum(int numThreads) { numThreads_ = numThreads; } // 设置底层subloop的个数
        void start(const ThreadInitCallback &cb = ThreadInitCallback()); // 启动线程池

        EventLoop* getNextLoop(); // 通过轮询算法选择一个subloop
        std::vector<EventLoop*> getAllLoops(); // 获取所有的subloop

        bool started() const { return started_; }
        const std::string& name() const { return name_; }

    private:
        EventLoop *baseLoop_; // 用户传入的baseloop，mainLoop
        std::string name_; // 线程池名称

        bool started_; // 标识线程池是否启动
        int numThreads_; // 线程池中subloop的个数
        int next_; // 轮询算法，记录下一个被选中的subloop下标
        std::vector<std::unique_ptr<EventLoopThread>> threads_; // 线程池
        std::vector<EventLoop*> loops_; // 每个线程里面的subloop
};