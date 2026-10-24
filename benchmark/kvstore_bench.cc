#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>

#include <kvstore/api/kvstore.h>
#include <kvstore/common/status.h>
#include <kvstore/composition/composition_root.h>
#include <kvstore/composition/config.h>
#include <kvstore/core/memtable/skip_memtable.h>
#include <kvstore/core/sstable/sstable.h>
#include <kvstore/core/sstable/sstable_builder.h>
#include <kvstore/core/sstable/sstable_record.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
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
        std::filesystem::create_directories(path_);
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

kvstore::Key MakeKey(std::size_t index)
{
    kvstore::Key key(sizeof(index));
    for (std::size_t byte = 0; byte < sizeof(index); ++byte)
    {
        key[byte] = static_cast<std::uint8_t>(index >> (byte * 8));
    }
    return key;
}

kvstore::Value MakeValue(std::size_t index)
{
    return {static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(index >> 8),
            static_cast<std::uint8_t>(index >> 16),
            static_cast<std::uint8_t>(index >> 24)};
}

std::vector<kvstore::core::sstable::SSTableRecord> MakeRecords(std::size_t count)
{
    std::vector<kvstore::core::sstable::SSTableRecord> records;
    records.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        records.emplace_back(MakeKey(index), index + 1, false, MakeValue(index));
    }
    return records;
}

kvstore::Status BuildTable(
    const std::filesystem::path& path,
    const std::vector<kvstore::core::sstable::SSTableRecord>& records)
{
    kvstore::core::sstable::SSTableBuilder builder(path);
    for (const auto& record : records)
    {
        auto status = builder.Add(record);
        if (!status.ok())
        {
            return status;
        }
    }
    return builder.Finish();
}

