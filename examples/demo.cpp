// demo.cpp — agent_kv 最小使用示例（演示如何通过抽象接口使用存储引擎）
//
//   cmake --preset=release && cmake --build --preset=release
//   ./build/release/examples/kv_demo
#include <cstdio>
#include <memory>

#include "../include/agent_kv/kv_store.h"
#include "../src/engine/memory_engine/memory_kv_store.h"

int main() 
{
  using agent_kv::api::IKVStore;
  using agent_kv::engine::memory_engine::MemoryKVStore;

  // 调用方只依赖抽象接口：以后换成 RocksDB 引擎不需要改这里的代码
  std::unique_ptr<IKVStore> db = std::make_unique<MemoryKVStore>();

  db->Put("name", "agent_kv");
  db->Put("version", "v0");
  db->Put("engine", "memory_engine");
  std::printf("写入 3 条后 Size() = %zu\n\n", db->Size());

  for (const char* key : {"name", "version", "engine", "missing"}) {
    auto value = db->Get(key);
    std::printf("  Get(%-9s) -> %s\n", key, value ? value->c_str() : "(nullopt)");
  }

  std::printf("\nDelete(\"version\") = %s\n", db->Delete("version") ? "true" : "false");
  std::printf("Delete(\"version\") = %s   (重复删除)\n",
              db->Delete("version") ? "true" : "false");
  std::printf("Size() = %zu\n", db->Size());
  return 0;
}
