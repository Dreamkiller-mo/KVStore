#pragma once

#include <kvstore/common/types.h>

#include <utility>

namespace kvstore::core::sstable
{

struct SSTableRecord
{
    Key key_;
    Sequence sequence_ {0};
    bool deleted_ {false};
    Value value_;

    SSTableRecord() = default;
    SSTableRecord(Key key, Sequence sequence, bool deleted, Value value = {}) : key_(std::move(key)), sequence_(sequence), deleted_(deleted), value_(std::move(value))
    {}
};

} // namespace kvstore::core::sstable
