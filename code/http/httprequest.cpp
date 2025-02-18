#include "httprequest.h"

using namespace std;

constexpr char* CRLF = "\r\n";

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{   // 默认HTML路径
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture",};  
    
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{ // 特定标签路径
    {"/register.html", 0}, {"/login.html", 1},}; 

void HttpRequest::init() {
    _method = _path = _version = _body = "";
    _linger = false;
    _contentLen = 0;
    _state = REQUEST_LINE;
    _header.clear();
    _post.clear();
}

// 传统的控制流程都是按照顺序执行的，状态机能 处理任意顺序的事件，并能提供有意义的响应—即使这些事件发生的顺序和预计的不同。
// HTTP协议并未提供头部长度字段，并且头部长度的变化也很大。
HTTP_CODE HttpRequest::parse(Buffer& buff) {
    while(buff.readableBytes()) {
        const char* lineEnd;
        string line;
        // 除了消息体，逐行解析
        if (_state != BODY) {
            //search，找到返回第一次出现 要查找的串 的起始位置，找不到返回beginWriteConst()
            lineEnd = search(buff.peek(), buff.beginWriteConst(), CRLF, CRLF + 2);
            //如果没找到CRLF，也不是BODY，那么一定不完整
            if (lineEnd == buff.beginWrite()) return NO_REQUEST;
            // line: 一行内容 包含换行符\r\n
            line = std::string(buff.peek(), lineEnd);
            buff.retrieveUntil(lineEnd + 2); // 除消息体外，都有换行符
        }
        else {
            // 消息体读取全部内容，同时清空缓存
            _body += buff.retrieveAllToStr();
            if (_body.size() < _contentLen) {
                return NO_REQUEST;
            }
        }
        
        switch(_state) 
        {    
        // 有限状态自动机

        case REQUEST_LINE: {
            HTTP_CODE ret = _parseRequestLine(line);
            if (ret == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            _parsePath();
            break;
        }
        case HEADERS: {
            HTTP_CODE ret = _parseHeader(line);
            // 内部根据content-length字段判断请求完整，提前结束
            if (ret == GET_REQUEST) {
                return GET_REQUEST;
            }
            break;
        }
        case BODY: {
            HTTP_CODE ret = _parseBody();
            if(ret == GET_REQUEST) {
                return GET_REQUEST;
            }
            break;
        }
        default:
            break;
        }
    }
    LOG_DEBUG("state: %d", (int)_state);
    LOG_DEBUG("content length: %d", _contentLen);
    LOG_DEBUG("[%s], [%s], [%s]", _method.c_str(), _path.c_str(), _version.c_str());
    //缓存读空了，但请求还不完整，继续读
    return NO_REQUEST;
}

void HttpRequest::_parsePath() {
    if(_path == "/") {
        _path = "/index.html";
    }
    else if(DEFAULT_HTML.count(_path)){
        _path += ".html";
    }
}

HTTP_CODE HttpRequest::_parseRequestLine(const string& line) {
    regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, pattern)) {
        _method = subMatch[1];
        _path = subMatch[2];
        _version = subMatch[3];
        _state = HEADERS; // 状态转换为下一个状态
        return NO_REQUEST;
    }
    LOG_ERROR("RequestLine Error");
    LOG_DEBUG("%s", line);
    return BAD_REQUEST;
}

HTTP_CODE HttpRequest::_parseHeader(const string& line) {
    regex pattern("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, pattern)) {
        _header[subMatch[1]] = subMatch[2];
        if(subMatch[1] == "Connection") {
            _linger = (subMatch[2] == "keep-alive");
        }
        if(subMatch[1] == "Content-Length") {
            _contentLen = stoi(subMatch[2]);
        }
        return NO_REQUEST;
    }
    else if(_contentLen) {  // content-lenth是请求头最后一行，再过一行空行后，是请求体
        _state = BODY;
        return NO_REQUEST;
    }
    else {
        return GET_REQUEST;
    }
}

