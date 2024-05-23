cmake_minimum_required(VERSION 2.5) 
project(mtyThreadPool)

# 设置调试信息 以及 启动C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17 -fPIC")

add_executable(testThreadPool main.cpp threadpool.cpp)


# 编译生成动态库
add_library(myThreadPool SHARED threadpool.cpp)

# 添加目标文件需要链接的库
target_link_libraries(testThreadPool pthread)