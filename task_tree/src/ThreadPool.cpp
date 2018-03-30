#include <task_tree/ThreadPool.hpp>
#include <task_tree/ScopedSuspendLock.hpp>

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

    /// Signal the thread to stop when it has completed the current task.
    void signalStop();

    /// Execute a task.
    /// @param task The task to execute.
    /// @returns True if this thread is accepting a task, false otherwise.
    bool execute(Task&& task);

private:
    void runLoop();

    mutable std::mutex _stateMutex;
    bool _sigStop{false};
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
    signalStop();
    /// Don't wait for thread to finish
    _thread.detach();
}

bool ThreadPool::PooledThread::free() const
{
    StateLock lock{_stateMutex};
    return !_sigStop && !_task;
}

void ThreadPool::PooledThread::signalStop()
{
    {
        StateLock lock{_stateMutex};
        _sigStop = true;
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
    while (!_sigStop) {
        StateLock lock{_stateMutex};
        if (_task) {
            ScopedSuspendLock suspendLock{lock};
            (*_task)();
        }
        _task = nullopt;
        _sleepCV.wait(lock, [this]() { return _sigStop || _task; });
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

ThreadPool::ThreadPool()
: ThreadPool(std::thread::hardware_concurrency())
{
}

ThreadPool::~ThreadPool()
{
    {
        StateLock lock{_stateMutex};
        _sigStop = true;
    }

    _sleepCV.notify_one();
    _poolThread.join();
}

void ThreadPool::setPoolSize(SizeType size)
{
    if (size == 0) {
        throw std::runtime_error{"Can not set pool size to 0"};
    }
    const auto maxSize =
        std::min(static_cast<uint32_t>(std::numeric_limits<SizeType>::max()), std::thread::hardware_concurrency());
    if (size > maxSize) {
        std::cout << "Capping thread pool size to " << maxSize << std::endl;
        size = maxSize;
    }
    StateLock lock{_stateMutex};
    _targetSize = size;
}

ThreadPool::SizeType ThreadPool::getPoolSize() const
{
    StateLock lock{_stateMutex};
    return poolSize(lock);
}

ThreadPool::SizeType ThreadPool::poolSize(StateLock&) const
{
    return _threadPool.size();
}

void ThreadPool::resizePool(StateLock& lock, SizeType desiredSize)
{
    if (poolSize(lock) > desiredSize) {
        while (poolSize(lock) != desiredSize) {
            auto I = waitForFreeThread(lock);
            I->signalStop();
            I = _threadPool.erase(I);
            if (poolSize(lock) == desiredSize) {
                break;
            }
        }
    } else if (poolSize(lock) < desiredSize) {
        while (poolSize(lock) != desiredSize) {
            _threadPool.emplace_front();
        }
    }
}

auto ThreadPool::waitForFreeThread(StateLock&) -> std::list<PooledThread>::iterator
{
    for (;;) {
        for (auto I = std::begin(_threadPool); I != std::end(_threadPool); ++I) {
            if (I->free()) {
                return I;
            }
        }
        std::this_thread::yield();
    }
}

void ThreadPool::runLoop()
{
    StateLock lock{_stateMutex};
    while (!_sigStop || !queueEmpty(lock)) {
        if (poolSize(lock) != _targetSize) {
            resizePool(lock, _targetSize);
        }
        while (!queueEmpty(lock)) {
            auto thread = waitForFreeThread(lock);
            auto task = queuePop(lock);
            {
                ScopedSuspendLock suspendLock{lock};
                thread->execute(std::move(task));
            }
        }
        if (queueEmpty(lock) && !_sigStop) {
            _sleepCV.wait(lock);
        }
    }
    resizePool(lock, 0u);
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
