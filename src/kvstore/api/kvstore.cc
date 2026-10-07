#include "kvstore/core/wal/wal_manager.h"
#include <cstdint>
#include <kvstore/api/kvstore.h>
#include <kvstore/common/status.h>
#include <cassert>

using namespace kvstore::api;

KVStore::KVStore(core::wal::WALManager wal)
    : wal_(std::move(wal))
{}

kvstore::Status KVStore::Put(std::string_view key , std::string_view value)
{
    // check argument 
    if (key.empty() || value.empty()) { return Status{StatusCode::InvalidArgument,"key and value must not be empty"}; }

    // convert arugment type
    Key key_bytes(reinterpret_cast<const uint8_t*>(key.data()), reinterpret_cast<const uint8_t*>(key.data()) + key.size());
    Value value_bytes(reinterpret_cast<const uint8_t*>(value.data()), reinterpret_cast<const uint8_t*>(value.data()) + value.size());

    auto res = wal_.AppendPut(key_bytes, value_bytes);

    return res;
}
