#pragma once
#include "kvstore/core/wal/wal_manager.h"
#include <expected>
#include <kvstore/api/kvstore.h>
#include <kvstore/common/status.h>
#include <kvstore/composition/config.h>

namespace kvstore::composition 
{

class Composition
{
public:
    using WALResult = std::expected<core::wal::WALManager, Status>;
    using KVStoreResult = std::expected<kvstore::api::KVStore, kvstore::Status>;

    [[nodiscard]] KVStoreResult Create(const Config& config);

private:
    [[nodiscard]] WALResult CreateWAL(const Config& config);

};

}   // namespace kvstore::composition