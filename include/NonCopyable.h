#pragma once

/** 不可复制类 */

class NonCopyable {
    public:
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;
};