# 编译选项
CXXFLAGS = -std=c++17 \
           -Wall \
		   -g \
           -pthread \

# 链接选项
LDFLAGS = -lmysqlclient

# 设置源文件
SRCS = $(wildcard ../code/buffer/*.cpp) \
       $(wildcard ../code/http/*.cpp) \
       $(wildcard ../code/log/*.cpp) \
       $(wildcard ../code/pool/*.cpp) \
       $(wildcard ../code/server/*.cpp) \
	   $(wildcard ../code/timer/*.cpp) \
       ../code/test/server.cpp

# -pthread 一个编译器/链接器选项，告诉编译器启用线程支持，并自动处理链接线程库的细节。
# -lmysqlclient 链接 MySQL 客户端库。 -l 前缀告诉链接器（ld）去查找并链接名为 libmysqlclient.so（Linux）或 libmysqlclient.a（静态库）的库文件。

# 设置目标文件
OBJS = $(SRCS:.cpp=.o)

# 可执行文件的名字
TARGET = server.out

# 默认目标
all: $(TARGET)

# 生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o ../bin/$(TARGET) $(OBJS) $(LDFLAGS)

# 生成其他目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) ../bin/$(TARGET)

# 重新生成
rebuild: clean all
