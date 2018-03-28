project(task_tree LANGUAGES CXX)

find_package(SDL2 REQUIRED)

set(LIB_HEADERS
    include/task_tree/Task.hpp
    include/task_tree/TaskScheduler.hpp
    include/task_tree/Thread.hpp
    include/task_tree/ThreadFuture.hpp
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

target_link_libraries(task_tree_lib
    PUBLIC
    PRIVATE
    ${SDL2_LIBRARY}
    )

set (EXEC_HEADERS
    )

set (EXEC_SOURCES
    src/main.cpp
    )

add_executable(task_tree ${EXEC_SOURCES} ${EXEC_HEADERS})

target_link_libraries(task_tree
    task_tree_lib
    )

