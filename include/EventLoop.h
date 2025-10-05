#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

#include "NonCopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Poller;
class Channel;

class EventLoop : NonCopyable {
    public:
        using Functor = std::function<void()>;

        EventLoop();
        ~EventLoop();

        void loop(); // 开始事件循环
        void quit(); // 退出事件循环

        Timestamp pollReturnTime() const { return pollReturnTime_; }

        // 在当前loop中执行cb
        void runInLoop(Functor cb);
        // 把cb放入队列, 唤醒loop所在的线程, 执行cb
        void queueInLoop(Functor cb);

        // 通过wakeupFd_唤醒loop
        void wakeup();

        // EventLoop的方法 => Poller的方法
        void updateChannel(Channel *channel);
        void removeChannel(Channel *channel);
        bool hasChannel(Channel *channel);

        // 判断EventLoop对象是否在自己的线程里面
        bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

    private:
        void handleRead(); // wakeupfd有数据可读时, 处理函数
        void doPendingFunctors(); // 执行回调函数

        using ChannelList = std::vector<Channel *>;

        std::atomic<bool> looping_; // 原子操作, 标识当前loop是否在运行
        std::atomic<bool> quit_; // 标识退出loop循环

        const pid_t threadId_; // 记录当前loop所属的线程id

        Timestamp pollReturnTime_; // poller返回发生事件的channels的时间点
        std::unique_ptr<Poller> poller_; // IO复用的核心对象

        int wakeupFd_; // mainLoop通过该文件描述符唤醒subReactor(loop)
        std::unique_ptr<Channel> wakeupChannel_; // 专门负责监听wakeupFd_可读事件的channel

        ChannelList activeChannels_; // poller返回的活跃的channel列表

        std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
        std::vector<Functor> pendingFunctors_; // 存储loop需要执行的所有回调操作
        std::mutex mutex_; // 互斥锁, 用于保护pendingFunctors_线程安全
};