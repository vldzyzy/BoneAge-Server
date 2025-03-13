#include "log/log.h"
#include "pool/threadpool.h"

void testLog() {
    int cnt = 0, level = 0;
    Log::instance()->init(level, "/home/zhaoyu/cpp_project/WebServer-main/res", ".log", 0);
    for (level = 3; level >= 0; --level) {
        Log::instance()->setLevel(level);
        for(int i = 0; i < 10000; ++i) {
            for(int j = 0; j < 4; ++j) {
                LOG_BASE(j, "%s同步模式第%d行", "Test: ", cnt++);
            }
        }
    }
    printf("%d\n", cnt);
}

void testLog2() {
    int cnt = 0, level = 0;
    Log::instance()->init(level, "/home/zhaoyu/cpp_project/WebServer-main/res", ".log", 5000);
    for (level = 3; level >= 0; --level) {
        Log::instance()->setLevel(level);
        for(int i = 0; i < 10000; ++i) {
            for(int j = 0; j < 4; ++j) {
                LOG_BASE(j, "%s异步模式第%d行", "Test: ", cnt++);
            }
        }
    }
}

void threadLogTask(int i, int cnt) {
    for(int j = 0; j < 10000; j++ ) {
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ",std::this_thread::get_id(), cnt++);
    }
}

void testThreadPool() {
    Log::instance()->init(0, "/home/zhaoyu/cpp_project/WebServer-main/res/testThread", ".log", 5000);
    ThreadPool threadpool(8);
    for(int i = 0; i < 24; ++i) {
        threadpool.addTask(std::bind(threadLogTask, i % 4, i * 10000));
    }
    int n;
    std::cin >> n;
}

int main() {
    testThreadPool();
}