#include "net/eventloopthread.h"
#include "net/eventloopthreadpool.h"
#include "net/eventloop.h"
#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <set>
#include <vector>
#include <memory> // 需要包含 memory 头文件

using namespace net;

class ThreadingInfrastructureTest : public ::testing::Test {};

// 测试 EventLoopThread 的核心功能和生命周期
TEST_F(ThreadingInfrastructureTest, EventLoopThreadLifecycleAndFunctionality) {
    std::unique_ptr<EventLoopThread> loop_thread = std::make_unique<EventLoopThread>();
    
    // 1. 验证 StartLoop 返回一个有效的、运行在其他线程的 EventLoop
    EventLoop* loop = loop_thread->StartLoop();
    ASSERT_NE(loop, nullptr);
    ASSERT_FALSE(loop->IsInLoopThread()); // 从主线程调用，应返回 false

    std::promise<bool> promise;
    auto future = promise.get_future();
    
    // 2. 验证 loop 确实在运行，可以接受并执行任务
    loop->RunInLoop([&]() {
        ASSERT_TRUE(loop->IsInLoopThread()); // 在 loop 线程内调用，应返回 true
        promise.set_value(true);
    });

    // 等待任务执行完成
    ASSERT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_TRUE(future.get());
    
    // 3. 验证析构的正确性
    // loop_thread 离开作用域时，其析构函数会被调用。
    // 如果析构函数能正确 Quit() 和 join() 线程，测试将顺利结束而不会卡死。
    // 这就是对 RAII 最好的测试。
}

// 测试 EventLoopThreadPool 的核心功能和生命周期
TEST_F(ThreadingInfrastructureTest, EventLoopThreadPoolDistributionAndLifecycle) {
    constexpr int kThreadPoolSize = 4;
    
    {
        EventLoopThreadPool pool(kThreadPoolSize);
        pool.Start();

        // 1. 验证 GetNextLoop 的轮询和线程安全性
        std::vector<EventLoop*> loops;
        for (int i = 0; i < kThreadPoolSize; ++i) {
            loops.push_back(pool.GetNextLoop());
            ASSERT_NE(loops.back(), nullptr);
        }
        // 再调用一次，应该回到第一个 loop
        ASSERT_EQ(pool.GetNextLoop(), loops[0]);

        // 2. 验证所有 loop 都在不同的、独立的线程中运行
        std::vector<std::future<std::thread::id>> futures;
        for (int i = 0; i < kThreadPoolSize; ++i) {
            // 核心修正：将 promise 包装在 shared_ptr 中
            auto promise_ptr = std::make_shared<std::promise<std::thread::id>>();
            futures.push_back(promise_ptr->get_future());
            
            // 把任务分发到每个 loop, lambda 捕获可拷贝的 shared_ptr
            loops[i]->RunInLoop([promise_ptr]() {
                promise_ptr->set_value(std::this_thread::get_id());
            });
        }

        // 收集所有线程 ID，验证它们是唯一的
        std::set<std::thread::id> thread_ids;
        for (auto& f : futures) {
            thread_ids.insert(f.get());
        }
        ASSERT_EQ(thread_ids.size(), kThreadPoolSize);

    } // pool 在这里离开作用域，测试其析构函数能否正确清理所有线程。
      // 如果测试能正常结束，就证明了 RAII 的正确性。
}

