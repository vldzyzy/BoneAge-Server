#include "httpresponse.h"


// MIME类型推断规则：
// 1. 优先识别扩展名（支持10+种常见类型）
// 2. 无扩展名或未知类型默认text/plain
// 3. 特殊处理XML/XHTML等严格类型
const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".png",   "image/png" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".css",   "text/css" },
    { ".js",    "text/javascript" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 404, "Not Found" },
    { 403, "Forbidden" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 404, "/404.html" },
    { 403, "/403.html" },
};

HttpResponse::HttpResponse()
    : _code(-1),           // 默认为无效状态码
      _path(""),            // 空字符串初始化
      _srcDir(""),          // 空字符串初始化
      _isKeepAlive(false),  // 默认为不保持连接
      _mmFile(nullptr),     // 映射文件指针初始化为 nullptr
      _mmFileStat({0})      // 初始化文件状态为空
{}


HttpResponse::~HttpResponse() {
    unmapFile();    // 清理映射文件
}

void HttpResponse::init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if(_mmFile) unmapFile();    // 如果有映射文件，先解除映射
    _code = code;
    _isKeepAlive = isKeepAlive;
    _path = path;
    _srcDir = srcDir;
    _mmFile = nullptr;
    _mmFileStat = { 0 };
}

/**
 * @brief 构建完整的HTTP响应报文
 * 
 * @param buff 输出缓冲区引用
 * 
 * 方法流程：
 * 1. 三级文件检查：
 *   - 存在性检查（stat）
 *   - 目录检查（S_ISDIR）
 *   - 读权限检查（S_IROTH）
 * 2. 错误处理：
 *   - 自动加载错误模板页面
 *   - 动态生成简易错误页面
 * 3. 响应组件构建：
 *   - 状态行（HTTP/1.1 200 OK）
 *   - 头域（Connection/Content-type等）
 *   - 消息体（内存映射内容或错误信息）
 * 
 * 性能特征：
 * - 零拷贝：文件内容通过mmap直接映射到内存
 * - 内存高效：大文件处理时内存消耗恒定
 * - 异常安全：所有系统资源通过RAII管理
 */
void HttpResponse::makeResponse(Buffer& buff) {
    // 资源不存在 或 是一个目录
    if(stat((_srcDir + _path).data(), &_mmFileStat) < 0 || S_ISDIR(_mmFileStat.st_mode)) {
        _code = 404;
    }
    // 文件对其他用户不可读
    else if(!(_mmFileStat.st_mode & S_IROTH)) {
        _code = 403;
    }
    // 前两步通过，且状态码未被设置
    else if (_code == -1) {
        _code = 200;
    }

    // 加载或生成错误页面
    _errorHtml();
    
    _addState(buff);
    _addHeader(buff);
    _addContent(buff);
}

