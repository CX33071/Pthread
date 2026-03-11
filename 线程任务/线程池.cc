#include <unistd.h>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
const int MAX_NUM = 10;
std::mutex cout_mutex;
std::mutex order_mutex;
std::condition_variable order_cond;
int next_execute_num = 0;
class Task {
   public:
    using functype = void (*)(void*);
    void set(functype func, void* arg,int num) {
        func_ = func;
        arg_ = arg;
        num_ = num;
    }
    void exe() {
        std::unique_lock<std::mutex> lock(order_mutex);
        while (num_ != next_execute_num) {
            order_cond.wait(lock);  
        }
        func_(arg_);         
        next_execute_num++;  
        order_cond.notify_all();
    }
   private:
    functype func_;
    void* arg_;
    int num_;
};
class Queue {
   public:
    Queue() : front_(0), last_(0), count_(0) {}
    void init() {
        front_ = 0;
        last_ = 0;
        count_ = 0;
    }
    bool add_task(Task& task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ >= MAX_NUM) {
            return false;
        }
        tasks[last_] = task;
        last_ = (last_ + 1) % MAX_NUM;
        count_++;
        cond_.notify_one();
        return true;
    }
    bool gettask(Task& task, bool& is_running) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ == 0 && is_running) {
            cond_.wait(lock);
        }
        if (!is_running && count_ == 0) {
            return false;
        }
        task = tasks[front_];
        front_ = (front_ + 1) % MAX_NUM;
        count_--;
        lock.unlock();
        return true;
    }
    void notify_all_threads() { cond_.notify_all(); }

   private:
    int front_;
    int last_;
    int count_;
    std::mutex mutex_;
    std::condition_variable cond_;
    Task tasks[MAX_NUM];
};
class Pool {
   public:
    Pool() : is_running(false) { queue.init(); }
    void init() {
        is_running = true;
        for (int i = 0; i < MAX_NUM; i++) {
            pthreads[i] = std::thread(&Pool::pthread_func, this);
        }
    }
    void pool_add_task(Task& task) { queue.add_task(task); }
    void close() {
        pool_mutex_.lock();
        is_running = 0;
        pool_mutex_.unlock();
        queue.notify_all_threads();
        for (auto& t : pthreads) {
            t.join();
        }
        std::cout << "线程池已关闭" << std::endl;
        return;
    }

   private:
    bool is_running;
    std::thread pthreads[MAX_NUM];
    Queue queue;
    void pthread_func() {
        while (is_running) {
            Task task;
            if (!queue.gettask(task, is_running)) {
                break;
            }
            task.exe();
        }
    };
    std::mutex pool_mutex_;
};
void juzhen(void* arg) {
    int num = *(int*)arg;
    long long ret = static_cast<long long>(num * num);
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "线程" << std::this_thread::get_id() << "计算" << num
              << "的平方等于" << ret << std::endl;
    free(arg);
}
void add_juzhen(Pool& pool) {
    for (int i = 0; i < 10; i++) {
        Task task;
        int* arg = (int*)malloc(sizeof(int));
        *arg = i;
        task.set(&juzhen, arg,i);
        pool.pool_add_task(task);
    }
}
int main() {
    Pool pool;
    pool.init();
    add_juzhen(pool);
    usleep(10000);
    pool.close();
}