cmake_minimum_required(VERSION 3.21)  
project(BoneAge_Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

add_subdirectory(code)
# add_subdirectory(ceshi)
# add_subdirectory(tests)