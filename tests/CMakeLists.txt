# 性能测试
add_executable(v0_bench v0_benchmark.cpp)
target_link_libraries(v0_bench PRIVATE agent_kv_lib)
