#pragma once

#include <kvstore/common/status.h>
#include <kvstore/common/types.h>
#include <kvstore//core/wal/wal_codec.h>

namespace kvstore::core::wal 
{

class WALManager
{
public:
    [[nodiscard]] Status AppendPut(std::span<const uint8_t> key,std::span<const uint8_t> value);

    // [[nodiscard]] Status AppendDelete(std::span<const uint8_t> key);

    [[nodiscard]] Status Flush();

private:
    WALCodec codec_;

    std::vector<uint8_t> buffer_;

    uint64_t sequence_ = 0;

    // 后面再加
    // File file_;
};

}   //namespace kvstore::core::wal
