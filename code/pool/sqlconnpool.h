#pragma once
#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

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

private:
    SqlConnPool();
    ~SqlConnPool();

    int _MAX_CONN;
    int _useCount;
    int _freeCount;

    std::queue<MYSQL*> _connQue;
    std::mutex _mtx;
    sem_t _semId;
};
