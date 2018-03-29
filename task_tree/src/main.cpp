#include <iostream>
#include <sstream>

#include <task_tree/ThreadPool.hpp>
//#include <task_tree/TaskScheduler.hpp>

using namespace task_tree;
using namespace std::chrono_literals;

template <typename T>
void concatTo(std::stringstream& ss, const T& val)
{
    ss << val;
}

template <typename First, typename... Args>
void concatTo(std::stringstream& ss, const First& val, Args... args)
{
    ss << val << " ";
    concatTo(ss, args...);
}

template <typename... Args>
std::string print(Args... args)
{
    std::stringstream ss{};
    concatTo(ss, args...);
    return ss.str();
}

int main(int /*argv*/, char** /*argc*/)
{
    auto t = [](int x) { print(x, '\n'); };
    auto times = 100000u;

    std::cout << "Async\n";
    {
        auto threadPool = ThreadPool{};
        auto start = std::chrono::steady_clock::now();
        {
            for (auto i{0u}; i < times; ++i) {
                threadPool.queueTask(t, i);
            }
        }
        std::cout << (std::chrono::steady_clock::now() - start).count() << std::endl;
        auto t_ret = [](int x) { return x; };
        auto res = threadPool.queueTask(t_ret,1);
        std::cout << res.get() << std::endl;
    }

    std::cout << "Sync\n";
    {
        auto start = std::chrono::steady_clock::now();
        {
            for (auto i{0u}; i < times; ++i) {
                t(i);
            }
        }
        std::cout << (std::chrono::steady_clock::now() - start).count() << std::endl;
    }

    //{
    // auto taskScheduler = TaskScheduler{threadPool};
    //
    // taskScheduler.scheduleEvery([]() { std::cout << "1\n"; }, 1s);
    // taskScheduler.scheduleEvery([]() { std::cout << "2\n"; }, 2s);
    // taskScheduler.scheduleEvery([]() { std::cout << "3\n"; }, 3s);
    // taskScheduler.scheduleEvery([]() { std::cout << "4\n"; }, 4s);
    // taskScheduler.scheduleEvery([]() { std::cout << "5\n"; }, 5s);
    //
    // std::mutex _m;
    // std::condition_variable cv;
    // taskScheduler.scheduleIn(
    //[&]() {
    // std::cout << "Done\n";
    // cv.notify_one();
    //},
    // 10s);
    // std::unique_lock<std::mutex> lock{_m};
    // cv.wait(lock);
    //}

    return 0;
}
