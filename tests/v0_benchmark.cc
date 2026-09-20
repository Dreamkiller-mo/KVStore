// tests/v0_benchmark.cc — AgentKV V0 压测（Google Benchmark）
//
// 运行：
//   cmake --preset=release && cmake --build --preset=release
//   ./build/release/v0_benchmark
//   ./build/release/v0_benchmark --benchmark_filter=BM_Put
//
// 说明：Google Benchmark 没有 `->Fixture(setup, teardown)` 这个 API，
// 全局测试数据用"惰性初始化的函数内静态变量"来准备（见 Data()），
// 效果等价：所有用例共用同一份预生成数据，且不计入被测时间。

#include "../src/engine/memory_engine/memory_kv_store.h"
#include <benchmark/benchmark.h>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

using namespace agent_kv;

// 辅助函数：生成指定长度的随机字符串
std::string GenerateRandomString(size_t length, std::mt19937& rng) 
{
    //随机数组
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    auto dist = std::uniform_int_distribution<>(0, sizeof(charset) - 2);
    
    std::string str;
    str.reserve(length);
    for (size_t i = 0; i < length; ++i) 
    {
        str.push_back(charset[dist(rng)]);
    }

    return str;
}

// 预生成测试数据，避免测试时字符串生成的耗时影响测量
struct TestData 
{
    std::vector<std::string> keys;
    std::vector<std::string> values;

    TestData(size_t num_ops, size_t key_len, size_t value_len) 
    {
        std::mt19937 rng(std::random_device{}());

        keys.reserve(num_ops);
        values.reserve(num_ops);
        for (size_t i = 0; i < num_ops; ++i) 
        {
            keys.push_back(GenerateRandomString(key_len, rng));
            values.push_back(GenerateRandomString(value_len, rng));
        }
    }
};

static constexpr size_t kNumOps = 1000000;   // 100 万个 Key/Value
static constexpr size_t kKeyLen = 16;        // Key 16 字节
static constexpr size_t kValueLen = 128;     // Value 128 字节

// 全局测试数据（只构造一次；构造耗时不算进任何用例）
static TestData& Data() 
{
    static TestData data(kNumOps, kKeyLen, kValueLen);
    return data;
}

// --- 1. 测试 Put 性能 ---
static void BM_Put(benchmark::State& state) 
{
    TestData& data = Data();
    auto kv = std::make_unique<engine::memory_engine::MemoryKVStore>();
    size_t i = 0;
    for (auto _ : state) 
    {
        kv->Put(data.keys[i], data.values[i]);
        benchmark::DoNotOptimize(kv); // 防止编译器优化掉
        i = (i + 1) % kNumOps;
    }

    // 设置统计指标
    state.SetItemsProcessed(state.iterations());
    // 可以自定义统计指标，比如 QPS
    state.counters["QPS"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
//把BM_Put的Time、CPU 列强制用微秒 (μs) 展示,不再自动切换ns
BENCHMARK(BM_Put)->Unit(benchmark::kMicrosecond);

// --- 2. 测试 Get 性能 ---
static void BM_Get(benchmark::State& state) 
{
    TestData& data = Data();
    auto kv = std::make_unique<engine::memory_engine::MemoryKVStore>();
    // 先写入数据
    for (size_t i = 0; i < kNumOps; ++i) {
        kv->Put(data.keys[i], data.values[i]);
    }

    size_t i = 0;
    for (auto _ : state) {
        auto res = kv->Get(data.keys[i]);
        benchmark::DoNotOptimize(res);
        i = (i + 1) % kNumOps;
    }
    state.SetItemsProcessed(state.iterations());
    state.counters["QPS"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_Get)->Unit(benchmark::kMicrosecond);

// --- 3. 混合读写测试（模拟 Agent 场景：写多读少） ---
static void BM_MixedReadWrite(benchmark::State& state) 
{
    TestData& data = Data();
    auto kv = std::make_unique<engine::memory_engine::MemoryKVStore>();
    size_t i = 0;
    for (auto _ : state) 
    {
        // 80% 写，20% 读
        if (i % 5 == 0) {
            auto res = kv->Get(data.keys[i]);
            benchmark::DoNotOptimize(res);
        } else {
            kv->Put(data.keys[i], data.values[i]);
            benchmark::DoNotOptimize(kv);
        }
        i = (i + 1) % kNumOps;
    }
    state.SetItemsProcessed(state.iterations());
    state.counters["QPS"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_MixedReadWrite)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