HTTP_CODE HttpRequest::_parseBody() {
    if (_method == "POST" && _header["Content-Type"].find("multipart/form-data") != std::string::npos) {
        // 解析 multipart/form-data 格式的数据
        _parseMultipartFormData();
    }
    // 检查是否为 POST 请求，且 Content-Type 为 application/x-www-form-urlencoded
    else if (_method == "POST" && _header["Content-Type"] == "application/x-www-form-urlencoded") {
        // 解析 POST 请求体中的表单数据（URL 编码格式）
        _parseFormUrlencoded();

        // 检查当前请求路径是否是 特定 HTML 页面路径之一
        if (DEFAULT_HTML_TAG.count(_path)) {
            // 获取对应的标签（用于注册或登录页面） 
            int tag = DEFAULT_HTML_TAG.find(_path)->second;
            LOG_DEBUG("Tag:%d", tag);

            // 根据 tag 判断是否为登录页面（tag == 1 表示登录）
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1); // 如果是登录页面，isLogin 设置为 true
                // 验证用户的用户名和密码
                if (userVerify(_post["username"], _post["password"], isLogin)) {
                    // 验证成功，跳转到欢迎页面
                    _path = "/welcome.html";
                } else {
                    // 验证失败，跳转到错误页面
                    _path = "/error.html";
                }
            }
        }
    }
    LOG_DEBUG("Body len:%d",_body.size());
    return GET_REQUEST;
}

