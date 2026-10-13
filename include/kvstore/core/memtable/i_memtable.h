#pragma once

#include <kvstore/common/status.h>
#include <kvstore/common/types.h>

namespace kvstore::core::memtable
{

class IMemTable
{
public:
    virtual ~IMemTable() = default;

    [[nodiscard]] virtual Status Put(const Key&key, const Value& value, Sequence sequence) = 0;    
};

} // namespace kvstore::core::memtable