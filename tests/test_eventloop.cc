#include "net/eventloop.h"
#include "net/channel.h"
#include "net/epoller.h"
#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <unistd.h>
#include <atomic>
#include <sys/timerfd.h>

using namespace net;

class ReactorCoreTest : public ::testing::Test {};

// 测试1：EventLoop 线程归属与安全启停
// 验证 EventLoop 能在自己的线程中启动和销毁
TEST_F(ReactorCoreTest, LifecycleAndThreadAffinity) {
    EventLoop* loop_ptr = nullptr;
    std::promise<EventLoop*> promise;
    auto future = promise.get_future();

    std::thread t([&]() {
        EventLoop loop;
        loop_ptr = &loop;
        promise.set_value(loop_ptr);
        loop.Loop(); // 阻塞，直到 Quit() 被调用
    });

    EventLoop* loop_in_other_thread = future.get();
    ASSERT_TRUE(loop_in_other_thread != nullptr);
    ASSERT_FALSE(loop_in_other_thread->IsInLoopThread());

    // 从主线程请求退出
    loop_in_other_thread->Quit();
    t.join(); // 线程应该能干净地退出
}

// 测试2：跨线程任务调度 (RunInLoop)
// 验证从外部线程提交的任务能被正确执行
TEST_F(ReactorCoreTest, CrossThreadTaskExecution) {
    EventLoop* loop_ptr = nullptr;
    std::promise<EventLoop*> promise;
    auto future = promise.get_future();
    std::atomic<int> counter = 0;

    std::thread t([&]() {
        EventLoop loop;
        loop_ptr = &loop;
        promise.set_value(loop_ptr);
        loop.Loop();
    });

    EventLoop* loop_in_other_thread = future.get();

    // 提交一系列任务
    loop_in_other_thread->RunInLoop([&]() { counter++; });
    loop_in_other_thread->RunInLoop([&]() { counter++; });
    
    // 提交一个任务，它再提交一个任务 (测试自唤醒)
    loop_in_other_thread->RunInLoop([&]() {
        counter++;
        loop_in_other_thread->RunInLoop([&]() {
            counter++;
        });
    });

    // 提交最后一个任务，用于同步和退出
    std::promise<void> quit_promise;
    auto quit_future = quit_promise.get_future();
    loop_in_other_thread->RunInLoop([&]() {
        loop_in_other_thread->Quit();
        quit_promise.set_value();
    });

    // 等待退出任务完成，确保所有任务都被处理
    quit_future.wait();
    t.join();

    ASSERT_EQ(counter.load(), 4);
}

// 测试3：IO 事件处理 (Channel & Epoller)
// 验证 Channel 能被正确添加、触发 IO 事件并执行回调
TEST_F(ReactorCoreTest, IOEventHandling) {
    EventLoop* loop_ptr = nullptr;
    std::promise<EventLoop*> promise;
    auto future = promise.get_future();
    std::atomic<bool> callback_executed = false;

    // 使用 timerfd 进行测试，它比 pipe 更可控
    int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    ASSERT_GE(timer_fd, 0);

    std::thread t([&]() {
        EventLoop loop;
        loop_ptr = &loop;
        promise.set_value(loop_ptr);
        loop.Loop();
    });

    EventLoop* loop = future.get();
    
    // 在 EventLoop 线程中创建和注册 Channel
    loop->RunInLoop([&]() {
        Channel* timer_channel = new Channel(loop, timer_fd);
        timer_channel->SetReadCallback([&]() {
            callback_executed = true;
            uint64_t expirations;
            ::read(timer_fd, &expirations, sizeof(expirations)); // 必须读取，否则会 busy-loop
        });
        timer_channel->EnableReading(); // 这会调用 loop->UpdateChannel
    });

    // 启动定时器，10ms后触发
    struct itimerspec new_value;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_nsec = 10 * 1000 * 1000;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    ::timerfd_settime(timer_fd, 0, &new_value, nullptr);

    // 等待回调被执行
    for (int i = 0; i < 10 && !callback_executed; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_TRUE(callback_executed);

    // 清理
    loop->Quit();
    t.join();
    ::close(timer_fd);
}

// 测试4：Channel 的移除
// 验证 Channel 能被正确地从 Epoller 中移除，不再接收事件
TEST_F(ReactorCoreTest, ChannelRemoval) {
    EventLoop* loop_ptr = nullptr;
    std::promise<EventLoop*> promise;
    auto future = promise.get_future();
    std::atomic<int> call_count = 0;

    int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    Channel* timer_channel_ptr = nullptr; // 需要在外部持有指针以便移除

    std::thread t([&]() {
        EventLoop loop;
        loop_ptr = &loop;
        promise.set_value(loop_ptr);
        
        Channel timer_channel(&loop, timer_fd);
        timer_channel_ptr = &timer_channel;

        timer_channel.SetReadCallback([&]() {
            call_count++;
            uint64_t expirations;
            ::read(timer_fd, &expirations, sizeof(expirations));
        });
        timer_channel.EnableReading();

        loop.Loop();
    });

    EventLoop* loop = future.get();
    
    // 启动一个周期性定时器，每 20ms 触发一次
    struct itimerspec new_value;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_nsec = 20 * 1000 * 1000;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 20 * 1000 * 1000;
    ::timerfd_settime(timer_fd, 0, &new_value, nullptr);
    
    // 等待几次触发
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_GE(call_count.load(), 2);

    // 在 EventLoop 线程中移除 Channel
    loop->RunInLoop([&]() {
        timer_channel_ptr->DisableAll(); // 必须先 Disable
        // loop->RemoveChannel(timer_channel_ptr); // 在重构后的 Epoller 中，DisableAll 就会触发 remove
    });
    
    // 记录移除时的计数值
    int count_before_removal = call_count.load();
    
    // 再等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 验证回调次数没有再增加
    ASSERT_EQ(call_count.load(), count_before_removal);

    // 清理
    loop->Quit();
    t.join();
    ::close(timer_fd);
}