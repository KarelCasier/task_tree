#pragma once

#include <mutex>

namespace task_tree {

/// Helper class to suspend a unique_lock while in scope.
class ScopedSuspendLock {
public:
    using StateLock = std::unique_lock<std::mutex>;
    ScopedSuspendLock(StateLock& lock);
    ~ScopedSuspendLock();

private:
    StateLock& _lock;
};

inline ScopedSuspendLock::ScopedSuspendLock(StateLock& lock)
: _lock{lock}
{
    _lock.unlock();
}

inline ScopedSuspendLock::~ScopedSuspendLock()
{
    _lock.lock();
}

} // namespace task_tree
