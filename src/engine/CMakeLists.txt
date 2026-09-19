# 收集内存引擎源码
set(ENGINE_SOURCES
    memory_engine/memory_kv_store.h
    memory_engine/memory_kv_store.cc
)

# 静态库
add_library(agent_kv_lib STATIC ${ENGINE_SOURCES})

# 后续加RocksDB引擎只需要在这里追加文件，和链接库
# target_link_libraries(agent_kv_lib PRIVATE rocksdb)
