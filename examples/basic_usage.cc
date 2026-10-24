#include <kvstore/api/kvstore.h>
#include <kvstore/core/memtable/skip_memtable.h>
#include <kvstore/core/sstable/sstable.h>
#include <kvstore/core/sstable/sstable_builder.h>
#include <kvstore/composition/composition_root.h>
#include <kvstore/composition/config.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

void Check(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string(message));
    }
}

void TestBasicKVStoreOperations(const std::filesystem::path& data_dir)
{
    kvstore::composition::Composition composition;
    auto result = composition.Create(kvstore::Config{.data_dir = data_dir});
    Check(result.has_value(), "Composition::Create should succeed");

    auto& kv = *result;
    std::string_view value;

    auto missing = kv.Get("missing", &value);
    Check(missing.is_not_found(), "Get should report a missing key");
    Check(kv.Get("key", nullptr).Code() == kvstore::StatusCode::InvalidArgument,
          "Get should reject a null output");
    Check(kv.Put("", "value").Code() == kvstore::StatusCode::InvalidArgument,
          "Put should reject an empty key");
    Check(kv.Put("key", "").Code() == kvstore::StatusCode::InvalidArgument,
          "Put should reject an empty value");

    Check(kv.Put("key", "first value").ok(), "Put should store a key/value pair");
    Check(kv.Get("key", &value).ok() && value == "first value",
          "Get should return the stored value");

    Check(kv.Put("key", "updated value").ok(), "Put should overwrite an existing key");
    Check(kv.Get("key", &value).ok() && value == "updated value",
          "Get should return the most recently written value");

    Check(kv.Put("another key", "another value").ok(), "Put should support multiple keys");
    Check(kv.Get("key", &value).ok() && value == "updated value",
          "writing another key should preserve existing values");
}

void TestSkipMemTableOperations()
{
    kvstore::core::memtable::SkipMemTable memtable;
    const kvstore::Key key{0x01, 0x02};
    const kvstore::Value value{0x11, 0x12};
    const kvstore::Value updated_value{0x21, 0x22};
    const kvstore::Value* stored_value = nullptr;

    Check(memtable.Size() == 0, "a new memtable should be empty");
    Check(memtable.Put(key, value, 1).ok(), "memtable Put should succeed");
    Check(memtable.Get(key, &stored_value).ok() && stored_value != nullptr &&
              *stored_value == value,
          "memtable Get should return the stored value");

    Check(memtable.Put(key, updated_value, 2).ok(), "memtable Put should update a key");
    Check(memtable.Size() == 1, "updating a key should not increase memtable size");
    Check(memtable.Get(key, &stored_value).ok() && *stored_value == updated_value,
          "memtable Get should return the updated value");

    Check(memtable.Delete(key, 3).ok(), "memtable Delete should record a tombstone");
    Check(memtable.Get(key, &stored_value).is_not_found(),
          "Get should treat a deleted key as missing");
    Check(memtable.Delete(key, 4).ok(),
          "a newer delete should replace the existing tombstone");
    Check(memtable.Delete({0x03}, 5).ok() &&
              memtable.Get({0x03}, &stored_value).is_not_found(),
          "deleting a key absent from the memtable should preserve a tombstone");
    Check(memtable.Put(key, updated_value, 2).ok() &&
              memtable.Get(key, &stored_value).is_not_found(),
          "an older write must not replace a newer tombstone");
    Check(memtable.Put(key, updated_value, 5).ok() &&
              memtable.Get(key, &stored_value).ok() && *stored_value == updated_value,
          "a newer write should replace a tombstone");
}

void TestSSTableOperations(const std::filesystem::path& data_dir)
{
    using kvstore::core::sstable::SSTable;
    using kvstore::core::sstable::SSTableBuilder;
    using kvstore::core::sstable::SSTableRecord;

    const auto first_path = data_dir / "first.sst";
    const auto second_path = data_dir / "second.sst";
    const auto compacted_path = data_dir / "compacted.sst";

    SSTableBuilder first_builder(first_path);
    Check(first_builder.Add(SSTableRecord{{0x02}, 1, false, {0x11}}).ok(),
          "SSTable builder should accept value records");
    Check(first_builder.Add(SSTableRecord{{0x02}, 3, true}).ok(),
          "SSTable builder should accept tombstones");
    Check(first_builder.Add(SSTableRecord{{0x01}, 2, false, {0x21}}).ok(),
          "SSTable builder should accept records in arbitrary order");
    Check(first_builder.Finish().ok(), "SSTable builder should write a sorted table");

    SSTableBuilder second_builder(second_path);
    Check(second_builder.Add(SSTableRecord{{0x02}, 1, false, {0x11}}).ok(),
          "second table should accept an identical older version");
    Check(second_builder.Add(SSTableRecord{{0x03}, 5, false, {0x31}}).ok(),
          "second table should accept another key");
    Check(second_builder.Finish().ok(), "second table should be written");

    SSTable first;
    Check(first.Open(first_path).ok() && first.Size() == 3,
          "SSTable reader should load its index");
    kvstore::Value value;
    Check(first.Get({0x02}, &value, 2).ok() && value == kvstore::Value{0x11},
          "SSTable lookup should honor a sequence snapshot");
    Check(first.Get({0x02}, &value).is_not_found(),
          "SSTable lookup should apply the newest tombstone");

    SSTable second;
    Check(second.Open(second_path).ok(), "second SSTable should reopen");
    Check(SSTable::Compact({&first, &second}, compacted_path).ok(),
          "compaction should merge versions from input tables");

    SSTable compacted;
    Check(compacted.Open(compacted_path).ok() && compacted.Size() == 4,
          "compaction should deduplicate identical versions and preserve tombstones");
    Check(compacted.Get({0x02}, &value).is_not_found(),
          "compaction must retain tombstones when lower-level coverage is unknown");
    std::vector<SSTableRecord> records;
    Check(compacted.ReadAll(&records).ok() && records.size() == 4 &&
              records[0].key_ == kvstore::Key{0x01} &&
              records[1].key_ == kvstore::Key{0x02} &&
              records[1].sequence_ == 3,
          "SSTable records should remain key-sorted and newest-version-first");
}

} // namespace

int main()
{
    const auto data_dir = std::filesystem::temp_directory_path() /
        ("kvstore-basic-usage-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        TestSkipMemTableOperations();
        std::filesystem::create_directories(data_dir);
        TestSSTableOperations(data_dir);
        TestBasicKVStoreOperations(data_dir);
        std::filesystem::remove_all(data_dir);
        std::cout << "All basic_usage checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::error_code cleanup_error;
        std::filesystem::remove_all(data_dir, cleanup_error);
        if (cleanup_error)
        {
            spdlog::error("Failed to clean test data directory: {}", cleanup_error.message());
        }
        spdlog::error("basic_usage check failed: {}", error.what());
        return 1;
    }
}