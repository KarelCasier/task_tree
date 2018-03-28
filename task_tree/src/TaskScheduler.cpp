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

Task TaskScheduler::ScheduledTask::get()
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
        _running = false;
    }
    _sleepCV.notify_one();
    _schedulerThread.join();
}

bool TaskScheduler::running() const
{
    StateLock lock{_stateMutex};
    return _running;
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
    while (_running) {
        StateLock lock{_taskMutex};
        if (!_scheduledTasks.empty()) {
            auto nextTask = _scheduledTasks.top();
            if (nextTask.scheduledAt() <= Clock::now()) {
                _scheduledTasks.pop();
                lock.unlock();
                _threadPool->queueTask(nextTask.get());
                lock.lock();
            }
            if (!_scheduledTasks.empty()) {
                // Sleep until the next task is scheduled to execute or another is added.
                _sleepCV.wait_until(lock, _scheduledTasks.top().scheduledAt());
            }
        } else {
            // Sleep until a task is added.
            _sleepCV.wait(lock);
        }
    }
}

/// ]]] TaskScheduler ---------------------------------------------------------

} // namespace task_tree
