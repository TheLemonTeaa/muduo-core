#include <stdlib.h>

#include "Poller.h"
#include "EpollPoller.h"

Poller* Poller::newDefaultPoller(EventLoop *loop) {
    if (::getenv("MUDUO_USE_POLL")) {
        return nullptr; // TODO: return new PollPoller(loop);
    } else {
        return new EpollPoller(loop);
    }
    
}