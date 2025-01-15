# 定义编译器
CC = g++

# 头文件
INCLUDE = -I../code/log \
 		  -I../code/util \
		  -I../code/pool \
		  -I../code/server \
		  -I../code/buffer \

# 定义编译选项，指定包含路径
CFLAGS = -std=c++17 -Wall -Wextra -pthread -lmysqlclient $(INCLUDE) 

# 定义可执行文件名称
TARGET = main.out

# 定义源文件路径
SRCS = ../code/main.cpp \
       ../code/log/log.cpp \
       ../code/buffer/buffer.cpp\

# 生成目标文件名
OBJS = $(SRCS:.cpp=.o)

# 编译规则
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# 通用规则将.cpp文件编译为.o文件
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(OBJS) $(TARGET)