#include "kvstore/common/status.h"
#include "kvstore/common/types.h"
#include <kvstore/core/wal/wal_manager.h>
#include <kvstore/core/wal/wal_record.h>

using namespace kvstore::core::wal;

kvstore::Status WALManager::AppendPut(std::span<const uint8_t> key, std::span<const uint8_t> value)
{
    // create WALRecord
    PutWALRecord record_type { .key = Key(key.begin(), key.end()), .value = Value(value.begin(), value.end())};
    Operation operation = {std::move(record_type)};
    Sequence sequnce = ++sequence_;
    WALRecord record(sequnce, operation);

    // call WALCodec::Encode()
    if( !codec_.Encode(record, buffer_) ) return kvstore::Status{StatusCode::Corruption};

    // save up record

    return kvstore::Status{StatusCode::Ok};
}