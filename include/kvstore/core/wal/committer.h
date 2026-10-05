#pragma once
#include <kvstore/common/status.h>
#include <span>

namespace kvstore::core::wal 
{

class ICommitter
{
public:
    virtual ~ICommitter() = default;

    [[nodiscard]] virtual Status Commit(std::span<const std::uint8_t> data) = 0;

};

}   // namespace kvstore::core::wal