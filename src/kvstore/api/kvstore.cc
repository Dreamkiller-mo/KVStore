#include <kvstore/core/wal/wal_manager.h>
#include <kvstore/api/kvstore.h>
#include <kvstore/common/status.h>
#include <kvstore/core/memtable/skip_memtable.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstdint>

using namespace kvstore::api;

KVStore::KVStore(core::wal::WALManager wal)
    : wal_(std::move(wal))
    , memtable_(std::make_unique<core::memtable::SkipMemTable>())
{}

kvstore::Status KVStore::Put(std::string_view key , std::string_view value)
{
    // check argument 
    if (key.empty() || value.empty()) { return Status{StatusCode::InvalidArgument,"key and value must not be empty"}; }

    // convert arugment type
    Key key_bytes(reinterpret_cast<const uint8_t*>(key.data()), reinterpret_cast<const uint8_t*>(key.data()) + key.size());
    Value value_bytes(reinterpret_cast<const uint8_t*>(value.data()), reinterpret_cast<const uint8_t*>(value.data()) + value.size());

    auto res = wal_.AppendPut(key_bytes, value_bytes);
    if(!res) return res.error();
    auto sequence = *res;

    // memtable
    spdlog::info("KVStore::Put ---> memtable_ -> Put");
    return memtable_->Put(key_bytes, value_bytes, sequence);
}

kvstore::Status KVStore::Get(std::string_view key, std::string_view* value) const
{
    if (key.empty() || value == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "key and value output must not be empty or null"};
    }

    Key key_bytes(reinterpret_cast<const uint8_t*>(key.data()),
                  reinterpret_cast<const uint8_t*>(key.data()) + key.size());
    const Value* value_bytes = nullptr;
    auto status = memtable_->Get(key_bytes, &value_bytes);
    if (!status.ok())
    {
        return status;
    }

    *value = std::string_view(reinterpret_cast<const char*>(value_bytes->data()), value_bytes->size());
    return status;
}