int HttpRequest::converHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::_parseFormUrlencoded() {
    if(_body.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = _body.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = _body[i];
        switch (ch) {
        // key
        case '=':
            key = _body.substr(j, i - j);
            j = i + 1;
            break;
        // 键值对中的空格换为+或者%20
        case '+':
            _body[i] = ' ';
            break;
        case '%':
            num = converHex(_body[i + 1]) * 16 + converHex(_body[i + 2]);
            _body[i + 2] = num % 10 + '0';
            _body[i + 1] = num / 10 + '0';
            i += 2;
            break;
        // 键值对连接符
        case '&':
            value = _body.substr(j, i - j);
            j = i + 1;
            _post[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(_post.count(key) == 0 && j < i) {
        value = _body.substr(j, i - j);
        _post[key] = value;
    }
}

// void HttpRequest::_parseFormData()
// {
//     if (_body.size() == 0) return;

//     size_t st = 0, ed = 0;
//     ed = _body.find(CRLF);
//     string boundary = _body.substr(0, ed);

//     // 解析文件信息
//     st = _body.find("filename=\"", ed) + strlen("filename=\"");
//     ed = _body.find("\"", st);
//     _fileInfo["filename"] = _body.substr(st, ed - st);
    
//     // 解析文件内容，文件内容以\r\n\r\n开始
//     st = _body.find("\r\n\r\n", ed) + strlen("\r\n\r\n");
//     ed = _body.find(boundary, st) - 2; // 文件结尾也有\r\n
//     string content = _body.substr(st, ed - st);

//     ofstream ofs;
//     // 如果文件分多次发送，应该采用app，同时为避免重复上传，应该用md5做校验
//     ofs.open("./files/" + _fileInfo["filename"], ios::ate);
//     ofs << content;
//     ofs.close();
// }

void HttpRequest::_parseMultipartFormData() {
    const std::string boundary = "--" + _header["Content-Type"].substr(30); // 从Content-Type中提取boundary
    size_t boundaryPos = 0;
    size_t lastPos = 0;

    // 分割multipart内容并解析每个部分
    while ((boundaryPos = _body.find(boundary, lastPos)) != std::string::npos) {
        lastPos = boundaryPos + boundary.length();
        size_t endPos = _body.find(boundary, lastPos);
        if (endPos == std::string::npos) break;
        // 使用 string_view 指向 _body 中这一段，避免复制
        std::string_view part(_body.data() + lastPos, endPos - lastPos);

        // 如果是图片部分（通过字段名判断）
        if (part.find("Content-Disposition: form-data; name=\"image\"") != std::string::npos) {
            size_t filenamePos = part.find("filename=\"");
            if (filenamePos != std::string::npos) {
                size_t fileStartPos = part.find("\r\n\r\n", filenamePos);
                if (fileStartPos != std::string::npos) {
                    fileStartPos += 4;
                    // 假定结尾有 "\r\n" 两个字节
                    size_t fileDataLen = part.size() - fileStartPos - 2;
                    // 直接引用 _body 内部对应位置的数据
                    _uploadImage = std::string_view(_body.data() + lastPos + fileStartPos, fileDataLen);
                    LOG_DEBUG("Uploaded image data size: %zu", fileDataLen);
                }
            }
        }
        else {
            // 解析文本数据（如性别）
            size_t fieldNamePos = part.find("name=\"") + 6;
            size_t fieldEndPos = part.find("\"", fieldNamePos);
            
            // 使用 std::string_view 提供的 find 和 substr 方法来处理
            std::string_view keyView = part.substr(fieldNamePos, fieldEndPos - fieldNamePos);
            std::string key(keyView);  // 如果需要 std::string，可以将 std::string_view 转换为 std::string
            
            size_t valueStartPos = part.find("\r\n\r\n") + 4;
            std::string_view valueView = part.substr(valueStartPos);

            // 去除value两端的\r\n以及可能的空白字符
            size_t start = valueView.find_first_not_of("\r\n ");
            size_t end = valueView.find_last_not_of("\r\n ");
            
            // 使用 substr 提取真正的值
            valueView = valueView.substr(start, end - start + 1);
            
            // 需要转换为 std::string 时，将 std::string_view 转为 std::string
            std::string value(valueView);

            _post[key] = value;
            LOG_DEBUG("Uploaded text: %s = %s", key.c_str(), value.c_str());
        }

    }
}


void HttpRequest::saveFile(const std::string& fileData) {
    // 假设你希望将文件保存在一个指定的目录下
    std::ofstream outFile("/path/to/save/image.jpg", std::ios::binary);
    outFile.write(fileData.c_str(), fileData.size());
    outFile.close();
}


/**
 * @brief 验证用户登录或注册请求
 *
 * 该函数负责验证用户提交的用户名和密码，既支持登录验证，也支持注册新用户。
 *
 * 函数流程及关键机制：
 * 1. **输入参数检查**  
 *    - 首先检查传入的用户名(name)和密码(pwd)是否为空，若为空则直接返回 false。
 *
 * 2. **日志记录**  
 *    - 使用日志宏(LOG_INFO)记录此次验证操作的用户名和密码，便于调试和后续问题追踪。
 *
 * 3. **数据库连接获取 (RAII & 连接池机制)**  
 *    - 声明一个 MYSQL* 类型的指针，用于指向数据库连接。  
 *    - 使用 SqlConnRAII 对象包装该连接指针，并从 SqlConnPool 单例中获取一个可用的数据库连接。  
 *    - 这里采用了 RAII（Resource Acquisition Is Initialization）技术：  
 *         - 当 SqlConnRAII 对象构造时自动从连接池中取出连接，
 *         - 当该对象析构时自动将连接归还给连接池，从而避免了手动释放连接的麻烦和潜在的资源泄漏。
 *
 * 4. **SQL 查询构造与执行**  
 *    - 定义一个字符数组 order 用于存储 SQL 语句。  
 *    - 构造 SQL 查询语句：从 user 表中查找与给定用户名匹配的记录（只查找一条记录）。  
 *    - 使用 mysql_query 执行查询，如果查询执行失败，则释放结果集（虽然此处 res 可能为空）并返回 false。
 *
 * 5. **结果处理**  
 *    - 调用 mysql_store_result 获取查询的结果集。  
 *    - 获取结果集中字段数及字段信息（字段信息主要用于调试或后续扩展，此处未做深入处理）。
 *    - 通过 while 循环遍历查询返回的记录（MYSQL_ROW）：  
 *         - 如果处于登录模式（isLogin 为 true），则：
 *              - 比较数据库中存储的密码与传入的密码是否一致，若一致则将标记 flag 设为 true，
 *              - 若不一致则记录密码错误日志，并将 flag 保持为 false。
 *         - 如果处于注册模式（isLogin 为 false），则：
 *              - 表明该用户名已存在（用户已经被使用），记录日志并将 flag 设为 false。
 *
 * 6. **释放查询结果**  
 *    - 调用 mysql_free_result 释放结果集资源，避免内存泄露。
 *
 * 7. **注册新用户处理（仅在注册模式下执行）**  
 *    - 如果当前操作是注册（isLogin 为 false）且 flag 为 true（即在查询中未找到同名用户），则执行注册逻辑：  
 *         - 构造插入用户记录的 SQL 语句，将用户名和密码写入 user 表中。
 *         - 执行 mysql_query 插入操作，若插入失败则记录错误日志并将 flag 设为 false。
 *         - 注意：代码中在执行完 mysql_query 后将 flag 强制设为 true，
 *           这可能会掩盖 mysql_query 执行失败的情况（应根据实际情况调整逻辑）。
 *
 * 8. **结束日志与返回结果**  
 *    - 最后记录用户验证成功的日志（无论是登录还是注册），返回 flag 表示验证是否成功。
 *
 * 采用这些机制的原因：
 * - **RAII机制**：自动管理数据库连接资源，保证无论函数如何退出（正常返回或异常），数据库连接都能被正确归还到连接池中。
 * - **连接池**：通过复用数据库连接，降低频繁建立和销毁连接的开销，提高系统的并发处理能力和性能。
 * - **日志记录**：详细的日志可以帮助开发者和运维人员快速定位问题，尤其在并发环境中，日志记录对于追踪问题至关重要。
 *
 * @param name 用户名字符串
 * @param pwd  密码字符串
 * @param isLogin 标识当前操作类型，true 表示登录，false 表示注册
 * @return true 表示验证成功（登录成功或注册成功），false 表示验证失败
 */
bool HttpRequest::userVerify(const string& name, const string& pwd, bool isLogin) {
    if(name == "" || pwd == "") return false;
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    
    // 利用 RAII 机制从数据库连接池获取一个连接，保证函数退出时自动归还连接
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::instance());
    assert(sql);

    bool flag = false; // 标记用户验证是否成功
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    // 注册时，默认认为可以注册（flag 为 true），但如果查询到用户存在则置为 false
    if(!isLogin) flag = true;
    
    // 构造查询语句：根据用户名查找对应的用户记录
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 执行查询操作
    if(mysql_query(sql, order)) {
        // 查询失败时，释放结果集并返回 false
        mysql_free_result(res);
        return false;
    }
    
    // 获取查询结果集
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    // 遍历结果集（通常最多只有一条记录，因为使用了 LIMIT 1）
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        if(isLogin) { // 登录模式下，比较密码是否匹配
            if(pwd == password)
                flag = true;
            else {
                flag = false;
                LOG_INFO("pwd error!");
            }
        }
        else { // 注册模式下，如果查询到记录，说明用户名已存在，不允许注册
            flag = false;
            LOG_INFO("user used!");
        }
    }
    
    // 释放查询结果资源，防止内存泄漏
    mysql_free_result(res);

    // 如果是注册模式且用户名未被使用，则进行用户注册（插入新用户记录）
    if(!isLogin && flag == true) {
        LOG_DEBUG("register!");
        memset(order, 0, sizeof(order));
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if(mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        // 注意：此处无论插入成功与否，flag 最终都会被置为 true，
        // 这可能需要进一步调整以确保插入失败时返回 false。
        flag = true;
    }
    
    LOG_DEBUG("userVerify success!");
    return flag;
}

std::string HttpRequest::path() const{
    return _path;
}

std::string& HttpRequest::path(){
    return _path;
}
std::string HttpRequest::method() const {
    return _method;
}

std::string HttpRequest::version() const {
    return _version;
}

std::string HttpRequest::getPost(const std::string& key) const {
    assert(key != "");
    if(_post.count(key) == 1) {
        return _post.find(key)->second;
    }
    return "";
}

std::string HttpRequest::getPost(const char* key) const {
    assert(key != nullptr);
    if(_post.count(key) == 1) {
        return _post.find(key)->second;
    }
    return "";
}

bool HttpRequest:: isKeepAlive() const {
    if (_header.count("Connection")) return _linger;
    return false;
}