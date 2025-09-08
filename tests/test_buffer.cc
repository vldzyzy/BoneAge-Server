#include "net/buffer.h"
#include <gtest/gtest.h>

using namespace net;

// 使用 Test Fixture 来为一组测试共享配置和代码
class BufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试用例开始前都会执行
    }

    void TearDown() override {
        // 每个测试用例结束后都会执行
    }

    Buffer buffer_;
};

// 测试1：初始状态
TEST_F(BufferTest, InitialState) {
    ASSERT_EQ(buffer_.ReadableBytes(), 0);
    // 默认构造大小是 1024
    ASSERT_EQ(buffer_.WriteableBytes(), 1024);
    ASSERT_EQ(buffer_.PrependableBytes(), 0);
}

// 测试2：Append 和 Retrieve
TEST_F(BufferTest, AppendAndRetrieve) {
    std::string str1 = "hello";
    buffer_.Append(str1);
    ASSERT_EQ(buffer_.ReadableBytes(), 5);
    ASSERT_EQ(buffer_.WriteableBytes(), 1024 - 5);
    ASSERT_EQ(buffer_.PrependableBytes(), 0);

    // 检查 Peek
    ASSERT_EQ(std::string(buffer_.Peek(), buffer_.ReadableBytes()), "hello");

    // 取出部分数据
    buffer_.Retrieve(3);
    ASSERT_EQ(buffer_.ReadableBytes(), 2);
    ASSERT_EQ(buffer_.WriteableBytes(), 1024 - 5);
    // PrependableBytes 应该等于取出的长度
    ASSERT_EQ(buffer_.PrependableBytes(), 3);
    ASSERT_EQ(std::string(buffer_.Peek(), buffer_.ReadableBytes()), "lo");

    // 再次 Append
    std::string str2 = " world";
    buffer_.Append(str2);
    ASSERT_EQ(buffer_.ReadableBytes(), 2 + str2.length());
    ASSERT_EQ(std::string(buffer_.Peek(), buffer_.ReadableBytes()), "lo world");
}

// 测试3：RetrieveAllToString
TEST_F(BufferTest, RetrieveAllToString) {
    std::string str = "all data";
    buffer_.Append(str);
    ASSERT_EQ(buffer_.ReadableBytes(), str.length());

    std::string retrieved_str = buffer_.RetrieveAllToString();
    ASSERT_EQ(retrieved_str, str);

    // RetrieveAll 之后，buffer 应该被重置
    ASSERT_EQ(buffer_.ReadableBytes(), 0);
    ASSERT_EQ(buffer_.WriteableBytes(), 1024); // 容量不变
    ASSERT_EQ(buffer_.PrependableBytes(), 0);
}

// 测试4：内部腾挪 (MakeSpace_ 的优化路径)
TEST_F(BufferTest, InternalMove) {
    // 构造一个 prependable 空间很大的场景
    buffer_.Append(std::string(800, 'x')); // 写入800字节
    buffer_.Retrieve(500); // 取出500字节，留下300字节可读，500字节可预留
    
    ASSERT_EQ(buffer_.ReadableBytes(), 300);
    ASSERT_EQ(buffer_.WriteableBytes(), 1024 - 800);
    ASSERT_EQ(buffer_.PrependableBytes(), 500);
    
    // 此时 PrependableBytes + WriteableBytes = 500 + 224 = 724
    
    // Append 的数据 len > WriteableBytes，但 len < (PrependableBytes + WriteableBytes)
    std::string str(400, 'y');
    buffer_.Append(str);

    // 此时应该发生内部腾挪，而不是 realloc
    ASSERT_EQ(buffer_.ReadableBytes(), 300 + 400);
    ASSERT_EQ(buffer_.PrependableBytes(), 0); // 腾挪后，prependable 空间被回收
    
    // 验证数据正确性
    std::string expected = std::string(300, 'x') + std::string(400, 'y');
    ASSERT_EQ(buffer_.RetrieveAllToString(), expected);
}

// 测试5：扩容 (MakeSpace_ 的 resize 路径)
TEST_F(BufferTest, Grow) {
    std::string str(1000, 'a');
    buffer_.Append(str);
    ASSERT_EQ(buffer_.ReadableBytes(), 1000);
    ASSERT_EQ(buffer_.WriteableBytes(), 24);

    // Append 的数据 > WriteableBytes + PrependableBytes
    std::string str2(50, 'b');
    buffer_.Append(str2);

    // 此时必须扩容
    ASSERT_EQ(buffer_.ReadableBytes(), 1050);
    // 扩容后的 WriteableBytes 至少为 0
    ASSERT_GE(buffer_.WriteableBytes(), 0); 
    
    // 验证数据正确性
    std::string expected = str + str2;
    ASSERT_EQ(buffer_.RetrieveAllToString(), expected);
}

// 测试6：EnsureWriteable 的功能
TEST_F(BufferTest, EnsureWriteable) {
    ASSERT_EQ(buffer_.WriteableBytes(), 1024);
    buffer_.EnsureWriteable(1200);
    // 调用后，可写空间必须大于等于请求的空间
    ASSERT_GE(buffer_.WriteableBytes(), 1200);
}