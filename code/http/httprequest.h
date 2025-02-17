#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,   // 请求行
        HEADERS,    // 首部行
        BODY,   // 实体体
        FINISH, // 完成
    };

    HttpRequest() { init(); }
    ~HttpRequest() = default;

    // 初始化HTTP请求对象
    void init();

    // 解析HTTP请求
    bool parse(Buffer& buff);

    std::string path() const;       // 获取请求文件的路径
    std::string& path();            // 获取路径
    std::string method() const;     // 获取方法
    std::string version() const;    // 获取版本

    // 获取Post数据
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;

    // 判断是否保持连接
    bool isKeepAlive() const;

private:
    bool _parseRequestLine(const std::string& line);    // 请求行解析
    void _parseHeader(const std::string& line);    // 首部行解析
    void _parseBody(const std::string& line);   // 实体体解析

    void _parsePath();  // 请求文件的路径解析
    void _parsePost();  // Post解析
    void _parseFormUrlencoded();   // url解析
    void _parseMultipartFormData(); // 解析 multipart/form-data 格式

    void saveFile(const std::string& fileData); // 保存文件

    static bool userVerify(const std::string& name, const std::string& pwd, bool isLogin); // 用户验证
    PARSE_STATE _state; // 当前解析状态
    std::string _method, _path, _version, _body;    // 请求信息
    std::unordered_map<std::string, std::string> _header;   // 请求头
    std::unordered_map<std::string, std::string> _post;     // POST 数据

    // 常量
    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认HTML路径
        
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // 特定标签路径

    static int converHex(char ch);  // 16进制转换为10进制
};