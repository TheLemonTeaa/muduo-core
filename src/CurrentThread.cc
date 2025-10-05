#include "CurrentThread.h"

namespace CurrentThread {
    __thread int t_cachedTid = 0; // 初始化线程局部存储变量

    void cacheTid() {
        if (t_cachedTid == 0) {
            // 通过系统调用获取线程ID并缓存
            // syscall是一个低级接口, 直接与内核交互, SYS_gettid是获取线程ID的系统调用号
            t_cachedTid = static_cast<int>(::syscall(SYS_gettid));
        }
    }
}