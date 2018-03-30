project(task_tree LANGUAGES CXX)

set(LIB_HEADERS
    include/task_tree/NonCopyable.hpp
    include/task_tree/ScopedSuspendLock.hpp
    include/task_tree/TaskScheduler.hpp
    include/task_tree/ThreadPool.hpp
    )

set(LIB_SOURCES
    src/TaskScheduler.cpp
    src/ThreadPool.cpp
    )

add_library(task_tree_lib STATIC ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(task_tree_lib
    PUBLIC
    include
    PRIVATE
    )

set (EXEC_SOURCES
    src/main.cpp
    )

add_executable(task_tree ${EXEC_SOURCES})

target_link_libraries(task_tree
    task_tree_lib
    )

