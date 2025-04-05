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
#include "../types/types.h"

enum HTTP_CODE {
    NO_REQUEST,  // http请求 解析还没完成，继续解析
    GET_REQUEST,    // 解析完成，不再解析
    BAD_REQUEST,    // 错误的请求内容，不再解析
    NO_RESOURSE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

enum PARSE_STATE {
    REQUEST_LINE,   // 请求行
    HEADERS,    // 首部行
    BODY,   // 实体体
    FINISH, // 完成
};

class HttpRequest {
public:
    HttpRequest() { init(); }
    ~HttpRequest() = default;

    // 初始化HTTP请求对象
    void init();

    // 解析HTTP请求
    HTTP_CODE parse(Buffer& buff);

    std::string path() const;       // 获取请求文件的路径
    std::string& path();            // 获取路径
    std::string method() const;     // 获取方法
    std::string version() const;    // 获取版本

    // 获取Post数据
    std::shared_ptr<PostData> getPostPtr() const;

    // 新增接口：获取用户上传的图片数据（对 _body 内部数据的引用，不经过复制）
    // std::string_view getUploadImage() const { return _uploadImage; }

    // 判断是否保持连接
    bool isKeepAlive() const;
    

private:
    HTTP_CODE _parseRequestLine(const std::string& line);    // 请求行解析
    HTTP_CODE _parseHeader(const std::string& line);    // 首部行解析
    HTTP_CODE _parseBody();   // 实体体解析
    void _parseFormData();  // 解析 multipart/form-data 格式

    void _parsePath();  // 请求文件的路径解析
    void _parsePost();  // Post解析
    void _parseFormUrlencoded();   // url解析
    void _parseMultipartFormData(); // 解析 multipart/form-data 格式

    void saveFile(const std::string& fileData); // 保存文件

    PARSE_STATE _state; // 当前解析状态
    std::string _method, _path, _version;    // 请求信息
    std::shared_ptr<PostData> _post;

    bool _linger;           // 
    size_t _contentLen;     // 

    std::unordered_map<std::string, std::string> _header;   // 首部字段

    // 常量
    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认HTML路径
        
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // 特定标签路径

    static int converHex(char ch);  // 16进制转换为10进制
};