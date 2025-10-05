#pragma once

#include <vector>
#include <unordered_map>

#include "NonCopyable.h"
#include "Timestamp.h"

class Channel;
class EventLoop;

/**
 * muduo库中多路事件分发器的核心I/O复用模块
 * 封装了poll/select/epoll等IO多路复用技术的底层细节
 * 主要负责监听多个文件描述符的事件, 并在事件发生时通知相应的Channel去处理
**/
class Poller : NonCopyable {
    public:
        using ChannelList = std::vector<Channel *>;

        Poller(EventLoop *loop);
        virtual ~Poller() = default;

        // 给所有IO复用保留统一的接口
        virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
        virtual void updateChannel(Channel *channel) = 0;
        virtual void removeChannel(Channel *channel) = 0;

        // 判断参数channel是否在当前Poller中
        virtual bool hasChannel(Channel *channel) const;

        // EventLoop可以通过该接口获取默认的IO复用对象
        static Poller* newDefaultPoller(EventLoop *loop);
    protected:
        using ChannelMap = std::unordered_map<int, Channel *>;
        ChannelMap channels_; // 管理所有的channel, key是fd, value是channel
    private:
        EventLoop *ownerLoop_; // Poller所属的EventLoop
};