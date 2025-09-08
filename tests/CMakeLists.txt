
# ====== httprequest ======
# add_executable(test
#     test_httprequest.cc
#     ${PROJECT_SOURCE_DIR}/code/net/httprequest.cc
# )

# target_link_libraries(
#     test
#     PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(
#     test
#     PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== httpresponse ======

# add_executable(test
#     test_httpresponse.cc
#     ${PROJECT_SOURCE_DIR}/code/net/httpresponse.cc
# )

# target_link_libraries(
#     test
#     PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(
#     test
#     PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== buffer ======

# add_executable(test
#     test_buffer.cc
#     ${PROJECT_SOURCE_DIR}/code/net/buffer.cc
# )

# target_link_libraries(
#     test
#     PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(
#     test
#     PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== inetaddress ======

# add_executable(test
#     test_inetaddress.cc
#     ${PROJECT_SOURCE_DIR}/code/net/inetaddress.cc
# )

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== socket ======

# add_executable(test
#     test_socket.cc
#     ${PROJECT_SOURCE_DIR}/code/net/socket.cc
#     ${PROJECT_SOURCE_DIR}/code/net/inetaddress.cc
# )

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== eventloop ======

# add_executable(test
#     test_eventloop.cc
#     ${PROJECT_SOURCE_DIR}/code/net/epoller.cc
#     ${PROJECT_SOURCE_DIR}/code/net/channel.cc
#     ${PROJECT_SOURCE_DIR}/code/net/eventloop.cc
# )

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== acceptor ======

# add_executable(test
#     test_acceptor.cc
#     ${PROJECT_SOURCE_DIR}/code/net/epoller.cc
#     ${PROJECT_SOURCE_DIR}/code/net/channel.cc
#     ${PROJECT_SOURCE_DIR}/code/net/eventloop.cc
#     ${PROJECT_SOURCE_DIR}/code/net/acceptor.cc
#     ${PROJECT_SOURCE_DIR}/code/net/inetaddress.cc
#     ${PROJECT_SOURCE_DIR}/code/net/socket.cc
# )

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# ====== eventloopthread ======

# add_executable(test
#     test_eventloopthread.cc
#     ${PROJECT_SOURCE_DIR}/code/net/buffer.cc
#     ${PROJECT_SOURCE_DIR}/code/net/epoller.cc
#     ${PROJECT_SOURCE_DIR}/code/net/channel.cc
#     ${PROJECT_SOURCE_DIR}/code/net/eventloop.cc
#     ${PROJECT_SOURCE_DIR}/code/net/acceptor.cc
#     ${PROJECT_SOURCE_DIR}/code/net/inetaddress.cc
#     ${PROJECT_SOURCE_DIR}/code/net/socket.cc
#     ${PROJECT_SOURCE_DIR}/code/net/eventloopthread.cc
#     ${PROJECT_SOURCE_DIR}/code/net/eventloopthreadpool.cc
# )

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== tcpconnection ======

# add_executable(test
#     test_tcpconnection.cc
#     ${PROJECT_SOURCE_DIR}/code/net/buffer.cc
#     ${PROJECT_SOURCE_DIR}/code/net/epoller.cc
#     ${PROJECT_SOURCE_DIR}/code/net/channel.cc
#     ${PROJECT_SOURCE_DIR}/code/net/eventloop.cc
#     ${PROJECT_SOURCE_DIR}/code/net/acceptor.cc
#     ${PROJECT_SOURCE_DIR}/code/net/inetaddress.cc
#     ${PROJECT_SOURCE_DIR}/code/net/socket.cc
#     ${PROJECT_SOURCE_DIR}/code/net/tcpconnection.cc
# )

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
# )

# # ====== sqlconnpool ======

# add_executable(test
#     test_sqlconnpool.cc
#     ${PROJECT_SOURCE_DIR}/code/sql/sqlconnpool.cc
#     ${PROJECT_SOURCE_DIR}/code/logging/logger.cc
# )

# set(MYSQLCLIENT_INCLUDE_DIRS "/usr/include/mysql")
# set(MYSQLCLIENT_LIBRARY_DIRS "/usr/lib/x86_64-linux-gnu")
# set(MYSQLCLIENT_LIBS "mysqlclient")

# target_link_directories(test PRIVATE ${MYSQLCLIENT_LIBRARY_DIRS})

# target_link_libraries(test PRIVATE
#     GTest::gtest_main
#     spdlog::spdlog
#     ${MYSQLCLIENT_LIBS}
# )

# target_include_directories(test PRIVATE
#     ${PROJECT_SOURCE_DIR}/code
#     ${MYSQLCLIENT_INCLUDE_DIRS}
# )

# ====== yolo ======

add_executable(test
    test_yolo.cc
    ${PROJECT_SOURCE_DIR}/code/nn/yolo11_detector.cc
    ${PROJECT_SOURCE_DIR}/code/utils/utils.cc
)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs dnn)
message(STATUS "Found system OpenCV version: ${OpenCV_VERSION}")

target_link_libraries(test PRIVATE
    onnxruntime
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_dnn
)

target_include_directories(test PRIVATE
    ${PROJECT_SOURCE_DIR}/code
    
)