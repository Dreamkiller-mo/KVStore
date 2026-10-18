#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>

#include <kvstore/api/kvstore.h>
#include <kvstore/common/status.h>
#include <kvstore/composition/composition_root.h>
#include <kvstore/composition/config.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{

class TemporaryDataDirectory
{
public:
    TemporaryDataDirectory()
    {
        // get current operating system temp directory and append a unique name to it
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("kvstore-benchmark-" + std::to_string(timestamp));
    }

    ~TemporaryDataDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void BM_Get(benchmark::State& state)
{   
    // close the spdlog logger to avoid logging during the benchmark
    spdlog::set_level(spdlog::level::off);

    // get the number of keys to use for the benchmark from the state
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    
    TemporaryDataDirectory data_dir;
    kvstore::composition::Composition composition;
    auto result = composition.Create(kvstore::Config{.data_dir = data_dir.Path()});
    if (!result)
    {
        const auto error = result.error().Error();
        state.SkipWithError(error.c_str());
        return;
    }

    auto& kv = *result;
    std::vector<std::string> keys;
    std::vector<std::string> expected_values;
    keys.reserve(num_keys);
    expected_values.reserve(num_keys);

    for (std::size_t index = 0; index < num_keys; ++index)
    {
        keys.push_back("benchmark-key-" + std::to_string(index));
        expected_values.push_back("benchmark-value-" + std::to_string(index));

        const auto put_status = kv.Put(keys.back(), expected_values.back());
        if (!put_status.ok())
        {
            const auto error = put_status.Error();
            state.SkipWithError(error.c_str());
            return;
        }
    }

    std::size_t index = 0;
    for (auto _ : state)
    {
        std::string_view value;
        const auto status = kv.Get(keys[index], &value);
        if (!status.ok())
        {
            const auto error = status.Error();
            state.SkipWithError(error.c_str());
            break;
        }
        if (value != expected_values[index])
        {
            state.SkipWithError("Get returned an unexpected value");
            break;
        }

        benchmark::DoNotOptimize(value.data());
        benchmark::DoNotOptimize(value.size());
        index = (index + 1) % num_keys;
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["DatasetSize"] = static_cast<double>(num_keys);
    state.counters["QPS"] =
        benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
}

void BM_Put(benchmark::State& state)
{
    // close the spdlog logger to avoid logging during the benchmark
    spdlog::set_level(spdlog::level::off);

    // get the number of keys to use for the benchmark from the state
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    
    TemporaryDataDirectory data_dir;
    kvstore::composition::Composition composition;
    auto result = composition.Create(kvstore::Config{.data_dir = data_dir.Path()});
    if (!result)
    {
        const auto error = result.error().Error();
        state.SkipWithError(error.c_str());
        return;
    }

    auto& kv = *result;

    for (auto _ : state)
    {
        for (std::size_t index = 0; index < num_keys; ++index)
        {
            const std::string key = "benchmark-key-" + std::to_string(index);
            const std::string value = "benchmark-value-" + std::to_string(index);

            const auto put_status = kv.Put(key, value);
            if (!put_status.ok())
            {
                const auto error = put_status.Error();
                state.SkipWithError(error.c_str());
                return;
            }
        }
    }

    state.SetItemsProcessed(state.iterations() * num_keys);
    state.counters["DatasetSize"] = static_cast<double>(num_keys);
    state.counters["QPS"] =
        benchmark::Counter(static_cast<double>(state.iterations() * num_keys), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_Get)
    ->Arg(1024)
    ->Arg(2048)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Put)
    ->Arg(1024)
    ->Arg(2048)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);
} // namespace
