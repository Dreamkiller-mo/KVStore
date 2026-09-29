# set minimum version
cmake_minimum_required(VERSION 3.28)

# define project : name + version + languages
project(kvstore
    VERSION 0.1.0
    LANGUAGES CXX
)

# define c++ 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# load dependence
include(FetchContent)

FetchContent_Declare(
    absl
    GIT_REPOSITORY git@github.com:abseil/abseil-cpp.git
    GIT_TAG 20260817.0
)

FetchContent_MakeAvailable(absl)

# recurse into subdirectories
add_subdirectory(src)

# add_subdirectory(tests)
add_subdirectory(examples)
