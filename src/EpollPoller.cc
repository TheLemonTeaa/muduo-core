#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

const int kNew = -1; // channel未添加到epoll中
const int kAdded = 1; // channel已添加到epoll中
const int kDeleted = 2; // channel已从epoll中删除

EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop)
#ifdef __linux__
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
#elif __APPLE__
    , epollfd_(::kqueue())
#endif
    , events_(kInitEventListSize) // vector<epoll_event>(16) 
    {
        if (epollfd_ < 0) {
            LOG_FATAL("epoll_create error:%d\n", errno);
        }
    }

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    // epoll_wait返回活跃事件的数量
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());
#ifdef __linux__
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
#elif __APPLE__
    struct timespec ts;
    ts.tv_sec = timeoutMs / 1000;
    ts.tv_nsec = (timeoutMs % 1000) * 1000000;
    int numEvents = ::kevent(epollfd_, nullptr, 0, &*events_.begin(), static_cast<int>(events_.size()), &ts);
#endif
    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()) {
            events_.resize(events_.size() * 2); // 扩容
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    } else {
        if (savedErrno != EINTR) {
            errno = savedErrno;
            LOG_ERROR("EpollPoller::poll() err:%d\n", errno);
        }
    }
    return now;
}

// Channel update remove最终调用的都是以下函数
void EpollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    if(index == kNew || index == kDeleted) {
        // 新的channel或者之前被删除的channel
        if(index == kNew) {
            int fd = channel->fd();
            // 添加新的channel
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        #ifdef __linux__
        update(EPOLL_CTL_ADD, channel); // epoll_ctl添加监听事件
        #elif __APPLE__
        update(EV_ADD, channel); // kqueue添加监听事件
        #endif
    } else {
        // 已经添加到epoll中的channel, 更新操作
        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            // 如果该channel不再感兴趣任何事件, 则将其从epoll中删除
            #ifdef __linux__
            update(EPOLL_CTL_DEL, channel);
            #elif __APPLE__
            update(EV_DELETE, channel);
            #endif
            channel->set_index(kDeleted);
        } else {
            // 否则更新其感兴趣的事件
            #ifdef __linux__
            update(EPOLL_CTL_MOD, channel);
            #elif __APPLE__
            update(EV_ADD | EV_ENABLE, channel);
            #endif
        }
    }
}

// 从Poller中删除channel
void EpollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);
    channels_.erase(fd);

    int index = channel->index();
    if (index == kAdded) {
        // 从epoll中删除
        #ifdef __linux__
        update(EPOLL_CTL_DEL, channel);
        #elif __APPLE__
        update(EV_DELETE, channel);
        #endif
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
#ifdef __linux__
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
#elif __APPLE__
        Channel *channel = static_cast<Channel *>(reinterpret_cast<void *>(events_[i].udata));
        channel->set_revents(events_[i].filter);
#endif
        activeChannels->push_back(channel);// EventLoop拿到活跃的channel列表
    }
}

// 更新channel在epoll中的状态 本质是epoll_ctl
void EpollPoller::update(int operation, Channel *channel) {
#ifdef __linux__
    struct epoll_event event;
    ::memset(&event, 0, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl DEL error:%d\n", errno);
        } else {
            LOG_FATAL("epoll_ctl ADD/MOD error:%d\n", errno);
        }
    }
#elif __APPLE__
    struct kevent ke;
    int fd = channel->fd();
    int16_t filter = channel->events();
    EV_SET(&ke, fd, filter, operation, 0, 0, reinterpret_cast<void *>(channel));
    if (::kevent(epollfd_, &ke, 1, nullptr, 0, nullptr) < 0) {
        if (operation == EV_DELETE) {
            LOG_ERROR("kevent DELETE error:%d\n", errno);
        } else {
            LOG_FATAL("kevent ADD/MOD error:%d\n", errno);
        }
    }
#endif
}
