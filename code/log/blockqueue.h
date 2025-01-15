#pragma once
#include <deque>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <iostream>

using namespace std;

/**
 * @brief 线程安全的阻塞队列模板类
 * @tparam T 队列中存储的元素类型
 */
template<typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t maxsize = 1000);
    ~BlockQueue();
    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item);
    bool pop(T& item, int timeout);
    void clear();
    T front();
    T back();
    size_t capacity();
    size_t size();
    void flush();
    void close();
private:
    deque<T> _deque;
    mutable mutex _mutex;
    bool _isClosed{false};  // 队列关闭标志
    const size_t _capacity;   // 队列容量
    condition_variable _notEmpty;  // 用于消费者的条件变量
    condition_variable _notFull;    // 用于生产者的条件变量
};

template<typename T>
BlockQueue<T>::BlockQueue(size_t maxsize) : _capacity(maxsize) {
    assert(maxsize > 0); // 确保最大容量大于0
}

template<typename T>
BlockQueue<T>::~BlockQueue() {
    close();
}

template<typename T>
void BlockQueue<T>::close() {
    {
        lock_guard<mutex> lock(_mutex);
        _deque.clear();
        _isClosed = true;
    }
    _notFull.notify_all();
    _notEmpty.notify_all();
}

template<typename T>
void BlockQueue<T>::clear() {
    lock_guard<mutex> lock(_mutex);
    _deque.clear();
}

template<typename T>
bool BlockQueue<T>::empty() {
    lock_guard<mutex> lock(_mutex);
    return _deque.empty();
}

template<typename T>
bool BlockQueue<T>::full() {
    lock_guard<mutex> lock(_mutex);
    return _deque.size() >= _capacity;
}

template<typename T>
void BlockQueue<T>::push_back(const T& item) {
    unique_lock<mutex> lock(_mutex);
    _notFull.wait(lock, [this]() { return _deque.size() < _capacity || _isClosed ;});
    if(_isClosed) return;
    _deque.push_back(item);
    _notEmpty.notify_one();
}

template<typename T>
void BlockQueue<T>::push_front(const T& item) {
    unique_lock<mutex> lock(_mutex);
    _notFull.wait(lock, [this]() { return _deque.size() < _capacity || _isClosed ;});
    if(_isClosed) return;
    _deque.push_front(item);
    _notEmpty.notify_one();
}

template<typename T>
bool BlockQueue<T>::pop(T& item) {
    unique_lock<mutex> lock(_mutex);
    _notEmpty.wait(lock, [this]() { return !_deque.empty() || _isClosed;});
    if(_isClosed) return false;
    item = _deque.front();
    _deque.pop_front();
    _notFull.notify_one();
    return true;
}
/**
 * @brief 从队列头部弹出元素，带有超时机制
 * @tparam T 队列中存储的元素类型
 * @param item 弹出的元素将存储在该参数中
 * @param timeout 超时时间（秒）
 * @return true 弹出成功
 * @return false 弹出失败（队列为空或超时）
 */
template<typename T>
bool BlockQueue<T>::pop(T& item, int timeout) {
    unique_lock<mutex> lock(_mutex);
    // 超时时返回最新的pred()
    if(!_notEmpty.wait_for(lock, chrono::seconds(timeout), 
                        [this]() { return !_deque.empty() || _isClosed;})){
        return false;
    }
    item = _deque.front();
    _deque.pop_front();
    _notFull.notify_one();
    return true;
}

template<typename T>
T BlockQueue<T>::front() {
    lock_guard<std::mutex> lock(_mutex);
    return _deque.front(); 
}

template<typename T>
T BlockQueue<T>::back() {
    lock_guard<std::mutex> lock(_mutex); 
    return _deque.back(); 
}

template<typename T>
size_t BlockQueue<T>::capacity() {
    lock_guard<std::mutex> lock(_mutex);  
    return _capacity;  
}

template<typename T>
size_t BlockQueue<T>::size() {
    lock_guard<std::mutex> lock(_mutex); 
    return _deque.size(); 
}

/**
 * @brief 唤醒一个等待的消费者线程
 * @tparam T 队列中存储的元素类型
 */
template<typename T>
void BlockQueue<T>::flush() {
    _notEmpty.notify_one();  // 唤醒一个等待的消费者线程
}
