#pragma once

#include <cstdint>
#include <forward_list>
#include <queue>

#include "ResultFuture.hpp"
#include "Task.hpp"
#include "Thread.hpp"
#include "ThreadFuture.hpp"

namespace task_tree {

/// Class that manages a pool of threads that can be given.
class ThreadPool {
public:
    using SizeType = uint8_t;
    static constexpr SizeType DEFAULT_POOL_SIZE = 16u;
    static constexpr SizeType MAX_POOL_SIZE = std::numeric_limits<SizeType>::max();

    /// Create a tread pool of size @p size.
    /// @throws invalid_argument if @p size is 0.
    ThreadPool(SizeType size = DEFAULT_POOL_SIZE);
    ~ThreadPool();

    /// Set the size of the thread pool.
    /// @note If @p size is smaller then the current size and threads are
    ///        busy, the size will decrease when tasks are finished.
    /// @throws invalid_argument if @p size is 0.
    /// @param size the new pool size.
    void setPoolSize(SizeType size);

    /// Get the size of the thread pool.
    /// @returns the pool size.
    SizeType getPoolSize() const;

    /// Queue a task to execute.
    /// @param task The task to execute.
    /// @returns True if the task has been queued, false otherwise.
    bool queueTask(Task&& task);

private:
    using StateLock = std::unique_lock<std::mutex>;
    class PooledThread;

    void resize(StateLock lock, SizeType desiredSize);
    void runLoop();

    std::thread _poolThread;
    bool _running{true};
    std::condition_variable _sleepCV;
    mutable std::mutex _stateMutex;
    std::queue<Task> _queuedTasks;
    std::forward_list<PooledThread> _threadPool;
    SizeType _currentSize{0u};
};

} // namespace task_tree
