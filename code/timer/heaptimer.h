#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <functional>

class TcpConnection;
class EventLoop;

class HeapTimer {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using ConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionWeakPtr = std::weak_ptr<TcpConnection>;

    explicit HeapTimer(EventLoop* loop);
    ~HeapTimer();

    void AddOrAdjust(const ConnectionPtr& conn, int timeout_ms);

};