void BM_Get(benchmark::State& state)
{   
    // close the spdlog logger to avoid logging during the benchmark
    spdlog::set_level(spdlog::level::off);

    // get the number of keys to use for the benchmark from the state
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::unique_ptr<TemporaryDataDirectory> data_dir;
    std::unique_ptr<kvstore::api::KVStore> kv;
    std::vector<std::string> keys;
    std::vector<std::string> expected_values;
    keys.reserve(num_keys);
    expected_values.reserve(num_keys);

    std::size_t index = 0;
    for (auto _ : state)
    {
        if (!kv)
        {
            state.PauseTiming();
            data_dir = std::make_unique<TemporaryDataDirectory>();
            kvstore::composition::Composition composition;
            auto result = composition.Create(kvstore::Config{.data_dir = data_dir->Path()});
            if (!result)
            {
                const auto error = result.error().Error();
                state.SkipWithError(error.c_str());
                return;
            }
            kv = std::make_unique<kvstore::api::KVStore>(std::move(*result));
            for (std::size_t key_index = 0; key_index < num_keys; ++key_index)
            {
                keys.push_back("benchmark-key-" + std::to_string(key_index));
                expected_values.push_back("benchmark-value-" + std::to_string(key_index));
                const auto put_status = kv->Put(keys.back(), expected_values.back());
                if (!put_status.ok())
                {
                    const auto error = put_status.Error();
                    state.SkipWithError(error.c_str());
                    return;
                }
            }
            state.ResumeTiming();
        }

        std::string_view value;
        const auto status = kv->Get(keys[index], &value);
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

    // set processed--total item iterations, because per iteration we process one item of Get function -> iterations() * 1
    state.SetItemsProcessed(state.iterations());
    state.counters["DatasetSize"] = static_cast<double>(num_keys);
    // set the QPS counter to the number of iterations per second
    state.counters["QPS"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
}

void BM_MemTablePut(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::off);
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::vector<kvstore::core::sstable::SSTableRecord> records;

    for (auto _ : state)
    {
        if (records.empty())
        {
            state.PauseTiming();
            records = MakeRecords(num_keys);
            state.ResumeTiming();
        }
        kvstore::core::memtable::SkipMemTable memtable;
        for (const auto& record : records)
        {
            const auto status = memtable.Put(record.key_, record.value_, record.sequence_);
            if (!status.ok())
            {
                state.SkipWithError(status.Error().c_str());
                return;
            }
        }
        benchmark::DoNotOptimize(memtable.Size());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(num_keys));
    state.counters["DatasetSize"] = static_cast<double>(num_keys);
    state.counters["OpsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations() * static_cast<std::int64_t>(num_keys)),
        benchmark::Counter::kIsRate);
}

void BM_MemTableGet(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::off);
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::vector<kvstore::core::sstable::SSTableRecord> records;
    kvstore::core::memtable::SkipMemTable memtable;
    std::size_t index = 0;
    for (auto _ : state)
    {
        if (records.empty())
        {
            state.PauseTiming();
            records = MakeRecords(num_keys);
            for (const auto& record : records)
            {
                const auto status = memtable.Put(record.key_, record.value_, record.sequence_);
                if (!status.ok())
                {
                    state.SkipWithError(status.Error().c_str());
                    return;
                }
            }
            state.ResumeTiming();
        }
        const kvstore::Value* value = nullptr;
        const auto status = memtable.Get(records[index].key_, &value);
        if (!status.ok() || value == nullptr || *value != records[index].value_)
        {
            state.SkipWithError("MemTable Get returned an unexpected result");
            return;
        }
        benchmark::DoNotOptimize(value);
        index = (index + 1) % num_keys;
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["DatasetSize"] = static_cast<double>(num_keys);
    state.counters["OpsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
}

void BM_SSTableBuild(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::off);
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::vector<kvstore::core::sstable::SSTableRecord> records;
    std::unique_ptr<TemporaryDataDirectory> data_dir;
    std::filesystem::path path;

    for (auto _ : state)
    {
        if (!data_dir)
        {
            state.PauseTiming();
            records = MakeRecords(num_keys);
            data_dir = std::make_unique<TemporaryDataDirectory>();
            path = data_dir->Path() / "build.sst";
            state.ResumeTiming();
        }
        const auto status = BuildTable(path, records);
        if (!status.ok())
        {
            state.SkipWithError(status.Error().c_str());
            return;
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(num_keys));
    state.counters["RecordsPerTable"] = static_cast<double>(num_keys);
    state.counters["RecordsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations() * static_cast<std::int64_t>(num_keys)),
        benchmark::Counter::kIsRate);
}

void BM_SSTableOpen(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::off);
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::unique_ptr<TemporaryDataDirectory> data_dir;
    std::filesystem::path path;

    for (auto _ : state)
    {
        if (!data_dir)
        {
            state.PauseTiming();
            const auto records = MakeRecords(num_keys);
            data_dir = std::make_unique<TemporaryDataDirectory>();
            path = data_dir->Path() / "open.sst";
            const auto build_status = BuildTable(path, records);
            if (!build_status.ok())
            {
                state.SkipWithError(build_status.Error().c_str());
                return;
            }
            state.ResumeTiming();
        }
        kvstore::core::sstable::SSTable table;
        const auto status = table.Open(path);
        if (!status.ok())
        {
            state.SkipWithError(status.Error().c_str());
            return;
        }
        benchmark::DoNotOptimize(table.Size());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(num_keys));
    state.counters["RecordsPerTable"] = static_cast<double>(num_keys);
    state.counters["RecordsIndexedPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations() * static_cast<std::int64_t>(num_keys)),
        benchmark::Counter::kIsRate);
}

void BM_SSTableGet(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::off);
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::vector<kvstore::core::sstable::SSTableRecord> records;
    std::unique_ptr<TemporaryDataDirectory> data_dir;
    kvstore::core::sstable::SSTable table;
    kvstore::Status status{kvstore::StatusCode::Ok};
    std::size_t index = 0;
    for (auto _ : state)
    {
        if (!data_dir)
        {
            state.PauseTiming();
            records = MakeRecords(num_keys);
            data_dir = std::make_unique<TemporaryDataDirectory>();
            const auto path = data_dir->Path() / "lookup.sst";
            status = BuildTable(path, records);
            if (status.ok())
            {
                status = table.Open(path);
            }
            if (!status.ok())
            {
                state.SkipWithError(status.Error().c_str());
                return;
            }
            state.ResumeTiming();
        }
        kvstore::Value value;
        status = table.Get(records[index].key_, &value);
        if (!status.ok() || value != records[index].value_)
        {
            state.SkipWithError("SSTable Get returned an unexpected result");
            return;
        }
        benchmark::DoNotOptimize(value.data());
        benchmark::DoNotOptimize(value.size());
        index = (index + 1) % num_keys;
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["DatasetSize"] = static_cast<double>(num_keys);
    state.counters["OpsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
}

void BM_SSTableCompact(benchmark::State& state)
{
    spdlog::set_level(spdlog::level::off);
    constexpr std::size_t kInputTables = 4;
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::unique_ptr<TemporaryDataDirectory> data_dir;
    std::vector<kvstore::core::sstable::SSTable> tables(kInputTables);
    std::vector<const kvstore::core::sstable::SSTable*> inputs;
    inputs.reserve(kInputTables);

    std::filesystem::path compacted_path;
    for (auto _ : state)
    {
        if (!data_dir)
        {
            state.PauseTiming();
            data_dir = std::make_unique<TemporaryDataDirectory>();
            compacted_path = data_dir->Path() / "compacted.sst";
            for (std::size_t table_index = 0; table_index < kInputTables; ++table_index)
            {
                std::vector<kvstore::core::sstable::SSTableRecord> records;
                records.reserve(num_keys);
                for (std::size_t index = 0; index < num_keys; ++index)
                {
                    records.emplace_back(MakeKey(index), table_index + 1, false,
                                         MakeValue(index + table_index));
                }
                const auto input_path = data_dir->Path() /
                    ("input-" + std::to_string(table_index) + ".sst");
                auto status = BuildTable(input_path, records);
                if (!status.ok())
                {
                    state.SkipWithError(status.Error().c_str());
                    return;
                }
                status = tables[table_index].Open(input_path);
                if (!status.ok())
                {
                    state.SkipWithError(status.Error().c_str());
                    return;
                }
                inputs.push_back(&tables[table_index]);
            }
            state.ResumeTiming();
        }
        const auto status = kvstore::core::sstable::SSTable::Compact(inputs, compacted_path);
        if (!status.ok())
        {
            state.SkipWithError(status.Error().c_str());
            return;
        }
        benchmark::ClobberMemory();
    }

    const auto records_per_compaction =
        static_cast<std::int64_t>(num_keys * kInputTables);
    state.SetItemsProcessed(state.iterations() * records_per_compaction);
    state.counters["InputTables"] = static_cast<double>(kInputTables);
    state.counters["RecordsPerCompaction"] = static_cast<double>(records_per_compaction);
    state.counters["RecordsPerSecond"] = benchmark::Counter(
        static_cast<double>(state.iterations() * records_per_compaction),
        benchmark::Counter::kIsRate);
}

void BM_Put(benchmark::State& state)
{
    // close the spdlog logger to avoid logging during the benchmark
    spdlog::set_level(spdlog::level::off);

    // get the number of keys to use for the benchmark from the state
    const auto num_keys = static_cast<std::size_t>(state.range(0));
    std::unique_ptr<TemporaryDataDirectory> data_dir;
    std::unique_ptr<kvstore::api::KVStore> kv;
    for (auto _ : state)
    {
        if (!kv)
        {
            state.PauseTiming();
            data_dir = std::make_unique<TemporaryDataDirectory>();
            kvstore::composition::Composition composition;
            auto result = composition.Create(kvstore::Config{.data_dir = data_dir->Path()});
            if (!result)
            {
                const auto error = result.error().Error();
                state.SkipWithError(error.c_str());
                return;
            }
            kv = std::make_unique<kvstore::api::KVStore>(std::move(*result));
            state.ResumeTiming();
        }
        for (std::size_t index = 0; index < num_keys; ++index)
        {
            const std::string key = "benchmark-key-" + std::to_string(index);
            const std::string value = "benchmark-value-" + std::to_string(index);

            const auto put_status = kv->Put(key, value);
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
BENCHMARK(BM_MemTablePut)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_MemTableGet)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SSTableBuild)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_SSTableOpen)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SSTableGet)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_SSTableCompact)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMillisecond);
} // namespace
