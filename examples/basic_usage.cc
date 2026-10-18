#include <kvstore/api/kvstore.h>
#include <kvstore/core/memtable/skip_memtable.h>
#include <kvstore/composition/composition_root.h>
#include <kvstore/composition/config.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

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
    Check(memtable.Delete(key, 4).is_not_found(),
          "deleting an absent key should report NotFound");
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