#include <task_tree/TaskScheduler.hpp>

#include <cassert>
#include <experimental/optional>
#include <iostream>

namespace task_tree {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
using std::chrono::milliseconds;
using std::experimental::nullopt;
using std::experimental::optional;

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
    optional<Task> _task;
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
    auto returnTask = optional<Task>{nullopt};
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
        _signalStop = false;
    }
    _sleepCV.notify_one();
    _schedulerThread.join();
}

void TaskScheduler::scheduleNow(Task&& task)
{
    scheduleAndNotifiy({std::move(task), Clock::now()});
}

void TaskScheduler::scheduleIn(Task&& task, milliseconds delay)
{
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
    while (_signalStop) {
        if (!queueEmpty(lock)) {
            const auto& nextTask = queueTop(lock);
            if (nextTask.scheduledAt() <= Clock::now()) {
                auto task = queuePop(lock);
                lock.unlock();
                _threadPool->queueTask(task.get());
                lock.lock();
            }
            if (!_scheduledTasks.empty() && !_signalStop) {
                // Sleep until the next task is scheduled to execute or another is added.
                _sleepCV.wait_until(lock, _scheduledTasks.top().scheduledAt());
            }
        } else {
            // Sleep until a task is added.
            _sleepCV.wait(lock);
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
