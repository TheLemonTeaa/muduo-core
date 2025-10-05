#pragma once
#include "NonCopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>

class EventLoop; // 相当于subreactor

/** 
 * Channel为通道, 封装了sockfd和其感兴趣的event,
 * 还绑定了poller返回的具体事件
**/

class Channel : NonCopyable {
    public:
        using EventCallback = std::function<void()>;
        using ReadEventCallback = std::function<void(Timestamp)>;

        Channel(EventLoop *loop, int fd);
        ~Channel();

        // fd得到Poller通知之后 处理事件 handleEvent在EventLoop::loop()中调用
        void handleEvent(Timestamp receiveTime);

        // 设置回调函数
        void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
        void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
        void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
        void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

        // 防止channel被手动remove掉时还在执行回调操作
        void tie(const std::shared_ptr<void> &);

        int fd() const { return fd_; }
        int events() const { return events_; }
        void set_revents(int revt) { revents_ = revt; }

        // 设置fd相应事件状态
        void enableReading() { events_ |= kReadEvent; update(); }
        void disableReading() { events_ &= ~kReadEvent; update(); }
        void enableWriting() { events_ |= kWriteEvent; update(); }
        void disableWriting() { events_ &= ~kWriteEvent; update(); }
        void disableAll() { events_ = kNoneEvent; update(); }

        // 返回fd当前的事件状态
        bool isNoneEvent() const { return events_ == kNoneEvent; }
        bool isWriting() const { return events_ & kWriteEvent; }
        bool isReading() const { return events_ & kReadEvent; }

        int index() const { return index_; }
        void set_index(int idx) { index_ = idx; }

        EventLoop* ownerLoop() { return loop_; }
        void remove(); // 从Poller中删除
    private:
        
        void update();
        void handleEventWithGuard(Timestamp receiveTime);

        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        EventLoop *loop_; // 事件循环
        const int fd_; // fd, Poller监听的对象
        int events_; // 注册fd感兴趣的事件
        int revents_; // Poller返回的具体发生的事件
        int index_; // used by Poller

        std::weak_ptr<void> tie_;
        bool tied_;

        ReadEventCallback readCallback_;
        EventCallback writeCallback_;
        EventCallback closeCallback_;
        EventCallback errorCallback_;
};