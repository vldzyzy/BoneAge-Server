#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

// 时间精度控制
using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using Timestamp = Clock::time_point;

// 定时节点结构体(内存对齐优化)
struct alignas(64) TimerNode {
    int id;     // 唯一标识符
    Timestamp expires;  // 过期时间点
    TimeoutCallBack cb;

    // 最小堆
    bool operator<(const TimerNode& rhs) const noexcept {
        return expires < rhs.expires;
    }
};

/**
 * @brief 基于小顶堆的定时器容器
 * @特点
 *  - 使用最小堆结构管理定时器，堆顶总是最先超时的任务
 *  - 支持添加/删除/调整定时器操作 O(logn)
 *  - 通过哈希表实现快速节点定位
 *  - 定时触发回调函数
 * @适用场景
 *  - 网络编程中的超时连接管理
 *  - 需要大量高效管理定时任务的场景
 */
class HeapTimer {
public:
    HeapTimer() { _heap.reserve(64); }
    ~HeapTimer() { clear(); }


    void adjust(int id, int newExpires);    // 调整现有节点时间
    void add(int id, int timeOut, const TimeoutCallBack& cb);   // 添加节点
    void doWork(int id);    // 执行指定节点的回调并删除
    void clear();           // 清空所有节点
    void tick();            // 处理超时节点
    void pop();             // 删除堆顶节点
    int getNextTick();      // 获取下一个超时节点的剩余时间

private:
    void _del(size_t i);    // 删除指定索引节点
    void _siftup(size_t i); // 从索引i开始上浮调整
    bool _siftdown(size_t i, size_t n); // 从i开始下沉调整, n为堆大小
    void _swapNode(size_t i, size_t j);     // 交换两个节点位置

    std::vector<TimerNode> _heap;   // 堆容器
    std::unordered_map<int, size_t> _ref;   // ID到堆索引的逆向映射

};