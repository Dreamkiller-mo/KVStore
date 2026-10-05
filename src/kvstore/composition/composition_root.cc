#include "kvstore/common/status.h"
#include "kvstore/storage/file.h"
#include <expected>
#include <kvstore/api/kvstore.h>
#include <kvstore/core/wal/wal_manager.h>
#include <kvstore/composition/composition_root.h>
#include <kvstore/core/wal/file_committer.h>
#include <memory>

using namespace kvstore::composition;

Composition::KVStoreResult Composition::Create(const Config& config)
{
    auto wal = CreateWAL(config);       // std::expected<kvstore::core::wal::WALManager, kvstore::Status>

    if (!wal){ return std::unexpected(wal.error()); }

    // *wal -> WALManager& ; std::move(*wal) -> WALManager&&
    return kvstore::api::KVStore(std::move(*wal));
    
}

Composition::WALResult Composition::CreateWAL(const Config& config)
{
    // assemble & create wal dir
    auto wal_dir = config.data_dir / "wal";

    std::error_code ec;
    std::filesystem::create_directories(wal_dir, ec);

    if (ec){ return std::unexpected(kvstore::Status{kvstore::StatusCode::IOError, ec.message()}); }

    // get wal file path
    auto wal_path = wal_dir / "current.log";
    auto file = kvstore::storage::File();

    auto status  = file.Open(std::string(wal_path));
    if(!status .ok()) { return std::unexpected(status ); }

    // create committer
    auto committer = std::make_unique<core::wal::FileCommitter>(std::move(file));

    // inject WALManager
    return core::wal::WALManager(std::move(committer));
}

