#pragma once
#include <unordered_map>
#include <string>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>   // struct stat
#include <sys/mman.h>   // mmap, munmap
#include "../buffer/buffer.h" 
#include "../log/log.h"

/**
 * @class HttpResponse
 * @brief HTTP响应生成器，用于构建和发送HTTP响应报文
 * 
 * 功能特性：
 * 1. 支持零拷贝文件传输（通过mmap内存映射）
 * 2. 自动处理20+种常见MIME类型识别
 * 3. 支持400/403/404等错误页面自动生成
 * 4. 完善的Keep-Alive连接管理
 * 5. 符合HTTP/1.1规范的标准响应构建
 * 
 * 核心组件：
 * - 内存映射文件处理：通过mmap实现高效文件读取
 * - 智能缓冲区管理：与外部Buffer类协同工作
 * - 状态码自动处理：支持4种标准状态码及扩展
 * 
 * 典型工作流程：
 * 1. init()初始化响应参数
 * 2. makeResponse()构建响应报文
 * 3. 通过file()/fileLen()获取内存映射数据
 * 4. unmapFile()资源清理
 */
class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buff);    // 创建HTTP响应
    void unmapFile();
    char* file();
    size_t fileLen() const;
    void errorContent(Buffer& buff, std::string message);
    int code() const { return _code; };

private:
    void _addState(Buffer& buff); // 添加状态行
    void _addHeader(Buffer& buff);  // 添加响应头
    void _addContent(Buffer& buff); // 添加响应体

    void _errorHtml();  // 错误HTML页面
    std::string _getFileType();  // 获取文件类型

    int _code; // 状态码
    bool _isKeepAlive;
    std::string _path;  // 文件路径
    std::string _srcDir;    // 文件根目录

    char* _mmFile;  // 映射到内存的文件
    struct stat _mmFileStat;    // 文件的状态信息

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型
    static const std::unordered_map<int, std::string> CODE_STATUS;  // 状态码描述
    static const std::unordered_map<int, std::string> CODE_PATH;    // 错误页面路径
};