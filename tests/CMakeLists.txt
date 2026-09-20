set(TEST_SOURCES 
    v0_benchmark.cc
)

find_package(benchmark REQUIRED)

add_executable(agent_kv_test ${TEST_SOURCES} )

target_link_libraries(
    agent_kv_test PRIVATE 
    agent_kv_lib
    benchmark::benchmark
    benchmark::benchmark_main
)

add_test(NAME AgentKVTest COMMAND agent_kv_test)

target_compile_features(agent_kv_lib PUBLIC cxx_std_20)