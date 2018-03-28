cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
set(CMAKE_CXX_COMPILER /usr/local/opt/llvm/bin/clang++)
project(frontier VERSION 0.1 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g3" CACHE INTERNAL "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE INTERNAL "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-L/usr/local/opt/llvm/lib -Wl,-rpath,/usr/local/opt/llvm/lib")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(task_tree)
