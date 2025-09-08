#include "sqlconnpool.h"
#include "logging/logger.h"
#include <mutex>
#include <mysql/mysql.h>
#include <stdexcept>

namespace sql {

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* db_name,
            int conn_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < conn_size; i++) {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            throw std::runtime_error("SqlConnPool::Init: mysql_init failed");
        }
        conn = mysql_real_connect(conn, host, user, pwd,
                                 db_name, port, nullptr, 0);
        if (!conn) {
            mysql_close(conn);
            throw std::runtime_error("SqlConnPool::Init: mysql_real_connect failed");
        }
        conn_que_.push(conn);
    }
    is_shutdown_ = false;
}

SqlConnPool::MysqlConnPtr SqlConnPool::GetConn() {
    std::unique_lock<std::mutex> lock(mutex_);
    que_cv_.wait(lock, [this] { return !conn_que_.empty() || is_shutdown_; });
    if (is_shutdown_) {
        return nullptr;
    }
    MYSQL* raw_conn = conn_que_.front();
    conn_que_.pop();

    return MysqlConnPtr(raw_conn, [this](MYSQL* conn) {
        if (!conn) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_shutdown_) {
            mysql_close(conn);
        } else {
            conn_que_.push(conn);
            que_cv_.notify_one();
        }
    });
}

void SqlConnPool::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_shutdown_ = true;
    while (!conn_que_.empty()) {
        MYSQL* conn = conn_que_.front();
        conn_que_.pop();
        mysql_close(conn);
    }       
    que_cv_.notify_all();
}

}