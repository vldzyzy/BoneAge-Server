#include "sqlconnpool.h"
#include <vector>
using namespace std;

SqlConnPool::SqlConnPool() : _useCount(0)
, _freeCount(0) {
}

SqlConnPool* SqlConnPool::instance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        _connQue.push(sql);
        _lastIdleTime[sql] = std::chrono::steady_clock::now();
    }
    _MAX_CONN = connSize;
    sem_init(&_semId, 0, _MAX_CONN);
}

MYSQL* SqlConnPool::getConn() {
    MYSQL *sql = nullptr;
    sem_wait(&_semId);
    {
        lock_guard<mutex> locker(_mtx);
        sql = _connQue.front();
        _connQue.pop();
        _lastIdleTime.erase(sql);
    }
    return sql;
}

void SqlConnPool::freeConn(MYSQL* sql) {
    assert(sql);
    {
        lock_guard<mutex> locker(_mtx);
        _connQue.push(sql);
        _lastIdleTime[sql] = std::chrono::steady_clock::now();
    }
    sem_post(&_semId);
}

void SqlConnPool::closePool() {
    lock_guard<mutex> locker(_mtx);
    while(!_connQue.empty()) {
        auto item = _connQue.front();
        _connQue.pop();
        mysql_close(item);
    }
    _lastIdleTime.clear();
    mysql_library_end();        
}

int SqlConnPool::getFreeConnCount() {
    lock_guard<mutex> locker(_mtx);
    return _connQue.size();
}

SqlConnPool::~SqlConnPool() {
    closePool();
}

void SqlConnPool::pingIdleConnections() {
    std::lock_guard<std::mutex> locker(_mtx);
    int queueSize = _connQue.size();
    auto now = std::chrono::steady_clock::now();

    for(int i = 0; i < queueSize; ++i) {
        MYSQL* conn = _connQue.front();
        _connQue.pop();

        if(conn == nullptr) continue;

        auto it = _lastIdleTime.find(conn);
        bool needsPing = true;

        if(it != _lastIdleTime.end()) {
            auto idleDuration = now - it->second;
            if(idleDuration < MIN_IDLE_BEFORE_PING) {
                needsPing = false;
            }
        }
        
        if(needsPing) {
            if(mysql_ping(conn) != 0) {
                LOG_ERROR("mysql connection %p ping failed, closing it.", (void*)conn);
                mysql_close(conn);
                _lastIdleTime.erase(conn);
            }
            else {
                _connQue.push(conn);
                _lastIdleTime[conn] = now;
            }
        }
        else {
            _connQue.push(conn);
        }
    }
}