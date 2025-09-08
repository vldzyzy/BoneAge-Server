#include "gtest/gtest.h"
#include "sql/sqlconnpool.h" // 引入你的连接池头文件
#include <vector>
#include <thread>
#include <future>
#include <chrono>

using namespace sql;

// --- 测试用的数据库配置 ---
// !!! 在运行测试前，请确保这些配置与你的环境一致 !!!
const char* DB_HOST = "172.17.0.1";
const int DB_PORT = 3306;
const char* DB_USER = "webserver";
const char* DB_PASS = "ZYzy@2025+-*";
const char* DB_NAME = "webserver";
const int POOL_SIZE = 8;


// Test Fixture for setting up and tearing down the pool
class SqlConnPoolTest : public ::testing::Test {
protected:
    // 每个测试用例开始前执行
    void SetUp() override {
        pool = &SqlConnPool::GetInstance();
        try {
            pool->Init(DB_HOST, DB_PORT, DB_USER, DB_PASS, DB_NAME, POOL_SIZE);
        } catch (const std::runtime_error& e) {
            FAIL() << "Database pool Initialization failed: " << e.what() 
                   << "\nPlease ensure MySQL is running and configured correctly.";
        }
    }

    // 每个测试用例结束后执行
    void TearDown() override {
        pool->Shutdown();
    }

    SqlConnPool* pool;
};

// 测试1：初始化
// 验证连接池初始化后，空闲连接数是否正确
TEST_F(SqlConnPoolTest, Initialization) {
    // 因为 Deleter 的实现，无法直接获取空闲连接数。
    // 我们可以通过连续获取 POOL_SIZE 次连接来验证。
    std::vector<SqlConnPool::MysqlConnPtr> conns;
    for(int i = 0; i < POOL_SIZE; ++i) {
        auto conn = pool->GetConn();
        ASSERT_NE(conn, nullptr) << "Failed to get connection #" << i;
        conns.push_back(std::move(conn));
    }
    
    // 此时再获取应该会阻塞（或在超时设计下失败）
    // 为了不让测试卡住，我们这里只验证前面的获取是成功的。
    SUCCEED();
}

// 测试2：连接的获取与自动归还
// 验证RAII机制是否正常工作
TEST_F(SqlConnPoolTest, GetAndReturn) {
    {
        auto conn1 = pool->GetConn();
        ASSERT_NE(conn1, nullptr);
        // conn1 在这里离开作用域，应自动归还
    }
    
    // 应该能立刻获取到刚刚归还的连接
    auto conn2 = pool->GetConn();
    ASSERT_NE(conn2, nullptr);
}

// 测试3：多线程并发测试
// 模拟多个线程同时从池中获取和归还连接
TEST_F(SqlConnPoolTest, MultiThreaded) {
    auto& pool = SqlConnPool::GetInstance();
    std::vector<std::thread> threads;
    const int num_threads = 16;
    const int queries_per_thread = 50;

    auto task = [&]() {
        for (int i = 0; i < queries_per_thread; ++i) {
            auto conn = pool.GetConn();
            ASSERT_NE(conn, nullptr);

            // 执行查询
            int query_res = mysql_query(conn.get(), "SELECT 1");
            if (query_res != 0) {
                 FAIL() << "Query failed: " << mysql_error(conn.get());
            }
            ASSERT_EQ(query_res, 0);

            MYSQL_RES* result = mysql_store_result(conn.get());
            ASSERT_NE(result, nullptr) << "mysql_store_result failed: " << mysql_error(conn.get());
            mysql_free_result(result);
        }
    };

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(task);
    }

    for (auto& t : threads) {
        t.join();
    }
}

// 测试4：连接耗尽与等待
// 验证当池中无可用连接时，新的请求会正确等待
TEST_F(SqlConnPoolTest, ExhaustAndWaiting) {
    std::vector<SqlConnPool::MysqlConnPtr> conns;
    
    // 1. 取出所有连接
    for (int i = 0; i < POOL_SIZE; ++i) {
        conns.push_back(pool->GetConn());
    }

    // 2. 异步启动一个新任务，它应该会阻塞
    auto waiting_future = std::async(std::launch::async, [&]() {
        // 这个 getConn() 会被阻塞，直到有连接被归还
        auto conn = pool->GetConn();
        ASSERT_NE(conn, nullptr);
    });

    // 3. 检查任务是否真的在等待
    auto status = waiting_future.wait_for(std::chrono::milliseconds(50));
    ASSERT_EQ(status, std::future_status::timeout) << "The thread should be waiting for a connection.";

    // 4. 归还一个连接
    conns.pop_back();

    // 5. 等待任务完成，这次应该很快就能完成
    ASSERT_NO_THROW(waiting_future.get());
}

// 测试5：关闭连接池
// 验证关闭后，无法获取新连接，且等待的线程会退出
TEST_F(SqlConnPoolTest, ClosePool) {
    // 1. 启动一个等待的线程
    auto waiting_future = std::async(std::launch::async, [&]() {
        // 先获取所有连接，让下一个 getConn 阻塞
        std::vector<SqlConnPool::MysqlConnPtr> conns;
        for (int i = 0; i < POOL_SIZE; ++i) conns.push_back(pool->GetConn());
        
        // 这个 getConn() 会阻塞，直到 close() 被调用
        auto conn = pool->GetConn();
        ASSERT_EQ(conn, nullptr); // 关闭后应返回 nullptr
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 确保上面的线程已经阻塞

    // 2. 关闭连接池
    pool->Shutdown();

    // 3. 等待的线程应该会立即退出
    ASSERT_NO_THROW(waiting_future.get());

    // 4. 再次尝试获取连接，应该直接返回 nullptr
    auto conn_after_close = pool->GetConn();
    ASSERT_EQ(conn_after_close, nullptr);
}
