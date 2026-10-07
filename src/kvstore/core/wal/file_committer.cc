#include "kvstore/storage/file.h"
#include <kvstore/core/wal/file_committer.h>
#include <spdlog/spdlog.h>

using namespace kvstore::core::wal;

FileCommitter::FileCommitter(kvstore::storage::File file)
    : file_(std::move(file))
{}

kvstore::Status FileCommitter::Commit(std::span<const std::uint8_t> data)
{
    spdlog::info("WAL Commit bytes={}",data.size());
    // check buffer empty
    if (data.empty()) { spdlog::error("WAL Commit data is empty"); return Status{StatusCode::Ok}; }

    // fill page cache
    auto status = file_.Append(data);
    if(!status.ok()) { spdlog::error("WAL Commit Append failed, status={}",status.Error()); return status; }

    // try flush disk
    status = file_.Sync();
    if(!status.ok()) { spdlog::error("WAL Commit Sync failed, status={}",status.Error()); return status; }

    spdlog::info("WAL Commit success, bytes={}",data.size());
    return Status{StatusCode::Ok};
}

