#pragma once
#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

namespace sql {

class SqlConnPool {
public:
    using MysqlConnPtr = std::shared_ptr<MYSQL>;

    SqlConnPool(const SqlConnPool&) = delete;
    SqlConnPool& operator=(const SqlConnPool&) = delete;

    static SqlConnPool& GetInstance() {
        static SqlConnPool instance;
        return instance;
    }

    void Init(const char* host, int port,
        const char* user, const char* pwd,
        const char* db_name, int conn_size = 8);

    MysqlConnPtr GetConn();

    void Shutdown();

private:

    SqlConnPool() = default;
    ~SqlConnPool() { Shutdown(); };

    std::queue<MYSQL*> conn_que_;
    std::mutex mutex_;
    std::condition_variable que_cv_;

    bool is_shutdown_{true};
};

}