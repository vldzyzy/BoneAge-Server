#pragma once
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->getConn();
        _sql = *sql;
        _connpool = connpool;
    }
    
    ~SqlConnRAII() {
        if(_sql) { _connpool->freeConn(_sql); }
    }
    
private:
    MYSQL *_sql;
    SqlConnPool* _connpool;
};