cmake_minimum_required(VERSION 3.15)  # 提升CMake版本要求
project(BoneAge_Server.out VERSION 0.0.1 LANGUAGES C CXX CUDA)  # 直接添加 CUDA 支持

# 禁用优化并启用调试
add_compile_options(-g -O0)

# ========== 基础配置 ==========
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ========== OpenCV配置 ==========
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# ========== CUDA配置 ==========
option(USE_CUDA "Enable CUDA support" ON)
if(USE_CUDA AND NOT APPLE)
    # 显式查找 CUDA Toolkit
    find_package(CUDAToolkit REQUIRED)
    
    # 启用 CUDA 语言支持
    enable_language(CUDA)
    
    # 添加 CUDA 包含路径和库
    include_directories(${CUDAToolkit_INCLUDE_DIRS})
    add_definitions(-DUSE_CUDA)
    
    # 确保 CMake 知道需要链接 CUDA 运行时
    list(APPEND CUDA_LIBRARIES cudart)
endif()

# ========== ONNX Runtime配置 ==========
set(ONNXRUNTIME_ROOT "/usr/local")  # 根据实际安装路径修改

include_directories(${PROJECT_NAME} ${ONNXRUNTIME_ROOT}/include)

# ========== MySQL 配置 ==========
# 手动指定头文件和库路径（根据实际路径修改）
find_library(MySQL_CLIENT_LIB
  NAMES mysqlclient
  PATHS /usr/lib/x86_64-linux-gnu 
  REQUIRED
  NO_DEFAULT_PATH
)

# ========== 可执行文件配置 ==========
# 自动收集各模块的源文件
file(GLOB ONNX_SOURCES "code/onnx/*.cpp")
file(GLOB LOG_SOURCES "code/log/*.cpp")
file(GLOB BUFFER_SOURCES "code/buffer/*.cpp")
file(GLOB HTTP_SOURCES "code/http/*.cpp")
file(GLOB JSON_SOURCES "code/json/*.cpp" "code/json/*.c")
file(GLOB SERVER_SOURCES "code/server/*.cpp")
file(GLOB TIMER_SOURCES "code/timer/*.cpp")
file(GLOB POOL_SOURCES "code/pool/*.cpp")
file(GLOB MAIN_SOURCES "code/main/server.cpp")

file(GLOB TEST_SOURCES "code/onnx/test.cpp" "code/onnx/BoneAgeCls.cpp")

add_executable(${PROJECT_NAME}
    ${ONNX_SOURCES}
    ${LOG_SOURCES}
    ${BUFFER_SOURCES}
    ${HTTP_SOURCES}
    ${JSON_SOURCES}
    ${SERVER_SOURCES}
    ${TIMER_SOURCES}
    ${POOL_SOURCES}
    ${MAIN_SOURCES}
)

# add_executable(${PROJECT_NAME}
#     ${JSON_SOURCES}
#     ${TEST_SOURCES}
# )

target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    ${MySQL_CLIENT_LIB}
    ${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so
    CUDA::cudart  # 使用现代 CMake 目标名称
)

# ========== 后处理配置 ==========
# 在 CUDA 配置部分后添加验证
message(STATUS "CUDA Toolkit version: ${CUDAToolkit_VERSION}")
message(STATUS "CUDA include dirs: ${CUDAToolkit_INCLUDE_DIRS}")
message(STATUS "CUDA libraries: ${CUDA_LIBRARIES}")
message(STATUS "JSON_SOURCES: ${JSON_SOURCES}")