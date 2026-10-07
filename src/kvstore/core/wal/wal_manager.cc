#include "kvstore/common/status.h"
#include "kvstore/common/types.h"
#include "kvstore/core/wal/committer.h"
#include <kvstore/core/wal/wal_manager.h>
#include <kvstore/core/wal/wal_record.h>
#include <spdlog/spdlog.h>

using namespace kvstore::core::wal;

WALManager::WALManager(std::unique_ptr<ICommitter> committer)
    : committer_(std::move(committer))
    {}

kvstore::Status WALManager::AppendPut(std::span<const uint8_t> key, std::span<const uint8_t> value)
{    
    // check WALManager Status
    if (failed_) { spdlog::error("WALManager is in failed status"); return Status{StatusCode::InvalidState, "WALManager is in failed status"}; }

    spdlog::info("WAL AppendPut key_size={} value_size={}",key.size(),value.size());

    // clear buffer
    buffer_.clear();

    // temporarily next sequence number
    auto next_sequence = sequence_ + 1;

    // create WALRecord
    PutWALRecord record_type { .key = Key(key.begin(), key.end()), .value = Value(value.begin(), value.end())};
    Operation operation = {std::move(record_type)};
    WALRecord record(next_sequence, operation);

    // call WALCodec::Encode()
    codec_.Encode(record, buffer_);

    // save up record and flush
    auto status = committer_->Commit(buffer_);
    if(!status.ok()) { failed_ = true; return status; }
    sequence_ = next_sequence;

    return status;
}