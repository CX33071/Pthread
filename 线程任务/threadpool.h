#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class pool {
   private:
    int thread_count;
    std::queue<std::function<void()>>
        works;  // 任务队列：存储无参数、无返回值的可调用对象
    std::vector<std::thread> thd;
    std::mutex mtx;
    std::condition_variable cv;
    bool runflag = true;

   public:
    pool(int n = 4) : thread_count(n) {
        for (int i = 0; i < thread_count; ++i) {
            thd.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock,
                                [this] { return !works.empty() || !runflag; });
                        if (!runflag && works.empty())
                            return;
                        if (!works.empty()) {
                            task = std::move(works.front());
                            works.pop();
                        }
                    }
                    if (task)
                        task();
                }
            });
        }
    }

    ~pool() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            runflag = false;
        }
        cv.notify_all();
        for (auto& t : thd) {
            if (t.joinable())
                t.join();
        }
    }

   public:
    template <typename F, typename... Args>//接受任意类型的函数：可调用函数+任意数量参数
    auto enqueue(F&& f, Args&&... args)//万能引用，可接收左值和右值
        -> std::future<typename std::invoke_result<F, Args...>::type> {//std::invoke推导f的返回值类型,future是未来取结果的容器
        // 这一步相当于定义了一个 “万能快递箱”，能装下任何类型的 “包裹（函数）”和 “配件（参数）”，还承诺会返回一个 “取件码（future）”
        using return_type = typename std::invoke_result<F, Args...>::type;

        // 使用 packaged_task 封装任务并绑定返回值
        auto task = std::make_shared<
            std::packaged_task<return_type()>>([func = std::forward<F>(f),
                                                args = std::make_tuple(
                                                    std::forward<Args>(
                                                        args)...)]() mutable {
            return std::apply(
                func,
                args);  // 把传入的可变参数（比如
                        // 1,2）打包成一个std::tuple（元组），把元组args拆成单个参数，传给func执行
        });

        // 获取 future 对象，获取 future：给调用者返回 “取件码”
        std::future<return_type> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mtx);
            works.emplace([task]() { (*task)(); });  // 将任务包装为 void() 类型
        }

        cv.notify_one();
        return res;
    }
};