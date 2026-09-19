cmake_minimum_required(VERSION 3.16)
project(agent_kv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# 公共头文件，全局可用
include_directories(${PROJECT_SOURCE_DIR}/include)

# 递归进入子目录处理
add_subdirectory(src/engine)
add_subdirectory(tests)
add_subdirectory(examples)
