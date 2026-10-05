#include "kvstore/core/wal/wal_manager.h"
#include <kvstore/api/kvstore.h>
#include <kvstore/common/status.h>

using namespace kvstore::api;

KVStore::KVStore(core::wal::WALManager wal)
    : wal_(std::move(wal))
{}

kvstore::Status KVStore::Put(Key key , Value value)
{
    auto res = wal_.AppendPut(key, value);

    
    return Status(StatusCode::Ok);
}
