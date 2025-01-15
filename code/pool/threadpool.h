#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <cassert>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8)
    : _pool(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for(size_t i = 0; i < threadCount; ++i) {
            std::thread([pool = _pool]() {
                std::unique_lock<std::mutex> lock(pool->mtx);
                while (true)
                {   
                    if(!pool->tasks.empty()) {
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        lock.unlock();
                        task();
                        lock.lock();
                    }
                    else if(pool->isClosed) break;
                    else pool->cond.wait(lock);
                }
            }).detach();
        }
    }

    ThreadPool() = default;

    // 禁用拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 启用移动
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() {
        if(static_cast<bool>(_pool)) {
            {
            std::lock_guard<std::mutex> lock(_pool->mtx);
            _pool->isClosed = true;
            }
            _pool->cond.notify_all();
        }
    }

    template<class T>
    void addTask(T&& task) {
        {
            std::lock_guard<std::mutex> lock(_pool->mtx);
            _pool->tasks.emplace(std::forward<T>(task));
        }
        _pool->cond.notify_one();
    }

private:
    // 声明给创建的线程传递的参数的结构体
    // 也就是线程间的共享资源：锁、条件变量、运行状态、任务队列
    struct Pool {
        std::mutex mtx;     // 线程的锁
        std::condition_variable cond;   // 线程的条件变量
        bool isClosed;  // 线程的运行状态
        std::queue<std::function<void()>> tasks;    // 任务队列
    };
    // 线程参数结构体创建后用智能指针管理
    // 保证当最后一个线程销毁时，这个保存在ThreadPool类中的结构体才会销毁，而不会随着类对象的销毁而销毁
    std::shared_ptr<Pool> _pool;
};