#include "gtest/gtest.h"
#include "net/httpresponse.h" // 引入我们要测试的类
#include "net/buffer.h"   // 引入 Buffer 类

using namespace net;

// 辅助函数，用于检查一个字符串是否包含另一个子字符串
void ExpectSubstring(const std::string& str, const std::string& sub) {
    EXPECT_NE(str.find(sub), std::string::npos)
        << "'" << str << "' does not contain '" << sub << "'";
}

// 测试固件，为每个测试用例提供干净的环境
class HttpResponseTest : public ::testing::Test {
protected:
    void SetUp() override {
        response.Init();
    }

    HttpResponse response;
    Buffer buffer;
};

// --- 测试用例 ---

// 测试构建一个基本的 200 OK 响应
TEST_F(HttpResponseTest, BuildsBasic200OKResponse) {
    response.SetStatusCode(200);
    response.SetContentType("text/html");
    response.SetBody("<html><body><h1>Hello</h1></body></html>");
    response.SetKeepAlive(true);

    response.AppendToBuffer(buffer);
    std::string response_str = buffer.RetrieveAllToString();

    // 分别验证各个部分，不再依赖固定顺序
    EXPECT_TRUE(response_str.rfind("<html><body><h1>Hello</h1></body></html>") != std::string::npos);
    ExpectSubstring(response_str, "HTTP/1.1 200 OK\r\n");
    ExpectSubstring(response_str, "Connection: keep-alive\r\n");
    ExpectSubstring(response_str, "Content-Length: 40\r\n");
    ExpectSubstring(response_str, "Content-Type: text/html\r\n");
}

// 测试构建一个 404 Not Found 响应
TEST_F(HttpResponseTest, Builds404NotFoundResponse) {
    response.SetStatusCode(404);
    response.SetStatusMessage("Not Found");
    response.SetContentType("text/plain");
    response.SetBody("The requested resource was not found.");
    response.SetKeepAlive(false);

    response.AppendToBuffer(buffer);
    std::string response_str = buffer.RetrieveAllToString();

    ExpectSubstring(response_str, "HTTP/1.1 404 Not Found\r\n");
    ExpectSubstring(response_str, "Connection: close\r\n");
    ExpectSubstring(response_str, "Content-Length: 37\r\n");
    ExpectSubstring(response_str, "Content-Type: text/plain\r\n");
    EXPECT_TRUE(response_str.rfind("The requested resource was not found.") != std::string::npos);
}

// 测试没有消息体的响应
TEST_F(HttpResponseTest, BuildsResponseWithEmptyBody) {
    response.SetStatusCode(200);
    response.SetKeepAlive(true);

    response.AppendToBuffer(buffer);
    std::string response_str = buffer.RetrieveAllToString();

    ExpectSubstring(response_str, "HTTP/1.1 200 OK\r\n");
    ExpectSubstring(response_str, "Connection: keep-alive\r\n");
    ExpectSubstring(response_str, "Content-Length: 0\r\n");
    // 验证 Body 为空
    EXPECT_EQ(response_str.substr(response_str.find("\r\n\r\n") + 4), "");
}

// 测试自定义 Header
TEST_F(HttpResponseTest, BuildsResponseWithCustomHeaders) {
    response.SetStatusCode(200);
    response.SetHeader("X-Custom-Header", "MyValue");
    response.SetHeader("Server", "MyWebServer");

    response.AppendToBuffer(buffer);
    std::string response_str = buffer.RetrieveAllToString();

    // 这个测试已经很健壮了，保持不变
    ExpectSubstring(response_str, "X-Custom-Header: MyValue\r\n");
    ExpectSubstring(response_str, "Server: MyWebServer\r\n");
    ExpectSubstring(response_str, "Content-Length: 0\r\n");
}

// 测试 Init() 方法的复用性
TEST_F(HttpResponseTest, ResetsCorrectlyForReuse) {
    response.SetStatusCode(404);
    response.SetBody("Error");
    response.SetHeader("X-First-Request", "true");
    response.AppendToBuffer(buffer);
    buffer.RetrieveAll();

    response.Init();
    response.SetBody("OK");
    response.AppendToBuffer(buffer);
    std::string response_str = buffer.RetrieveAllToString();

    ExpectSubstring(response_str, "HTTP/1.1 200 OK\r\n");
    ExpectSubstring(response_str, "Connection: close\r\n");
    ExpectSubstring(response_str, "Content-Length: 2\r\n");
    EXPECT_TRUE(response_str.rfind("OK") != std::string::npos);

    // 验证旧的 Header 已经被清除
    EXPECT_EQ(response_str.find("X-First-Request"), std::string::npos);
    EXPECT_EQ(response_str.find("404"), std::string::npos);
}
