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
    bool available() const;

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
    Thread _thread;
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

bool ThreadPool::PooledThread::available() const
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
            _sleepCV.wait(lock);
        }
    }
}

/// ]]] ThreadPool::PooledThread ----------------------------------------------

/// [[[ ThreadPool ------------------------------------------------------------

ThreadPool::ThreadPool(SizeType size)
: _poolThread([this]() { runLoop(); })
{
    setPoolSize(size);
}

ThreadPool::~ThreadPool()
{
    {
        StateLock lock{_stateMutex};
        _running = false;
    }
    _sleepCV.notify_one();
    _poolThread.join();
}

void ThreadPool::setPoolSize(SizeType size)
{
    if (size == 0) {
        throw std::invalid_argument{"'size' must be greater then 0."};
    }
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
                if (I->available()) {
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

bool ThreadPool::queueTask(Task&& task)
{
    {
        StateLock lock{_stateMutex};
        if (!_running) {
            return false;
        }
        _queuedTasks.emplace(std::move(task));
    }
    _sleepCV.notify_one();
    return true;
}

void ThreadPool::runLoop()
{
    while (_running) {
        StateLock lock{_stateMutex};
        while (!_queuedTasks.empty() && _currentSize) {
            for (auto I = std::begin(_threadPool); I != std::end(_threadPool); ++I) {
                if (I->available()) {
                    auto task = _queuedTasks.front();
                    _queuedTasks.pop();
                    lock.unlock();
                    I->execute(std::move(task));
                    lock.lock();
                    if (_queuedTasks.empty()) {
                        break;
                    }
                }
            }
            std::this_thread::yield();
        }
        _sleepCV.wait(lock);
    }
}

/// ]]] ThreadPool ------------------------------------------------------------

} // namespace task_tree
