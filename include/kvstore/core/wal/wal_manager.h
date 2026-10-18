#pragma once

#include <expected>
#include <kvstore/core/wal/committer.h>
#include <kvstore/common/status.h>
#include <kvstore/common/types.h>
#include <kvstore/core/wal/wal_codec.h>
#include <kvstore/storage/file.h>
#include <memory>

namespace kvstore::core::wal 
{

class WALManager
{
public:
    explicit WALManager(std::unique_ptr<ICommitter> committer);

    [[nodiscard]] std::expected<Sequence, Status> AppendPut(std::span<const uint8_t> key,std::span<const uint8_t> value);

    // [[nodiscard]] Status AppendDelete(std::span<const uint8_t> key);


private:
    WALCodec codec_;

    std::vector<uint8_t> buffer_;

    Sequence sequence_ = 0;

    // committer
    std::unique_ptr<ICommitter> committer_;
    
    // is broken
    bool failed_ {false};
};

}   //namespace kvstore::core::wal
