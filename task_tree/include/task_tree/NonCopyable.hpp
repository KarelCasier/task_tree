#pragma once

namespace task_tree {

class NonCopyable {
public:
    virtual ~NonCopyable() = default;

    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;
};

} // namespace task_tree
