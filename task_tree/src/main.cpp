#include <iostream>

#include <task_tree/ThreadPool.hpp>
#include <task_tree/TaskScheduler.hpp>

using namespace task_tree;
using namespace std::chrono_literals;

int main(int /*argv*/, char** /*argc*/)
{
    auto threadPool = std::make_shared<ThreadPool>();
    {
        auto taskScheduler = TaskScheduler{threadPool};

        taskScheduler.scheduleEvery([]() { std::cout << "1\n"; }, 1s);
        taskScheduler.scheduleEvery([]() { std::cout << "2\n"; }, 2s);
        taskScheduler.scheduleEvery([]() { std::cout << "3\n"; }, 3s);
        taskScheduler.scheduleEvery([]() { std::cout << "4\n"; }, 4s);
        taskScheduler.scheduleEvery([]() { std::cout << "5\n"; }, 5s);

        std::mutex _m;
        std::condition_variable cv;
        taskScheduler.scheduleIn(
            [&]() {
                std::cout << "Done\n";
                cv.notify_one();
            },
            10s);
        std::unique_lock<std::mutex> lock{_m};
        cv.wait(lock);
    }

    return 0;
}
