#include <iostream>
#include <sstream>
#include <cassert>

#include <task_tree/TaskScheduler.hpp>

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
    auto t = [](std::atomic<unsigned int>* a, int x) {
        print(x, '\n');
        a->fetch_add(1);
        std::this_thread::sleep_for(100000ns);
    };
    auto times = 100000u;

    std::cout << "Async\n";
    {
        auto start = std::chrono::steady_clock::now();
        std::atomic<unsigned int> count;
        {
            ThreadPool threadPool{};
            for (auto i{0u}; i < times; ++i) {
                threadPool.queueTask(t, &count, i);
            }
        }
        assert(count == times);
        std::cout
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
            << std::endl;
    }

    std::cout << "Sync\n";
    {
        auto start = std::chrono::steady_clock::now();
        std::atomic<unsigned int> count;
        {
            for (auto i{0u}; i < times; ++i) {
                t(&count, i);
            }
        }
        assert(count == times);
        std::cout
            << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
            << std::endl;
    }

    {
        auto threadPool = std::make_shared<ThreadPool>();
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
