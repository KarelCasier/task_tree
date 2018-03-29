#pragma once

#include <chrono>
#include <memory>
#include <queue>

#include "ThreadPool.hpp"

namespace task_tree {

/// Class that schedules self contained tasks to execute on a thread pool at a
/// given time.
class TaskScheduler {
public:
    using Task = std::function<void()>;

    TaskScheduler(std::shared_ptr<ThreadPool> threadPool);
    ~TaskScheduler();

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

    void runLoop();
    void scheduleAndNotifiy(ScheduledTask&& task);

    bool queueEmpty(StateLock&) const;
    const ScheduledTask& queueTop(StateLock&) const;
    ScheduledTask queuePop(StateLock&);

    std::shared_ptr<ThreadPool> _threadPool;
    std::thread _schedulerThread;
    bool _signalStop{true};
    std::priority_queue<ScheduledTask> _scheduledTasks;
    std::condition_variable _sleepCV;
    mutable std::mutex _taskMutex;
    mutable std::mutex _stateMutex;
};

} // namespace task_tree
