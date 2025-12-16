cmake_minimum_required(VERSION 3.10)
project(gitlite VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录为build文件夹
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 源文件列表
set(SRC_FILES
    src/Utils.cpp
    src/GitliteException.cpp
    src/Repository.cpp
    src/SomeObj.cpp
    main.cpp
)

# 创建可执行文件
add_executable(gitlite ${SRC_FILES})