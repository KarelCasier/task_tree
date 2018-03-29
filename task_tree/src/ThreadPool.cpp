#include <task_tree/ThreadPool.hpp>

#include <experimental/optional>
#include <iostream>

namespace task_tree {

using std::experimental::nullopt;
using std::experimental::optional;

/// [[[ ThreadPool::PooledThread ----------------------------------------------

class ThreadPool::PooledThread {
public:
    PooledThread();
    ~PooledThread();

    /// Get the availabilty of the thread.
    bool free() const;

    /// Schedule the thread to stop when it has completed the current task.
    void scheduleStop();

    /// Execute a task.
    /// @param task The task to execute.
    /// @returns True if this thread is accepting a task, false otherwise.
    bool execute(Task&& task);

private:
    void runLoop();

    mutable std::mutex _stateMutex;
    bool _run{true};
    optional<Task> _task;
    std::condition_variable _sleepCV;
    std::thread _thread;
};

ThreadPool::PooledThread::PooledThread()
: _thread{[this]() { runLoop(); }}
{
}

ThreadPool::PooledThread::~PooledThread()
{
    scheduleStop();
    _thread.join();
}

bool ThreadPool::PooledThread::free() const
{
    StateLock lock{_stateMutex};
    return _run && !_task;
}

void ThreadPool::PooledThread::scheduleStop()
{
    {
        StateLock lock{_stateMutex};
        _run = false;
    }
    _sleepCV.notify_one();
}

bool ThreadPool::PooledThread::execute(Task&& task)
{
    {
        StateLock lock{_stateMutex};
        if (_task) {
            return false;
        }
        _task = std::move(task);
    }
    _sleepCV.notify_one();
    return true;
}

void ThreadPool::PooledThread::runLoop()
{
    while (_run) {
        StateLock lock{_stateMutex};
        if (_task) {
            lock.unlock();
            (*_task)();
            lock.lock();
            _task = nullopt;
        } else {
            if (_run) {
                _sleepCV.wait(lock);
            }
        }
    }
}

/// ]]] ThreadPool::PooledThread ----------------------------------------------

/// [[[ ThreadPool ------------------------------------------------------------

ThreadPool::ThreadPool(SizeType size)
: _poolThread([this, size]() {
    setPoolSize(size);
    runLoop();
})
{
}

ThreadPool::~ThreadPool()
{
    {
        StateLock lock{_stateMutex};
        _signalStop = true;
    }

    _sleepCV.notify_one();
    _poolThread.join();

    resize(StateLock{_stateMutex}, 0u);
}

void ThreadPool::setPoolSize(SizeType size)
{
    if (size == 0) {
        throw std::invalid_argument{"'size' must be greater then 0."};
    }
    size = std::min(static_cast<uint32_t>(MAX_POOL_SIZE), std::thread::hardware_concurrency());
    resize(StateLock{_stateMutex}, size);
}

ThreadPool::SizeType ThreadPool::getPoolSize() const
{
    StateLock lock{_stateMutex};
    return _currentSize;
}

void ThreadPool::resize(StateLock, SizeType desiredSize)
{
    if (_currentSize > desiredSize) {
        while (_currentSize != desiredSize) {
            for (auto I = std::begin(_threadPool); I != std::end(_threadPool); ++I) {
                if (I->free()) {
                    I->scheduleStop();
                    if (--_currentSize == desiredSize) {
                        break;
                    }
                }
            }
            std::this_thread::yield();
        }
    } else if (_currentSize < desiredSize) {
        while (_currentSize != desiredSize) {
            _threadPool.emplace_front();
            ++_currentSize;
        }
    }
}

ThreadPool::PooledThread& ThreadPool::waitForFreeThread(StateLock&)
{
    for (;;) {
        for (auto I = std::begin(_threadPool); I != std::end(_threadPool); ++I) {
            if (I->free()) {
                return *I;
            }
        }
        std::this_thread::yield();
    }
}

void ThreadPool::runLoop()
{
    StateLock lock{_stateMutex};
    while (!_signalStop || !queueEmpty(lock)) {
        while (!queueEmpty(lock)) {
            auto& thread = waitForFreeThread(lock);
            auto task = queuePop(lock);
            lock.unlock();
            thread.execute(std::move(task));
            lock.lock();
        }
        if (queueEmpty(lock) && !_signalStop) {
            _sleepCV.wait(lock);
        }
    }
}

bool ThreadPool::queueEmpty(StateLock&) const
{
    return _queuedTasks.empty();
}

const ThreadPool::Task& ThreadPool::queueTop(StateLock&) const
{
    return _queuedTasks.front();
}

ThreadPool::Task ThreadPool::queuePop(StateLock& lock)
{
    auto task = queueTop(lock);
    _queuedTasks.pop();
    return task;
}

/// ]]] ThreadPool ------------------------------------------------------------

} // namespace task_tree
