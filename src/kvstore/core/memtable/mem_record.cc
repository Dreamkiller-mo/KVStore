#include <kvstore/core/memtable/mem_record.h>

using namespace kvstore::core::memtable;

MemTableRecord::MemTableRecord(MemTableRecord&& mr) noexcept
    : key_(std::move(mr.key_))
    , sequence_(std::move(mr.sequence_))
    , type_(std::move(mr.type_))
    , value_(std::move(mr.value_))
{}

MemTableRecord::MemTableRecord(const Key& key, Sequence sequence, RecordType type, const Value& value) noexcept
    : key_(key)
    , sequence_(sequence)
    , type_(type)
    , value_(value)
{}

MemTableRecord::MemTableRecord(const Key& key, Sequence sequence, RecordType type, Value&& value) noexcept
    : key_(key)
    , sequence_(sequence)
    , type_(type)
    , value_(std::move(value))
{} 

MemTableRecord::MemTableRecord(Key&& key, Sequence sequence, RecordType type, Value&& value) noexcept
    : key_(std::move(key))
    , sequence_(sequence)
    , type_(type)
    , value_(std::move(value))
{}

MemTableRecord& MemTableRecord::operator=(MemTableRecord&& mr) noexcept
{
    if (this != &mr)
    {
        key_ = std::move(mr.key_);
        sequence_ = std::move(mr.sequence_);
        type_ = std::move(mr.type_);
        value_ = std::move(mr.value_);
    }
    return *this;
}
