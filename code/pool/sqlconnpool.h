#pragma once
#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"
#include <chrono>
#include <unordered_map>

class SqlConnPool {
public:
    static SqlConnPool* instance();

    MYSQL* getConn();
    void freeConn(MYSQL* conn);
    int getFreeConnCount();

    void init(const char* host, int port,
        const char* user, const char* pwd,
        const char* dbName, int connSize);
    void closePool();

    void pingIdleConnections();     // 定时刷新空闲连接，避免超时

private:
    SqlConnPool();
    ~SqlConnPool();

    int _MAX_CONN;
    int _useCount;
    int _freeCount;

    std::queue<MYSQL*> _connQue;
    std::mutex _mtx;
    sem_t _semId;

    std::unordered_map<MYSQL*, std::chrono::steady_clock::time_point> _lastIdleTime;    // 存储连接最后一次变为空闲的时间戳
    static constexpr std::chrono::seconds MIN_IDLE_BEFORE_PING = std::chrono::hours(4);
};
