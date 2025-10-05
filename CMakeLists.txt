#设置最低版本和项目名称
cmake_minimum_required(VERSION 3.10)
project(muduo-core) #锁定项目的工作目录

#设置全局的c++标准
set (CMAKE_CXX_STANDARD 17) 
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# 获取当前目录及子目录下所有 .cc 文件
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# 创建动态库
add_library(muduo_core SHARED ${SRC_FILES})

# 设置头文件的路径
target_include_directories(muduo_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# 添加可执行文件示例
# 假设你的 main.cpp 在项目根目录
add_executable(testserver ./example/testserver.cc)

# 链接 muduo_core 库
target_link_libraries(testserver PRIVATE muduo_core)

# 指定动态库运行路径，让 testserver 运行时能找到 libmuduo_core.dylib
set_target_properties(testserver PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)