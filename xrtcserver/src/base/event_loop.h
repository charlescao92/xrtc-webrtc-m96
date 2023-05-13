/**
 * @file event_loop.h
 * @author charles
 * @brief 
*/

#ifndef __BASE_EVENT_LOOP_H_
#define __BASE_EVENT_LOOP_H_

#include <libev/ev.h>

namespace xrtc {

class EventLoop;
class IOWatcher;
class TimerWatcher;

typedef void(*io_cb_t)(EventLoop *el, IOWatcher *w, int fd, int event, void *data);
typedef void(*time_cb_t)(EventLoop *el, TimerWatcher *w, void *data);

class EventLoop {
public:
    enum {
        READ = 0x1,
        WRITE = 0x2
    };

    EventLoop(void *owner);
    ~EventLoop();

public:
    void start();
    void stop();
    void *owner() { return owner_; }
    unsigned long now();

    IOWatcher* create_io_event(io_cb_t cb, void *data);
    void start_io_event(IOWatcher *w, int fd, int mask);
    void stop_io_event(IOWatcher *w, int fd, int mask);
    void delete_io_event(IOWatcher *w);

    TimerWatcher* create_timer(time_cb_t cb, void *data, bool need_repeat = true);
    void start_timer(TimerWatcher *w, unsigned int usec);
    void stop_timer(TimerWatcher *w);
    void delete_timer(TimerWatcher *w);

private:
    void *owner_;
    struct ev_loop *loop_;
    
};

class IOWatcher {
public:
    IOWatcher(EventLoop *el, io_cb_t cb, void *data) :
        el(el),
        cb(cb),
        data(data) {
        io.data = this;
    }

public:
    EventLoop *el;
    ev_io io;
    io_cb_t cb;
    void *data;
};

class TimerWatcher {
public:
    TimerWatcher(EventLoop *el, time_cb_t cb, void *data, bool need_repeat) :
        el(el),
        cb(cb),
        data(data),
        need_repeat(need_repeat) {
        timer.data = this;
    }

public:
    EventLoop *el;
    struct ev_timer timer;
    time_cb_t cb;
    void *data;
    bool need_repeat = false;
};

} // namespace xrtc

#endif // __BASE_EVENT_LOOP_H_
