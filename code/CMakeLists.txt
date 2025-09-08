set(NET_SRCS net/buffer.cc net/channel.cc net/acceptor.cc net/epoller.cc net/eventloop.cc net/eventloopthread.cc net/eventloopthreadpool.cc net/inetaddress.cc net/socket.cc net/tcpconnection.cc net/tcpserver.cc)
set(HTTP_SRCS http/httpapplication.cc http/httprequest.cc http/httpresponse.cc http/httpcontext.cc http/middleware.cc http/router.cc)
set(LOG_SRCS logging/logger.cc)
set(NN_SRCS nn/detect.cc nn/classify.cc)
set(CONTEXT_SRCS context/context.cc context/executor.cc context/thread_pool.cc)
set(MYSQL_SRCS sql/sqlconnpool.cc)
set(TIMER_SRCS timer/heaptimer.cc)
set(INFERENCE_SRCS inference/boneage_inference.cc)

add_executable(bone_age_server
    ${NN_SRCS}
    ${INFERENCE_SRCS}
    ${NET_SRCS}
    ${HTTP_SRCS}
    ${CONTEXT_SRCS}
    boneageserver.cc
    ${LOG_SRCS}
    ${MYSQL_SRCS}
    # ${TIMER_SRCS}
)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer is enabled.")
    target_compile_options(bone_age_server PRIVATE 
        -fsanitize=address 
        -fno-omit-frame-pointer
    )
    target_link_options(bone_age_server PRIVATE -fsanitize=address)
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ====== thread ======
find_package(Threads REQUIRED)

# ====== opencv ======
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs dnn)
message(STATUS "Found system OpenCV version: ${OpenCV_VERSION}")

# ====== onnuxruntime ======
FetchContent_Declare(
  onnxruntime
  URL "https://github.com/microsoft/onnxruntime/releases/download/v1.18.1/onnxruntime-linux-x64-gpu-1.18.1.tgz"
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(onnxruntime)

add_library(onnxruntime INTERFACE)
target_include_directories(onnxruntime INTERFACE 
  ${onnxruntime_SOURCE_DIR}/include
)
target_link_libraries(onnxruntime INTERFACE 
  ${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so
)

# ====== mysql ======
set(MYSQLCLIENT_INCLUDE_DIRS "/usr/include/mysql")
set(MYSQLCLIENT_LIBRARY_DIRS "/usr/lib/x86_64-linux-gnu")
set(MYSQLCLIENT_LIBS "mysqlclient")

# ====== fmt =======
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.1.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fmt)

# ====== nlohmann/json ======
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ====== spdlog ======
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# ====== gtest ======
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
  GIT_SHALLOW TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ====== CLI11 ======
FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CLI11)

target_link_directories(bone_age_server PRIVATE ${MYSQLCLIENT_LIBRARY_DIRS})

target_link_libraries(bone_age_server 
    PRIVATE 
        fmt::fmt
        onnxruntime 
        nlohmann_json
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_dnn
        Threads::Threads
        spdlog::spdlog
        CLI11::CLI11
        ${MYSQLCLIENT_LIBS}
)

target_include_directories(bone_age_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CUDA_INCLUDE_DIRS}
        ${MYSQLCLIENT_INCLUDE_DIRS}
)

