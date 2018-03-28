#pragma once

#include <future>
#include <chrono>

#include "Thread.hpp"

namespace task_tree {

class ThreadFuture {
public:
    /// Wait for a thread to become available indefinetely.
    void wait() const;

    /// Wait for a thread to become available for duration.
    template <class Rep, class Period>
    void waitFor(std::chrono::duration<Rep, Period> duration) const;

    /// Wait for a thread to become available until timePoint.
    template <class Clock, class Duration>
    void waitUntil(std::chrono::time_point<Clock, Duration> timePoint) const;

    Thread& get();

private:
    friend class ThreadPool;

    ThreadFuture(std::future<Thread>&& threadFuture);

    std::future<Thread> _threadFuture;
};

}; // namespace thread_tree
