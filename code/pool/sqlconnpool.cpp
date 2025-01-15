#include "sqlconnpool.h"
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
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        _connQue.push(sql);
    }
    _MAX_CONN = connSize;
    sem_init(&_semId, 0, _MAX_CONN);
}

MYSQL* SqlConnPool::getConn() {
    MYSQL *sql = nullptr;
    if(_connQue.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&_semId);
    {
        lock_guard<mutex> locker(_mtx);
        sql = _connQue.front();
        _connQue.pop();
    }
    return sql;
}

void SqlConnPool::freeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(_mtx);
    _connQue.push(sql);
    sem_post(&_semId);
}

void SqlConnPool::closePool() {
    lock_guard<mutex> locker(_mtx);
    while(!_connQue.empty()) {
        auto item = _connQue.front();
        _connQue.pop();
        mysql_close(item);
    }
    mysql_library_end();        
}

int SqlConnPool::getFreeConnCount() {
    lock_guard<mutex> locker(_mtx);
    return _connQue.size();
}

SqlConnPool::~SqlConnPool() {
    closePool();
}