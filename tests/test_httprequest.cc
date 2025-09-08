#include "gtest/gtest.h"
#include "net/httprequest.h" // 引入我们要测试的类
#include "net/buffer.h"   // 引入 Buffer 类

using namespace net;

// 测试固件，为每个测试用例提供干净的环境
class HttpRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前，都会调用 Init() 来重置 request 对象
        request.Init();
    }

    Buffer buffer;
    HttpRequest request;
};

// --- 测试用例 ---

// 测试一个最基本的 GET 请求
TEST_F(HttpRequestTest, ParseSimpleGetRequest) {
    buffer.Append("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
    
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);
    EXPECT_EQ(request.GetMethod(), "GET");
    EXPECT_EQ(request.GetPath(), "/index.html");
    EXPECT_EQ(request.GetVersion(), "HTTP/1.1");
    EXPECT_TRUE(request.IsKeepAlive());
    EXPECT_EQ(buffer.ReadableBytes(), 0);
}

// 测试一个带有 Connection: close 的 GET 请求
TEST_F(HttpRequestTest, ParseGetRequestWithConnectionClose) {
    buffer.Append("GET / HTTP/1.1\r\nConnection: close\r\n\r\n");

    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);
    EXPECT_EQ(request.GetMethod(), "GET");
    EXPECT_EQ(request.GetPath(), "/");
    EXPECT_FALSE(request.IsKeepAlive());
}

// 测试 HTTP/1.0 的 Keep-Alive 逻辑
TEST_F(HttpRequestTest, ParseHttp10KeepAlive) {
    buffer.Append("GET /path HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
    
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);
    EXPECT_TRUE(request.IsKeepAlive());
}

// 测试 Header Key 的大小写不敏感性
TEST_F(HttpRequestTest, ParseHeaderCaseInsensitive) {
    buffer.Append("GET / HTTP/1.1\r\nCONTENT-length: 12\r\n\r\nHello World!");
    
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);
    auto headers = request.GetHeaders();
    // Key 应该被转为小写
    EXPECT_TRUE(headers.count("content-length"));
    EXPECT_EQ(headers.at("content-length"), "12");
}

// 测试一个简单的 POST 请求
TEST_F(HttpRequestTest, ParseSimplePostRequest) {
    const char* post_request = "POST /login HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Content-Length: 27\r\n"
                               "\r\n"
                               "username=admin&password=123";
    buffer.Append(post_request);

    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);
    EXPECT_EQ(request.GetMethod(), "POST");
    EXPECT_EQ(request.GetPath(), "/login");
    EXPECT_EQ(request.GetBody(), "username=admin&password=123");
    EXPECT_TRUE(request.IsKeepAlive());
}

// 测试分块到达的请求（半包）
TEST_F(HttpRequestTest, ParsePartialRequest) {
    // 第一次只收到请求行
    buffer.Append("GET /partial HTTP/1.1\r\n");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kNoRequest);

    // 第二次收到 Headers
    buffer.Append("Host: example.com\r\nConnection: Close\r\n\r\n");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);

    EXPECT_EQ(request.GetMethod(), "GET");
    EXPECT_EQ(request.GetPath(), "/partial");
    EXPECT_FALSE(request.IsKeepAlive()); // Connection: Close
}

// 测试分块到达的 POST Body
TEST_F(HttpRequestTest, ParsePartialPostBody) {
    buffer.Append("POST /submit HTTP/1.1\r\nContent-Length: 10\r\n\r\n");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kNoRequest);

    // Body 分两次到达
    buffer.Append("abcde");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kNoRequest);
    EXPECT_EQ(request.GetBody(), "abcde");

    buffer.Append("fghij");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kGetRequest);
    EXPECT_EQ(request.GetBody(), "abcdefghij");
}

// 测试格式错误的请求行
TEST_F(HttpRequestTest, ParseBadRequestLine) {
    buffer.Append("THIS IS NOT A VALID REQUEST\r\n\r\n");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kBadRequest);
}

// 测试格式错误的 Header
TEST_F(HttpRequestTest, ParseBadHeader) {
    buffer.Append("GET / HTTP/1.1\r\nInvalid Header\r\n\r\n");
    ASSERT_EQ(request.Parse(buffer), HttpRequest::HttpCode::kBadRequest);
}