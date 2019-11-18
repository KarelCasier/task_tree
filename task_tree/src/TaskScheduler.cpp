#include <task_tree/TaskScheduler.hpp>
#include <task_tree/ScopedSuspendLock.hpp>

#include <cassert>
#include <optional>
#include <iostream>

namespace task_tree {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using std::chrono::milliseconds;

/// [[[ TaskScheduler::ScheduledTask ------------------------------------------

class TaskScheduler::ScheduledTask {
public:
    ScheduledTask(Task&& task, Clock::time_point scheduledAt);

    /// Get the task.
    /// @throws logic_error on subsequent calls.
    /// @returns the wrapped task.
    Task get();

    /// Get the time the task is scheduled to execute at.
    const Clock::time_point& scheduledAt() const;

    /// Order the tasks by lowest in the priority queue (note the flpped sign).
    bool operator<(const ScheduledTask& other) const { return _scheduledAt > other._scheduledAt; }

private:
    std::optional<Task> _task;
    Clock::time_point _scheduledAt;
};

TaskScheduler::ScheduledTask::ScheduledTask(Task&& task, Clock::time_point scheduledAt)
: _task{std::move(task)}
, _scheduledAt{std::move(scheduledAt)}
{
}

TaskScheduler::Task TaskScheduler::ScheduledTask::get()
{
    if (!_task) {
        throw std::logic_error{"ScheduledTask::get should only be called once."};
    }
    auto returnTask = std::optional<Task>{std::nullopt};
    _task.swap(returnTask);
    return std::move(*returnTask);
}

const Clock::time_point& TaskScheduler::ScheduledTask::scheduledAt() const
{
    return _scheduledAt;
}

/// ]]] TaskScheduler::ScheduledTask ------------------------------------------

/// [[[ TaskScheduler ---------------------------------------------------------

TaskScheduler::TaskScheduler(std::shared_ptr<ThreadPool> threadPool)
: _threadPool{std::move(threadPool)}
, _schedulerThread{[this] { runLoop(); }}
{
}

TaskScheduler::~TaskScheduler()
{
    {
        StateLock lock{_stateMutex};
        _sigStop = true;
    }
    _sleepCV.notify_one();
    /// Don't wait for thread to finish
    _schedulerThread.detach();
}

void TaskScheduler::scheduleNow(Task&& task)
{
    scheduleAndNotifiy({std::move(task), Clock::now()});
}

void TaskScheduler::scheduleIn(Task&& task, milliseconds delay)
{
    assert(delay > 0ms);
    scheduleAndNotifiy({std::move(task), Clock::now() + delay});
}

void TaskScheduler::scheduleEvery(Task&& task, milliseconds delay)
{
    assert(delay > 0ms);
    auto repeatTask = Task{[this, delay, task{std::move(task)}]() mutable {
        task();
        scheduleEvery(std::move(task), delay);
    }};
    scheduleAndNotifiy({std::move(repeatTask), Clock::now() + delay});
}

void TaskScheduler::scheduleAt(Task&& task, Clock::time_point timePoint)
{
    assert(timePoint > Clock::now());
    scheduleAndNotifiy({std::move(task), timePoint});
}

void TaskScheduler::scheduleAndNotifiy(ScheduledTask&& task)
{
    {
        StateLock lock{_taskMutex};
        _scheduledTasks.emplace(std::move(task));
    }
    _sleepCV.notify_one();
}

void TaskScheduler::runLoop()
{
    StateLock lock{_taskMutex};
    while (!_sigStop) {
        if (!queueEmpty(lock)) {
            const auto& nextTask = queueTop(lock);
            if (nextTask.scheduledAt() <= Clock::now()) {
                auto task = queuePop(lock);
                ScopedSuspendLock suspendLock{lock};
                _threadPool->queueTask(task.get());
            } else {
                // Sleep until the next task is scheduled to execute, another is added, or signaled to stop.
                _sleepCV.wait_until(lock, nextTask.scheduledAt());
            }
        } else {
            // Sleep until a task is added or signaled to stop.
            _sleepCV.wait(lock, [this, &lock]() { return _sigStop || !queueEmpty(lock); });
        }
    }
}

bool TaskScheduler::queueEmpty(StateLock&) const
{
    return _scheduledTasks.empty();
}

const TaskScheduler::ScheduledTask& TaskScheduler::queueTop(StateLock&) const
{
    return _scheduledTasks.top();
}

TaskScheduler::ScheduledTask TaskScheduler::queuePop(StateLock& lock)
{
    auto task = queueTop(lock);
    _scheduledTasks.pop();
    return task;
}

/// ]]] TaskScheduler ---------------------------------------------------------

} // namespace task_tree
