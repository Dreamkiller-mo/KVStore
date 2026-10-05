#include "kvstore/storage/file.h"
#include <kvstore/core/wal/file_committer.h>

using namespace kvstore::core::wal;

FileCommitter::FileCommitter(kvstore::storage::File file)
    : file_(std::move(file))
{}

kvstore::Status FileCommitter::Commit(std::span<const std::uint8_t> data)
{
    // check buffer empty
    if (data.empty()) { return Status{StatusCode::Ok}; }

    // fill page cache
    auto status = file_.Append(data);
    if(!status.ok()) { return status; }

    // try flush disk
    status = file_.Sync();
    if(!status.ok()) { return status; }

    // clear Resources

    return Status{StatusCode::Ok};
}