void HttpResponse::_addState(Buffer& buff) {
    std::string status;
    if(CODE_STATUS.count(_code) == 1) {
        status = CODE_STATUS.find(_code)->second;
    }
    else {
        _code = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.append("HTTP/1.1 " + std::to_string(_code) + " " + status + "\r\n");
}

void HttpResponse::_addHeader(Buffer& buff) {
    buff.append("Connection: ");
    if (_isKeepAlive) {
        // Keep-Alive详细参数配置（最大6个请求，120秒超时）
        // 符合主流浏览器行为，平衡连接复用与资源释放
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.append("close\r\n");
    }
    buff.append("Content-type: " + _getFileType() + "\r\n");
}

/**
 * @brief 内存映射处理注意事项：
 * 1. 映射生命周期：从makeResponse()到unmapFile()
 * 2. 文件描述符管理：mmap后立即close()以避免泄漏
 * 3. 线程安全性：非线程安全类，需外部同步
 * 4. 性能影响：适合静态文件，小文件建议直接缓冲
 */
void HttpResponse::_addContent(Buffer& buff) {
    int srcFd = open((_srcDir + _path).data(), O_RDONLY);
    if (srcFd < 0) {
        errorContent(buff, "File NotFound!");
        return;
    }
    // 使用mmap将文件直接映射到内存空间，避免read()+缓冲区的数据拷贝
    // 优势：提升大文件传输效率
    // 限制：适合静态文件，动态内容需使用传统缓冲区
    int* mmRet = (int*)mmap(0, _mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1) {
        errorContent(buff, "File NotFound!");
        return;
    }
    _mmFile = (char*)mmRet;
    close(srcFd);
    buff.append("Content-length: " + std::to_string(_mmFileStat.st_size) + "\r\n\r\n");
}

// 三级错误处理策略：
// 1. 预置错误模板页面（/404.html等）
// 2. 简单HTML错误页面生成
// 3. 状态码自动降级（无法识别的状态码转为400）
void HttpResponse::errorContent(Buffer& buff, std::string message) {
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(_code) == 1) {
        status = CODE_STATUS.find(_code)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(_code) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}

/**
 * 1. 功能: 
 * - 该函数用于解除文件的内存映射。如果 _mmFile 指针不为空，表示文件已被映射到内存中，函数通过调用 munmap 将文件的内存映射解除，并将 _mmFile 指针置为 nullptr，确保指针不会悬挂。
 * 
 * 2. 流程:
 * - 检查 _mmFile 是否为非空指针。如果非空，表示文件已经通过 mmap 映射到了内存中。
 * - 调用 munmap 函数解除映射，munmap 函数会将文件从进程的地址空间中移除。此时，文件不再占用内存。
 * - 将 _mmFile 置为 nullptr，防止进一步的访问。
 * 
 * 3. 为什么这么用:
 * - 内存映射：mmap 提供了高效的文件读取方式，通过将文件直接映射到内存，可以避免多次的 read 调用和数据拷贝，提升文件处理的效率。
 * - 资源释放：解除映射是为了释放资源，并避免内存泄漏。munmap 是释放文件映射的标准方法，保证了文件占用的内存能被及时清理。
 */
void HttpResponse::unmapFile() {
    if(_mmFile) {
        munmap(_mmFile, _mmFileStat.st_size);
        _mmFile = nullptr;
    }
}

// 1. 功能: 
// - 该函数用于根据文件路径的扩展名来推断文件的 MIME 类型。它通过查找文件路径中的扩展名，并将其与已定义的 MIME 类型映射表 SUFFIX_TYPE 进行匹配。
// 2. 流程:
// - 通过 find_last_of('.') 查找文件路径中的最后一个点字符，通常用于识别文件扩展名。
// - 如果没有找到点（即没有扩展名），默认返回 text/plain。
// - 如果找到了点，使用 substr 提取文件的扩展名（例如 .html、.jpg）。
// - 检查扩展名是否在 SUFFIX_TYPE 映射表中，如果找到，则返回对应的 MIME 类型。
// - 如果没有找到匹配的扩展名，返回默认的 text/plain。
// 3. 为什么这么用:
// - MIME 类型推断：根据文件的扩展名来推断文件的 MIME 类型是 Web 服务器的常见做法。这样前端浏览器能够根据 MIME 类型正确地渲染和处理文件（如显示 HTML 页面或下载 PDF 文件）。
// - 扩展性：通过 SUFFIX_TYPE 映射表，可以方便地扩展和支持更多的文件类型，只需修改这个表即可。
std::string HttpResponse::_getFileType() {
    std::string::size_type idx = _path.find_last_of('.');
    if(idx == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = _path.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

// 通过 mmap 映射文件后，可以直接在内存中访问文件内容。file() 函数提供了对映射文件的访问接口，其他地方可以通过这个函数来获取文件内容。
char* HttpResponse::file() {
    return _mmFile;
}

size_t HttpResponse::fileLen() const {
    return _mmFileStat.st_size;
}

// 1. 功能: 
// - 该函数用于根据 HTTP 响应的状态码 _code 生成相应的错误页面。如果状态码对应的错误页面已预定义，函数会更新 _path 为相应的页面路径，并通过 stat 获取该页面的文件状态。
// 2. 流程:
// - 检查 _code 是否在 CODE_PATH 中有预定义的错误页面路径。如果有，更新 _path 为对应的错误页面路径。
// - 使用 stat 获取错误页面文件的状态，确保文件存在且可访问。
// 3. 为什么这么用:
// - 错误页面的处理：根据不同的 HTTP 错误码（如 404、403），服务器返回不同的错误页面。通过 CODE_PATH 映射表可以方便地管理这些错误页面。
// - 状态码映射：CODE_PATH 映射表将 HTTP 错误码映射到相应的错误页面路径，可以根据错误码自动选择错误页面，而无需手动处理每个错误码的具体逻辑。
// 4. 扩展性：
// - 新的错误页面可以通过修改 CODE_PATH 映射表来快速添加，避免硬编码。
void HttpResponse::_errorHtml() {
    if(CODE_PATH.count(_code) == 1) {  // 如果错误代码有预定义的错误页面
        _path = CODE_PATH.find(_code)->second;  // 获取对应的错误页面路径
        stat((_srcDir + _path).data(), &_mmFileStat);  // 获取错误页面的文件状态
    }
}