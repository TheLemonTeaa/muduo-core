#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread {

    // 记录线程的ID, 避免每次调用tid()时都进行系统调用
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid() {
        if (__builtin_expect(t_cachedTid == 0, 0)) { // __builtin_expect用于分支预测, 这里表示t_cachedTid通常不为0
            cacheTid();
        }
        return t_cachedTid;
    }
}