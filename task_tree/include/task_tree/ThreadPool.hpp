#pragma once

#include <cstdint>
#include <forward_list>
#include <queue>
#include <thread>
#include <future>

namespace task_tree {

/// Class that manages a pool of threads that can be given tasks to perform.
/// @note The destructor blocks until all tasks complete.
class ThreadPool {
public:
    using Task = std::function<void()>;
    using SizeType = uint8_t;
    static constexpr SizeType DEFAULT_POOL_SIZE = 8u;
    static constexpr SizeType MAX_POOL_SIZE = std::numeric_limits<SizeType>::max();

    /// Create a tread pool of size @p size.
    /// @note size caps at min(MAX_POOL_SIZE, std::thread::hardware_concurrency()).
    /// @throws invalid_argument if @p size is 0.
    ThreadPool(SizeType size = DEFAULT_POOL_SIZE);
    ~ThreadPool();

    /// Set the size of the thread pool.
    /// @note size caps at min(MAX_POOL_SIZE, std::thread::hardware_concurrency()).
    /// @throws invalid_argument if @p size is 0.
    /// @param size the new pool size.
    void setPoolSize(SizeType size);

    /// Get the size of the thread pool.
    /// @returns the pool size.
    SizeType getPoolSize() const;

    /// Queue a task to execute.
    /// @throws runtime_error If the thread pool is stopped.
    /// @tparam Func the prototype of @p func.
    /// @tparam Args the arguments types to @p func.
    /// @param func The function to execute.
    /// @param args The arguments to call @p func with.
    /// @returns A future to the result of @p func.
    template <typename Func, typename... Args>
    auto queueTask(Func&& func, Args... args) -> std::future<decltype(func(args...))>;

private:
    using StateLock = std::unique_lock<std::mutex>;
    class PooledThread;

    void runLoop();
    void resize(StateLock lock, SizeType desiredSize);
    PooledThread& waitForFreeThread(StateLock& lock);

    bool queueEmpty(StateLock&) const;
    const Task& queueTop(StateLock&) const;
    Task queuePop(StateLock&);

    std::thread _poolThread;

    bool _signalStop{false};
    std::condition_variable _sleepCV;

    mutable std::mutex _stateMutex;
    std::queue<Task> _queuedTasks;
    std::forward_list<PooledThread> _threadPool;
    SizeType _currentSize{0u};
};

template <typename Func, typename... Args>
auto ThreadPool::queueTask(Func&& func, Args... args) -> std::future<decltype(func(args...))>
{
    {
        StateLock lock{_stateMutex};
        if (_signalStop) {
            throw std::runtime_error{"Can not queue task on stopped ThreadPool"};
        }
    }
    auto wrappedFunc = [func{std::move(func)}, args{std::make_tuple(std::forward<Args>(args)...)}]() {
        return std::apply(std::move(func), std::move(args));
    };
    using FuncRetT = decltype(func(args...));
    auto task = std::make_shared<std::packaged_task<FuncRetT()>>(std::move(wrappedFunc));
    auto taskFuture = task->get_future();
    {
        StateLock lock{_stateMutex};
        _queuedTasks.emplace([task{std::move(task)}]() mutable { (*task)(); });
    }
    _sleepCV.notify_one();
    return taskFuture;
}

} // namespace task_tree
