#pragma once

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <utility>
#include <stdexcept>

static const int MAX_EVENTS = 1024;

class EventLoop {
public:
    using Callback = std::function<void()>;
    using TimePoint = std::chrono::steady_clock::time_point;
    using TimerId = uint64_t;

    EventLoop()
        : running_(false), stopRequested_(false) {
        epollFd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epollFd_ == -1) {
            throw std::runtime_error("epoll_create1 failed");
        }
        wakeFd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (wakeFd_ == -1) {
            close(epollFd_);
            throw std::runtime_error("eventfd failed");
        } 
        
        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = wakeFd_;
        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, wakeFd_, &ev) == -1) {
            close(wakeFd_);
            close(epollFd_);
            throw std::runtime_error("epoll_ctl add wakeFd failed");
        }   
    }

    ~EventLoop() {
        stop();
        if (wakeFd_ != -1) close(wakeFd_);
        if (epollFd_ != -1) close(epollFd_);
    }

    void loop() {
        if (running_.exchange(true)) {
            return; 
        }

        std::vector<struct epoll_event> events(MAX_EVENTS);
        stopRequested_ = false;

        while (!stopRequested_) {
            int timeout = -1; // 默认 -1 代表无事件时永久阻塞休眠
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!timers_.empty()) {
                    auto now = std::chrono::steady_clock::now();
                    auto& first = timers_.begin()->second;
                    if (first.expireTime <= now) {
                        timeout = 0; //已经超时了，要求 epoll_wait 立即返回不阻塞
                    } 
                    else {
                        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                            first.expireTime - now);
                        timeout = static_cast<int>(diff.count());
                        if (timeout < 0) timeout = 0;
                    }
                }
            }

            int nfds = epoll_wait(epollFd_, events.data(), MAX_EVENTS, timeout);
            if (nfds == -1) {
                if (errno == EINTR) continue; //被信号中断了，不是一个真正的错误
                break;
            }

            // 1. 收集待执行的 I/O 回调（解耦锁与回调）
            std::vector<Callback> pendingCallbacks;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (int i = 0; i < nfds; ++i) {
                    int fd = events[i].data.fd;
                    if (fd == wakeFd_) {
                        uint64_t val;
                        read(wakeFd_, &val, sizeof(val));
                        continue;
                    }
                    auto it = fdEvents_.find(fd);
                    if (it != fdEvents_.end() && it->second.callback) {
                        pendingCallbacks.push_back(it->second.callback);
                    }
                }
            }

            // 2. 无锁执行 I/O 回调，防止死锁
            for (const auto& cb : pendingCallbacks) {
                cb();
            }

            // 3. 处理定时器
            processTimers();
        }

        running_ = false;
    }

    void stop() {
        stopRequested_ = true;
        wakeup();
    }

    void wakeup() {
        uint64_t val = 1;
        write(wakeFd_, &val, sizeof(val));
    }

    void addFd(int fd, uint32_t events, Callback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        fdEvents_[fd] = {events, std::move(cb)};
        updateEpollLocked(fd, events, EPOLL_CTL_ADD);
        wakeup();
    }

    void modifyFd(int fd, uint32_t events, Callback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = fdEvents_.find(fd);
        if (it == fdEvents_.end()) {
            fdEvents_[fd] = {events, std::move(cb)};
            updateEpollLocked(fd, events, EPOLL_CTL_ADD);
        } else {
            it->second.events = events;
            it->second.callback = std::move(cb);
            updateEpollLocked(fd, events, EPOLL_CTL_MOD);
        }
        wakeup();
    }

    void removeFd(int fd) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = fdEvents_.find(fd);
        if (it != fdEvents_.end()) {
            updateEpollLocked(fd, 0, EPOLL_CTL_DEL);
            fdEvents_.erase(it);
            wakeup();
        }
    }

    TimerId addTimer(int64_t milliseconds, Callback cb) {
        auto now = std::chrono::steady_clock::now();
        auto expire = now + std::chrono::milliseconds(milliseconds);
        
        std::lock_guard<std::mutex> lock(mutex_);
        TimerId id = ++nextTimerId_;
        TimerEvent te{expire, std::move(cb), id, false};
        timers_.emplace(expire, std::move(te));
        wakeup();
        return id;
    }

    void cancelTimer(TimerId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& pair : timers_) {
            if (pair.second.id == id) {
                pair.second.cancelled = true;
                break;
            }
        }
    }

private:
    struct FdEvent {
        uint32_t events;    // 监听的 epoll 事件类型（例如 EPOLLIN 读事件, EPOLLOUT 写事件）
        Callback callback;  // 当该 fd 上的事件就绪时，要执行的 C++ 回调函数
    };

    struct TimerEvent {
        TimePoint expireTime;  // 到期时间点（使用的是 std::chrono::steady_clock 的时间）
        Callback callback;     // 时间到了之后要执行的 C++ 回调函数
        TimerId id;            // 该定时器的唯一身份证（用来取消定时器）
        bool cancelled = false; // 取消标记（惰性删除：取消时先改标志位，触发时再真正删除）
    };

    int epollFd_ = -1;
    int wakeFd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    std::mutex mutex_;
    std::unordered_map<int, FdEvent> fdEvents_;
    std::multimap<TimePoint, TimerEvent> timers_;
    TimerId nextTimerId_ = 0;

    void updateEpollLocked(int fd, uint32_t events, int op) {
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epollFd_, op, fd, &ev);
    }

    void processTimers() {
        auto now = std::chrono::steady_clock::now();
        std::vector<Callback> expiredCallbacks;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = timers_.begin();
            while (it != timers_.end()) {
                if (it->second.cancelled) {
                    it = timers_.erase(it);
                    continue;
                }
                if (it->second.expireTime > now) {
                    break; 
                }
                if (it->second.callback) {
                    expiredCallbacks.push_back(it->second.callback);
                }
                it = timers_.erase(it);
            }
        }

        // 无锁执行定时器回调
        for (const auto& cb : expiredCallbacks) {
            cb();
        }
    }
};