#pragma once
#include <kvstore/common/types.h>
#include <kvstore/common/status.h>

namespace kvstore::core::memtable
{

struct MemTableRecord
{
    Key key;
    Sequence sequence;
    RecordType type;
    Value value;

    MemTableRecord(MemTableRecord&& mr) : key(std::move(mr.value)), sequence(std::move(mr.sequence)), type(std::move(mr.type)), value(std::move(mr.value)) {}
    
};
    
} // namespace kvstore::core::memtable