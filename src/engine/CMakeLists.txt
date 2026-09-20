# 收集内存引擎源码
set(ENGINE_SOURCES
    memory_engine/memory_kv_store.h
    memory_engine/memory_kv_store.cc
)

# 根据选项切换静态/动态库
if(AGENTKV_BUILD_SHARED)
  add_library(agent_kv_lib SHARED ${ENGINE_SOURCES})
else()
  add_library(agent_kv_lib STATIC ${ENGINE_SOURCES})
endif()

target_compile_features(agent_kv_lib PUBLIC cxx_std_20)

# agent_kv_lib
target_include_directories(
    agent_kv_lib
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} # src/ 目录，内部头文件
)

