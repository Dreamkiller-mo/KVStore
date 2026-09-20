// v0_test.cpp — agent_kv v0 正确性测试（不依赖任何第三方测试框架）
//
// 运行方式：
//   cmake --preset=release && cmake --build --preset=release
//   ./build/release/tests/v0_test         # 退出码 0 = 全部通过
//   ctest --preset=release                # 或在构建目录里 ctest
//
// 设计原则：先保证正确性，再谈性能。带 UB 的性能数据没有意义，
// 所以这份测试同时也是 ASan/UBSan 跑的那份（见 CMakePresets 的 asan/ubsan）。
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../include/agent_kv/kv_store.h"
#include "../src/engine/memory_engine/memory_kv_store.h"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                  \
  do {                                                               \
    ++g_checks;                                                      \
    if (!(cond)) {                                                   \
      ++g_failures;                                                  \
      std::printf("    FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                                \
  } while (0)

// 断言两个字符串相等，失败时打印实际值（便于定位）
#define CHECK_STR_EQ(actual, expected)                                          \
  do {                                                                          \
    ++g_checks;                                                                 \
    const std::string a_ = (actual);                                            \
    const std::string e_ = (expected);                                          \
    if (a_ != e_) {                                                             \
      ++g_failures;                                                             \
      std::printf("    FAIL  %s:%d  expected \"%s\" got \"%s\"\n", __FILE__,     \
                  __LINE__, e_.c_str(), a_.c_str());                            \
    }                                                                           \
  } while (0)

using agent_kv::api::IKVStore;
using agent_kv::engine::memory_engine::MemoryKVStore;

//------------------------------------------------------------------------------
// 基础语义
//------------------------------------------------------------------------------
void TestBasicPutGet() {
  MemoryKVStore store;
  store.Put("name", "agent_kv");

  auto value = store.Get("name");
  CHECK(value.has_value());
  CHECK_STR_EQ(value.value_or("<nullopt>"), "agent_kv");
  CHECK(store.Size() == 1);
}

void TestMissingKeyReturnsNullopt() {
  MemoryKVStore store;
  CHECK(!store.Get("nope").has_value());
  CHECK(store.Size() == 0);

  store.Put("a", "1");
  CHECK(!store.Get("b").has_value());
  CHECK(!store.Get("").has_value());
  CHECK(store.Size() == 1);
}

void TestOverwriteKeepsOneEntry() {
  MemoryKVStore store;
  store.Put("k", "v1");
  store.Put("k", "v2");
  store.Put("k", "v3");

  CHECK(store.Size() == 1);
  CHECK_STR_EQ(store.Get("k").value_or("<nullopt>"), "v3");
}

void TestDelete() {
  MemoryKVStore store;
  store.Put("k1", "v1");
  store.Put("k2", "v2");

  CHECK(store.Delete("k1"));          // 存在 -> true
  CHECK(!store.Delete("k1"));         // 已删除 -> false
  CHECK(!store.Delete("never"));      // 不存在 -> false
  CHECK(store.Size() == 1);
  CHECK(!store.Get("k1").has_value());
  CHECK(store.Get("k2").has_value());
}

void TestSizeAccounting() {
  MemoryKVStore store;
  CHECK(store.Size() == 0);

  for (int i = 0; i < 100; ++i) {
    store.Put("key" + std::to_string(i), "v");
  }
  CHECK(store.Size() == 100);

  // 覆盖写不改变条目数
  for (int i = 0; i < 100; ++i) {
    store.Put("key" + std::to_string(i), "v2");
  }
  CHECK(store.Size() == 100);

  // 删除一半
  for (int i = 0; i < 50; ++i) {
    CHECK(store.Delete("key" + std::to_string(i)));
  }
  CHECK(store.Size() == 50);

  // 全部删除后归零
  for (int i = 50; i < 100; ++i) {
    store.Delete("key" + std::to_string(i));
  }
  CHECK(store.Size() == 0);
}

//------------------------------------------------------------------------------
// 边界：空 key / 空 value / 超长 key / 超长 value
//------------------------------------------------------------------------------
void TestEmptyKeyAndValue() {
  MemoryKVStore store;
  store.Put("", "");                  // 空 key + 空 value
  CHECK(store.Size() == 1);
  auto v = store.Get("");
  CHECK(v.has_value());
  CHECK(v->empty());

  store.Put("empty-value", "");
  CHECK(store.Get("empty-value").has_value());
  CHECK(store.Get("empty-value")->empty());

  CHECK(store.Delete(""));
  CHECK(store.Size() == 1);
}

void TestLongKeyAndValue() {
  MemoryKVStore store;
  const std::string long_key(4096, 'k');      // 远超 SSO(15字节)
  const std::string long_value(1 << 20, 'v'); // 1 MiB

  store.Put(long_key, long_value);
  auto got = store.Get(long_key);
  CHECK(got.has_value());
  CHECK(got->size() == long_value.size());
  CHECK(*got == long_value);
  CHECK(store.Size() == 1);
}

// 值里含 '\0' 必须原样保存（禁止用 c_str()/strlen 语义的实现）
void TestEmbeddedNull() {
  MemoryKVStore store;
  const std::string key("bin", 3);
  const std::string value("a\0b\0c", 5);

  store.Put(key, value);
  auto got = store.Get(key);
  CHECK(got.has_value());
  CHECK(got->size() == 5);
  CHECK(*got == value);

  // 含 '\0' 的 key 也应正常工作
  const std::string key2("k\0ey", 4);
  store.Put(key2, "v");
  CHECK(store.Get(key2).has_value());
  CHECK(store.Size() == 2);
}

// 前缀 key 不能互相干扰（比较必须用完整长度，而不是 C 字符串比较）
void TestPrefixKeys() {
  MemoryKVStore store;
  store.Put("key", "1");
  store.Put("key1", "2");
  store.Put("key12", "3");
  store.Put("ke", "4");

  CHECK_STR_EQ(store.Get("key").value_or("?"), "1");
  CHECK_STR_EQ(store.Get("key1").value_or("?"), "2");
  CHECK_STR_EQ(store.Get("key12").value_or("?"), "3");
  CHECK_STR_EQ(store.Get("ke").value_or("?"), "4");
  CHECK(!store.Get("k").has_value());
  CHECK(store.Size() == 4);
}

// Put 收到的是 string_view：实现必须自己拷贝，调用方随后改动/销毁源串不能影响结果
void TestValueIsCopied() {
  MemoryKVStore store;
  {
    std::string key = "temp-key";
    std::string value = "temp-value";
    store.Put(key, value);
    key.assign("mutated");
    value.assign("mutated");
  }
  CHECK_STR_EQ(store.Get("temp-key").value_or("?"), "temp-value");
  CHECK(!store.Get("mutated").has_value());
}

//------------------------------------------------------------------------------
// 通过抽象接口使用（多态路径，后续 RocksDB 引擎走的就是这条）
//------------------------------------------------------------------------------
void TestThroughInterface() {
  std::unique_ptr<IKVStore> store = std::make_unique<MemoryKVStore>();

  store->Put("a", "1");
  store->Put("b", "2");
  CHECK(store->Size() == 2);
  CHECK_STR_EQ(store->Get("a").value_or("?"), "1");
  CHECK(store->Delete("a"));
  CHECK(store->Size() == 1);
  CHECK(!store->Get("a").has_value());
}

//------------------------------------------------------------------------------
// 批量往返 + 随机插删（覆盖 rehash 与删除后再插入）
//------------------------------------------------------------------------------
void TestBulkRoundTrip() {
  MemoryKVStore store;
  constexpr std::size_t kN = 100000;

  for (std::size_t i = 0; i < kN; ++i) {
    store.Put("key:" + std::to_string(i), "value:" + std::to_string(i));
  }
  CHECK(store.Size() == kN);

  bool all_ok = true;
  for (std::size_t i = 0; i < kN; ++i) {
    auto v = store.Get("key:" + std::to_string(i));
    if (!v || *v != "value:" + std::to_string(i)) {
      all_ok = false;
      break;
    }
  }
  CHECK(all_ok);

  // 删一半再查
  for (std::size_t i = 0; i < kN; i += 2) {
    CHECK(store.Delete("key:" + std::to_string(i)));
  }
  CHECK(store.Size() == kN / 2);

  // 删除的查不到、留下的还在
  CHECK(!store.Get("key:0").has_value());
  CHECK(store.Get("key:1").has_value());

  // 删掉再插回同一个 key
  store.Put("key:0", "reborn");
  CHECK_STR_EQ(store.Get("key:0").value_or("?"), "reborn");
  CHECK(store.Size() == kN / 2 + 1);
}

// 同一个 key 反复删/插，条目数不应漂移（回归检测）
void TestDeleteInsertCycle() {
  MemoryKVStore store;
  store.Put("hot", "0");
  for (int i = 1; i <= 1000; ++i) {
    CHECK(store.Delete("hot"));
    store.Put("hot", std::to_string(i));
    if (store.Size() != 1) {
      CHECK(store.Size() == 1);
      break;
    }
  }
  CHECK_STR_EQ(store.Get("hot").value_or("?"), "1000");
  CHECK(store.Size() == 1);
}

struct TestCase {
  const char* name;
  void (*fn)();
};

const TestCase kTests[] = {
    {"BasicPutGet", TestBasicPutGet},
    {"MissingKeyReturnsNullopt", TestMissingKeyReturnsNullopt},
    {"OverwriteKeepsOneEntry", TestOverwriteKeepsOneEntry},
    {"Delete", TestDelete},
    {"SizeAccounting", TestSizeAccounting},
    {"EmptyKeyAndValue", TestEmptyKeyAndValue},
    {"LongKeyAndValue", TestLongKeyAndValue},
    {"EmbeddedNull", TestEmbeddedNull},
    {"PrefixKeys", TestPrefixKeys},
    {"ValueIsCopied", TestValueIsCopied},
    {"ThroughInterface", TestThroughInterface},
    {"BulkRoundTrip", TestBulkRoundTrip},
    {"DeleteInsertCycle", TestDeleteInsertCycle},
};

}  // namespace

int main() {
  std::printf("agent_kv v0 correctness tests\n");
  for (const TestCase& t : kTests) {
    const int before = g_failures;
    std::printf("  [ RUN  ] %s\n", t.name);
    t.fn();
    std::printf("  [ %s ] %s\n", (g_failures == before) ? " OK " : "FAIL", t.name);
  }

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures == 0) {
    std::printf("ALL TESTS PASSED\n");
  }
  return g_failures == 0 ? 0 : 1;
}
