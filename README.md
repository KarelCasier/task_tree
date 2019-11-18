[![Actions Status](https://github.com/karelcasier/task_tree/workflows/Build/badge.svg)](https://github.com/karelcasier/task_tree/actions)

# task_tree

A simple ThreadPool and TaskScheduler in c++.

## quickstart

see [task_tree/src/main.cpp](task_tree/src/main.cpp) for usage examples.

## building

### macos

```sh
brew install CMake Ninja
brew install llvm
```

#### build

```sh
mkdir out/
cd out/
cmake .. -G Ninja
ninja
```

#### run

```sh
./bin/task_tree
```
