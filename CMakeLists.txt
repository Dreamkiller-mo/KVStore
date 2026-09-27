cmake_minimum_required(VERSION 3.20)
project(AgentKV VERSION 0.1 LANGUAGES CXX)

# ---- 全局配置 ----
set(CMAKE_CXX_STANDARD 20)              # -std=20
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # 编译器不支持c++20 → 配置阶段直接报错，不降级
set(CMAKE_CXX_EXTENSIONS OFF)           # 是否支持拓展
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 配置阶段是否在构建目录下生成 compile_commands.json 文件

# ---- 全局选项 ----
option(AGENTKV_BUILD_TESTS "Build unit tests" ON)
option(AGENTKV_BUILD_SHARED "Build shared lib" OFF)
option(AGENTKV_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# ---- warning lib ----
add_library(agentkv_warnings INTERFACE)
add_library(AgentKV::warnings ALIAS agentkv_warnings)           # 别名 AgentKV::warning

target_compile_options(agentkv_warnings INTERFACE
    # GCC / Clang
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
    # MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    # Werror
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<BOOL:${AGENTKV_WARNINGS_AS_ERRORS}>>:-Werror>          # not MSVC && AGENTKV_WARNINGS_AS_ERRORS
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${AGENTKV_WARNINGS_AS_ERRORS}>>:/WX>                     # MSVC && AGENTKV_WARNINGS_AS_ERRORS
)

# ---- 测试支持 ----
include(CTest)

# ---- Google Benchmark ----
find_package(benchmark CONFIG QUIET)
if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found, fetching from source")
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
        GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
else()
    message(STATUS "Using system Google Benchmark ${benchmark_VERSION}")
endif()

# ---- 构建主库 ----
add_subdirectory(src/engine)

# ---- 单元测试 ----
if(BUILD_TESTING AND AGENTKV_BUILD_TESTS)
    message(STATUS "Building tests: ON")
    add_subdirectory(tests)
else()
    message(STATUS "Building tests: OFF")
endif()

# ---- clangd 支持：自动把编译数据库暴露到工程根目录 ----
# clangd 只会从源文件所在目录逐级向上查找 compile_commands.json
# （外加每一级的 build/compile_commands.json），而 CMake presets 把数据库生成在
# build/<preset>/ 里，clangd 永远进不到那一层。找不到数据库时 clangd 会退化成
# clang 的默认标准（gnu++17），于是 C++20 的异构查找等特性被误报成错误
# ——"编译器能过、clangd 报红"就是这么来的。
# 这里在每次 configure 时自动重建根目录软链接：配置哪个 preset 就指向哪个，
# 不用手工维护（根目录软链接已在 .gitignore 中忽略）。
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    # 加引号是为了避免路径有空格 --> 识别成多个参数
    set(_agentkv_ccdb "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(_agentkv_link "${CMAKE_SOURCE_DIR}/compile_commands.json")
    
    file(REMOVE "${_agentkv_link}")
    file(CREATE_LINK "${_agentkv_ccdb}" "${_agentkv_link}"
         SYMBOLIC RESULT _agentkv_link_result)
    if(_agentkv_link_result STREQUAL "")
        message(STATUS "clangd: compile_commands.json -> ${_agentkv_ccdb}")
    else()
        message(STATUS "clangd: symlink not created (result=${_agentkv_link_result})")
    endif()
endif()
