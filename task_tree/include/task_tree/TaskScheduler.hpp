#pragma once

#include <chrono>
#include <memory>
#include <queue>

#include "ThreadPool.hpp"

namespace task_tree {

/// Class that schedules tasks to execute on a thread pool.
class TaskScheduler {
public:
    TaskScheduler(std::shared_ptr<ThreadPool> threadPool);
    ~TaskScheduler();

    /// Get the running status.
    bool running() const;

    /// Schedule a task to execute now.
    /// @param task The task to schedule.
    void scheduleNow(Task&& task);

    /// Schedule a task to execute in @p delay time.
    /// @param task The task to schedule.
    /// @param delay The delay before executing the task.
    void scheduleIn(Task&& task, std::chrono::milliseconds delay);

    /// Schedule a task to execute every @p delay period.
    /// @param task The task to schedule.
    /// @param delay The delay between executing the task.
    void scheduleEvery(Task&& task, std::chrono::milliseconds delay);

    /// Schedule a task to execute at @p timePoint
    /// @param task The task to schedule.
    /// @param timePoint The exact time to execute the task.
    void scheduleAt(Task&& task, std::chrono::steady_clock::time_point timePoint);

private:
    using StateLock = std::unique_lock<std::mutex>;
    class ScheduledTask;

    void scheduleAndNotifiy(ScheduledTask&& task);
    void runLoop();

    std::shared_ptr<ThreadPool> _threadPool;
    std::thread _schedulerThread;
    bool _running{true};
    std::priority_queue<ScheduledTask> _scheduledTasks;
    std::condition_variable _sleepCV;
    mutable std::mutex _taskMutex;
    mutable std::mutex _stateMutex;
};

} // namespace task_tree
