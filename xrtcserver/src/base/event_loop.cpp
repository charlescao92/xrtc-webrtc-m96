
#include "base/event_loop.h"

#define TRANS_TO_EV_MASK(mask) \
    (((mask) & EventLoop::READ ? EV_READ : 0) | ((mask) & EventLoop::WRITE ? EV_WRITE : 0))

#define TRANS_FROM_EV_MASK(mask) \
    (((mask) & EV_READ ? EventLoop::READ : 0) | ((mask) & EV_WRITE ? EventLoop::WRITE : 0))

namespace xrtc {

EventLoop::EventLoop(void *owner) :
    owner_(owner),
    loop_(ev_loop_new(EVFLAG_AUTO))
{
}

EventLoop::~EventLoop() {
}

void EventLoop::start() {
    ev_run(loop_);
}

void EventLoop::stop() {
    ev_break(loop_, EVBREAK_ALL);
}

static void generic_io_cb(struct ev_loop */*loop*/, struct ev_io *io, int event) {
    IOWatcher *watcher = (IOWatcher*)(io->data);
    watcher->cb(watcher->el, watcher, io->fd, TRANS_FROM_EV_MASK(event), watcher->data);
}

IOWatcher* EventLoop::create_io_event(io_cb_t cb, void *data) {
    IOWatcher *w = new IOWatcher(this, cb, data);
    ev_init(&(w->io), generic_io_cb);
    return w;
}

void EventLoop::start_io_event(IOWatcher *w, int fd, int mask) {
    struct ev_io *io = &(w->io);
    if (ev_is_active(io)) {  // 判断io是否已经启动
        int active_events = TRANS_FROM_EV_MASK(io->events);
        int events = active_events | mask;
        if (events == active_events) { // 重复操作添加了一样的事件
            return;
        }

        // 给io添加事件
        events = TRANS_TO_EV_MASK(events);
        ev_io_stop(loop_, io);
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);

    } else {
        // 添加事件并且启动io
        int events = TRANS_TO_EV_MASK(mask);
        ev_io_set(io, fd, events);
        ev_io_start(loop_, io);
    }
}

void EventLoop::stop_io_event(IOWatcher *w, int fd, int mask) {
    struct ev_io *io = &(w->io);
    if (ev_is_active(io)) {  // 判断io是否启动
        int active_events = TRANS_FROM_EV_MASK(io->events);
        int events = active_events & ~mask;
        if (events == active_events) {
            return;
        }

        events = TRANS_TO_EV_MASK(events);
        ev_io_stop(loop_, io);

        if (events != EV_NONE) {
            ev_io_set(io, fd, events);
            ev_io_start(loop_, io);    
        }
    }
}

void EventLoop::delete_io_event(IOWatcher *w) {
    struct ev_io *io = &(w->io);
    ev_io_stop(loop_, io);
    delete w;
    w = nullptr;
}

static void generic_timer_cb(struct ev_loop * /*loop*/, struct ev_timer *timer, int /*events*/) {
    TimerWatcher *watcher = (TimerWatcher*)(timer->data);
    watcher->cb(watcher->el, watcher, watcher->data);
}

TimerWatcher* EventLoop::create_timer(time_cb_t cb, void *data, bool need_repeat) {
    TimerWatcher *watcher = new TimerWatcher(this, cb, data, need_repeat);
    ev_init(&(watcher->timer), generic_timer_cb);
    return watcher;
}

void EventLoop::start_timer(TimerWatcher *w, unsigned int usec) {
    struct ev_timer* timer = &(w->timer);
    float sec = float(usec) / 1000000;

    if (!w->need_repeat) {
        ev_timer_stop(loop_, timer);
        ev_timer_set(timer, sec, 0);
        ev_timer_start(loop_, timer);
    } else {
        timer->repeat = sec;
        ev_timer_again(loop_, timer);
    }
}

void EventLoop::stop_timer(TimerWatcher *w) {
    struct ev_timer* timer = &(w->timer);
    ev_timer_stop(loop_, timer);
}

void EventLoop::delete_timer(TimerWatcher *w) {
    stop_timer(w);
    delete w;
    w = nullptr;
}

unsigned long EventLoop::now() {
    return static_cast<unsigned long>(ev_now(loop_) * 1000000);
}

} // namespace xrtc
