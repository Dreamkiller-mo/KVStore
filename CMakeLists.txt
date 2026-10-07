# set minimum version
cmake_minimum_required(VERSION 3.28)

project(KVStore
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Project-wide dependencies
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY git@github.com:gabime/spdlog.git
    GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(spdlog)

FetchContent_MakeAvailable(spdlog)
# recurse into subdirectories
add_subdirectory(src)

# add_subdirectory(tests)
add_subdirectory(examples)
