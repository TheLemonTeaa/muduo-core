#pragma once

#include <functional>
#include <thread>
#include <string>
#include <atomic>
#include <unistd.h>
#include <memory>

#include "NonCopyable.h"

class Thread : NonCopyable {
    public:
        using ThreadFunc = std::function<void()>;

        explicit Thread(ThreadFunc, const std::string &name = std::string());
        ~Thread();

        void start(); // 启动线程
        void join(); // 等待线程退出

        bool started() const { return started_; }
        pid_t tid() const { return tid_; } // 获取线程ID
        const std::string& name() const { return name_; }

        static int numCreated() { return numCreated_.load(); } // 获取创建的线程数
    private:
        void setDefaultName(); // 设置线程默认名称

        bool started_; // 标识线程是否启动
        bool joined_; // 标识线程是否被等待
        std::shared_ptr<std::thread> thread_; // 线程对象
        pid_t tid_; // 线程ID
        ThreadFunc func_; // 线程函数
        std::string name_; // 线程名称

        static std::atomic_int numCreated_; // 创建的线程数
};