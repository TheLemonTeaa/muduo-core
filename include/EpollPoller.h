#pragma once

#include <vector>
#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

#include "Poller.h"
#include "Timestamp.h"

/** 
 * epoll是linux下高效的IO多路复用技术
 * 使用epoll_create创建epoll实例
 * 使用epoll_ctl向epoll实例中添加/修改/删除感兴趣的IO事件
 * 使用epoll_wait等待感兴趣的IO事件发生
*/

class Channel;

class EpollPoller : public Poller{
    public:
        EpollPoller(EventLoop *loop);
        ~EpollPoller() override;
    
        // 重写基类的虚函数
        Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
        void updateChannel(Channel *channel) override;
        void removeChannel(Channel *channel) override;
    private:
        static const int kInitEventListSize = 16; // 初始监听事件的大小

        // 填写活跃的连接
        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
        // 更新channel在epoll中的状态 本质是epoll_ctl
        void update(int operation, Channel *channel);

#ifdef __linux__
        using EventList = std::vector<struct epoll_event>;
#elif __APPLE__
        using EventList = std::vector<struct kevent>;
#endif

        int epollfd_; // epoll_create返回的文件描述符
        EventList events_; // epoll_wait返回的活跃事件列表
};