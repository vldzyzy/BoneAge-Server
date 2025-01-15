#include "log.h"

Log::Log() : _fp(nullptr), _deque(nullptr), _writeThread(nullptr),
            _lineCount(0), _toDay(0), _isAsync(false) {}

Log::~Log() {
    if(_deque) {
        while(!_deque->empty()) {
            _deque->flush();
        }
        _deque->close();
    }
    if(_writeThread && _writeThread->joinable()) {
        _writeThread->join();   // 等待写线程完成
    }
    if(_fp) {
        std::lock_guard<std::mutex> lock(_mutex);
        flush();
        fclose(_fp);
    }
}

void Log::flush() {
    // if(_isAsync && _deque) {
    //     _deque->flush();    // 唤醒异步队列
    // }
    if(_fp) {
        fflush(_fp);    // 刷新文件缓冲区
    }
}

// 获取日志实例（单例模式）
Log* Log::instance() {
    static Log log; // 局部和静态变量, 线程安全
    return &log;
}

// 异步写日志线程函数
void Log::flushLogThread() {
    Log::instance()->_asyncWrite();
}

// 异步写日志的实际执行函数
void Log::_asyncWrite() {
    std::string str;
    while(_deque && _deque->pop(str)) {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_fp) fputs(str.c_str(), _fp);
    }
}

// 初始化日志系统
void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity) {
    _isOpen = true;
    _level = level;
    _path = path;
    _suffix = suffix;

    if(maxQueCapacity > 0) {
        _isAsync = true;
        if(!_deque) {
            _deque = std::make_unique<BlockQueue<std::string>>(maxQueCapacity);
            _writeThread = std::make_unique<std::thread>(flushLogThread);
        }
    }
    else _isAsync = false;

    _lineCount = 0;
    auto now = std::chrono::system_clock::now();
    auto timer = std::chrono::system_clock::to_time_t(now);
    auto systime = *std::localtime(&timer);

    std::ostringstream oss;
    oss << _path << "/" << std::put_time(&systime, "%Y_%m_%d") << _suffix;
    std::string fileName = oss.str();
    _toDay = systime.tm_mday;
    
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _buff.RetrieveAll();
        if(_fp) {
            flush();
            fclose(_fp);
        }

        // 创建目录
        std::filesystem::create_directories(_path);
        _fp = fopen(fileName.c_str(), "a");
        assert(_fp != nullptr);
    }
}

// TODO: 自己写一遍
// 写入日志
void Log::write(int level, const char* format, ...) {
    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() % 1000000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm sysTime = *std::localtime(&timer);
    va_list vaList;

    // 检查是否需要创建新日志文件
    if (_toDay != sysTime.tm_mday || (_lineCount && (_lineCount % _MAX_LINES == 0))) {
        std::ostringstream oss;
        oss << _path << "/" << std::put_time(&sysTime, "%Y_%m_%d");
        auto tt = oss.str().c_str();

        if (_toDay != sysTime.tm_mday) {  // 新的一天
            oss << _suffix;
            _toDay = sysTime.tm_mday;
            _lineCount = 0;
        } 
        else {  // 同一天，但日志行数超限
            oss << "-" << (_lineCount / _MAX_LINES) << _suffix;
        }

        // 关闭旧文件，打开新文件
        std::lock_guard<std::mutex> lock(_mutex);
        flush();    // 确保所有内容写入旧文件
        fclose(_fp);
        _fp = fopen(oss.str().c_str(), "a");
        if(!_fp) {
            fprintf(stderr, "error opening file. %s (errno: %d)\n", strerror(errno), errno);
            exit(EXIT_FAILURE);
        }
    }

    // 写入日志内容
    {
        // 构造日志内容
        std::ostringstream oss;
        oss << std::put_time(&sysTime, "%Y-%m-%d %H:%M:%S.")
                <<  std::setw(6) << std::setfill('0') << now_us << " ";
                
        std::lock_guard<std::mutex> lock(_mutex);
        ++_lineCount;
        _buff.Append(oss.str().c_str(), oss.str().size());

        _appendLogLevelTitle(level);

        va_start(vaList, format);
        auto _wb = _buff.WritableBytes() -  10;
        int m = vsnprintf(_buff.BeginWrite(), _wb, format, vaList);
        va_end(vaList);

        _buff.HasWritten((m > _wb) ? _wb - 1 : m);
        _buff.Append("\n\0", 2);

        if (_isAsync && _deque && !_deque->full()) {  // 异步模式
            _deque->push_back(_buff.RetrieveAllToStr());
        } else {  // 同步模式
            fputs(_buff.Peek(), _fp);
        }
        _buff.RetrieveAll();
    }
}

void Log::_appendLogLevelTitle(int level) {
    switch(level) {
    case 0:
        _buff.Append("[debug]: ", 9);
        break;
    case 1:
        _buff.Append("[info] : ", 9);
        break;
    case 2:
        _buff.Append("[warn] : ", 9);
        break;
    case 3:
        _buff.Append("[error]: ", 9);
        break;
    default:
        _buff.Append("[info] : ", 9);
        break;
    }
}

int Log::getLevel() {
    lock_guard<mutex> lock(_mutex);
    return _level;
}

void Log::setLevel(int level) {
    lock_guard<mutex> lock(_mutex);
    _level = level;
}