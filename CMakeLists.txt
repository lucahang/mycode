cmake_minimum_required(VERSION 3.10) # 声明要求的最低 CMake 版本
project(test)                  # 设置项目名称


set(SRC_LIST
    test.cpp
)

add_executable(exe ${SRC_LIST})       # 告诉 CMake 我们要用 main.cpp 生成一个叫 hello 的可执行文件
