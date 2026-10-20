# benchmark

# Disable Google Benchmark's own tests and install rules.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Fetch Google Benchmark only when KVStore benchmarks are enabled.
FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.10.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(google_benchmark)

add_executable(kvstore_bench
    kvstore_bench.cc
)

target_link_libraries(kvstore_bench
    PRIVATE
        kvstore
        benchmark::benchmark_main
        spdlog::spdlog
)

