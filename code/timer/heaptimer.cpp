#include "heaptimer.h"

// 交换两个节点位置（维护堆和索引映射）
void HeapTimer::_swapNode(size_t i, size_t j) {
    assert(i >= 0 && i < _heap.size());
    assert(j >= 0 && j < _heap.size());
    std::swap(_heap[i], _heap[j]);

    // 更新索引映射
    _ref[_heap[i].id] = i;
    _ref[_heap[j].id] = j;
}
// 上浮调整 (插入时调整) 
void HeapTimer::_siftup(size_t i) {
    assert(i >= 0 && i < _heap.size());
    size_t j = (i - 1) / 2;     // 父节点

    // 循环比较直到到达根节点或满足堆条件
    while(j >= 0) {
        if(_heap[j] < _heap[i]) break;
        _swapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 下沉调整 （删除时调整）（返回值：false 不需要下滑，true: 下滑成功
bool HeapTimer::_siftdown(size_t index, size_t n) {
    assert(index >= 0 && index < _heap.size());
    assert(n >= 0 && n <= _heap.size());

    size_t i = index;
    size_t j = i * 2 + 1;   // 左子节点

    while(j < n) {
        // 选择较小的子节点
        if(j + 1 < n && _heap[j + 1] < _heap[j]) ++j;

        if(_heap[i] < _heap[j]) break;

        _swapNode(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;   // 返回是否发生下沉
}

// 添加节点
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(_ref.count(id) == 0) {
        /* 新节点：堆末尾插入，调整堆 */
        i = _heap.size();
        _ref[id] = i;
        _heap.push_back({id, Clock::now() + MS(timeout), cb});
        _siftup(i);
    }
    else {
        /* 已有节点：调整堆 */
        i = _ref[id];
        _heap[i].expires = Clock::now() + MS(timeout);
        _heap[i].cb = cb;

//    Case1：新过期时间比原时间更长（需要下沉）
//    - 新expires > 任一子节点 -> 通过siftdown_正确下沉
//    - siftdown_返回true，不进入siftup_

//    Case2：新过期时间比原时间更短（可能需要上浮）
//    - 新expires < 父节点 -> siftdown_无法处理
//    - siftdown_返回false，触发siftup_上浮
        if(!_siftdown(i, _heap.size())) {
            _siftup(i);
        }
    }
}

// 立即执行指定定时器的回调并删除
void HeapTimer::doWork(int id) {
    if(_heap.empty() || _ref.count(id) == 0) return;
    size_t i = _ref[id];
    TimerNode node = _heap[i]; // TODO:为什么拷贝？
    node.cb();
    _del(i);
}

// 删除指定堆位置节点
// 1. 将要删除节点交换到堆尾
// 2. 对原位置执行下沉/上浮调整
// 3. 移除堆尾元素
void HeapTimer::_del(size_t index) {
    assert(!_heap.empty() && index >= 0 && index < _heap.size());
    size_t i = index;
    size_t n = _heap.size() - 1;
    assert(i <= n);
    if(i < n) {
        _swapNode(i, n);
        if(!_siftdown(i, n)){
            _siftup(i);
        }
    }
    _ref.erase(_heap.back().id);
    _heap.pop_back();
}

// 调整定时器过期时间
void HeapTimer::adjust(int id, int timeout) {
    assert(!_heap.empty() && _ref.count(id) > 0);
    _heap[_ref[id]].expires = Clock::now() + MS(timeout);
    _siftdown(_ref[id], _heap.size());
}

// 清理所有超时定时器
// 循环检查堆顶元素
// - 未超时：终止循环
// - 已超时：执行回调->删除堆顶->循环继续
void HeapTimer::tick() {
    if(_heap.empty()) return;
    while(!_heap.empty()) {
        TimerNode node = _heap.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        pop();
        node.cb();
    } 
}

// 删除堆顶元素（最小超时节点
void HeapTimer::pop() {
    assert(!_heap.empty());
    _del(0);
}

// 清空定时器容器
void HeapTimer::clear() {
    _ref.clear();
    _heap.clear();
}

/**
 * @brief 获取下次超时等待时间
 * @return 等待时间（毫秒）
 * - 无定时器返回-1
 * - 已超时返回0
 * @处理流程：
 * 1. 先执行tick清理已过期定时器
 * 2. 计算堆顶元素的剩余时间
 */
int HeapTimer::getNextTick() {
    tick();
    size_t res = -1;
    if(!_heap.empty()) {
        res = std::chrono::duration_cast<MS>(_heap.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}