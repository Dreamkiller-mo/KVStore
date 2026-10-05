#include "kvstore/common/status.h"
#include "kvstore/common/types.h"
#include "kvstore/core/wal/committer.h"
#include <kvstore/core/wal/wal_manager.h>
#include <kvstore/core/wal/wal_record.h>

using namespace kvstore::core::wal;

WALManager::WALManager(std::unique_ptr<ICommitter> committer)
    : committer_(std::move(committer))
    {}

kvstore::Status WALManager::AppendPut(std::span<const uint8_t> key, std::span<const uint8_t> value)
{
    // check WALManager Status
    if (failed_) { return Status{StatusCode::InvalidState, "WALManager is in failed status"}; }

    // create WALRecord
    PutWALRecord record_type { .key = Key(key.begin(), key.end()), .value = Value(value.begin(), value.end())};
    Operation operation = {std::move(record_type)};
    Sequence sequence = ++sequence_;
    WALRecord record(sequence, operation);

    // call WALCodec::Encode()
    codec_.Encode(record, buffer_);

    // save up record and flush
    
    return Status{StatusCode::Ok};
}