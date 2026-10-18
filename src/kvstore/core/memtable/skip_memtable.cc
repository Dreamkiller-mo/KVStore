#include <cstddef>
#include <kvstore/core/memtable/skip_memtable.h>
#include <spdlog/spdlog.h>

using namespace kvstore::core::memtable;

kvstore::Status SkipMemTable::Put(const Key& key, const Value& value, Sequence sequence)
{
    MemTableRecord record(key, sequence, RecordType::PUT, value);
    auto [it, inserted] = skip_list_.insert_or_assign(key, std::move(record));
    if (!inserted) { spdlog::info("SkipMemTable::Put: key already exists, updated value"); }

    return kvstore::Status{ StatusCode::Ok, "Put operation successful" };
}

kvstore::Status SkipMemTable::Get(const Key& key, const Value** value) const
{
    if (value == nullptr)
    {
        return kvstore::Status{ StatusCode::InvalidArgument, "value output must not be null" };
    }

    auto it = skip_list_.find(key);
    if (it == skip_list_.end())
    {
        return kvstore::Status{ StatusCode::NotFound, "Key not found" };
    }

    const MemTableRecord& record = it->second;
    if (record.type_ == RecordType::DELETE)
    {
        return kvstore::Status{ StatusCode::NotFound, "Key has been deleted" };
    }

    *value = &record.value_;
    return kvstore::Status{ StatusCode::Ok, "Get operation successful" };
}

kvstore::Status SkipMemTable::Delete(const Key& key, Sequence sequence)
{
    auto it = skip_list_.find(key);
    if (it == skip_list_.end() || it->second.type_ == RecordType::DELETE)
    {
        return kvstore::Status{ StatusCode::NotFound, "Key not found" };
    }

    it->second = MemTableRecord(key, sequence, RecordType::DELETE, Value{});
    return kvstore::Status{ StatusCode::Ok, "Delete operation successful" };
}

size_t SkipMemTable::Size() const
{
    return skip_list_.size();
}

kvstore::Status SkipMemTable::Put(const Key& key, Value&& value, Sequence sequence)
{
    MemTableRecord record(key, sequence, RecordType::PUT, std::move(value));
    auto [it, inserted] = skip_list_.insert_or_assign(key, std::move(record));
    if (!inserted) { spdlog::info("SkipMemTable::Put: key already exists, updated value"); }

    return kvstore::Status{ StatusCode::Ok, "Put operation successful" };
}