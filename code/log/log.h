#pragma once
#include <mutex>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/time.h>
#include <cstring>       
#include <cassert>
#include <cstdarg>
#include <filesystem>
#include <chrono>        
#include "../buffer/buffer.h"
#include "blockqueue.h"

// TODO: Modern C++
class Log {
public:
    // 初始化日志实例（阻塞队列最大容量、日志保存路径、日志文件后缀）
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* instance();
    static void flushLogThread();   // 异步写日志公有方法，调用私有方法asyncWrite
    
    void write(int level, const char *format,...);  // 将输出内容按照标准格式整理
    void flush();

    int getLevel();
    void setLevel(int level);
    bool isOpen() const { return _isOpen; }
    
private:
    Log();
    void _appendLogLevelTitle(int level);
    virtual ~Log();
    void _asyncWrite(); // 异步写日志方法

private:

    const char* _path;          // 路径名
    const char* _suffix;        // 后缀名

    static constexpr int _MAX_LINES = 50000;             // 最大日志行数
 
    int _lineCount;             // 日志行数记录
    int _toDay;                 // 按当天日期区分文件

    bool _isOpen;               
 
    Buffer _buff;       // 输出的内容，缓冲区
    int _level;         // 日志等级, 等级低的可以显示等级高的
    bool _isAsync;      // 是否开启异步日志

    // TODO: c++风格文件
    FILE* _fp;                                          // 打开log的文件指针
    std::unique_ptr<BlockQueue<std::string>> _deque;    // 阻塞队列
    std::unique_ptr<std::thread> _writeThread;          // 写线程的指针
    std::mutex _mutex;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* _log_ = Log::instance(); \
        if (_log_->isOpen() && _log_->getLevel() <= level) {\
            _log_->write(level, format, ##__VA_ARGS__); \
            _log_->flush();\
        }\
    } while(0);

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口   log->flush();
// ...表示可变参数，__VA_ARGS__就是将...的值复制到这里
// 前面加上##的作用是：当可变参数的个数为0时，这里的##可以把把前面多余的","去掉,否则会编译出错。
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);
