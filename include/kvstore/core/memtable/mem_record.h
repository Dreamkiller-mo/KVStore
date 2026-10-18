#pragma once
#include <kvstore/common/types.h>
#include <kvstore/common/status.h>

namespace kvstore::core::memtable
{

struct MemTableRecord
{
    Key key_;
    Sequence sequence_;
    RecordType type_;
    Value value_;

    MemTableRecord(MemTableRecord&& mr) noexcept;

    MemTableRecord(const Key& key, Sequence sequence, RecordType type, const Value& value) noexcept;

    MemTableRecord(const Key& key, Sequence sequence, RecordType type, Value&& value) noexcept;

    MemTableRecord(Key&& key, Sequence sequence, RecordType type, Value&& value) noexcept;

    MemTableRecord& operator=(MemTableRecord&& mr) noexcept;
    
};
    
} // namespace kvstore::core::memtable