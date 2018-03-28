#pragma once

#include <functional>

namespace task_tree {

using Task = std::function<void()>;

} // namespace task_tree
