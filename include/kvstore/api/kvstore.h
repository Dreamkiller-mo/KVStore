#pragma once

#include <kvstore/common/types.h>
#include <kvstore/core/wal/wal_manager.h>

namespace kvstore 
{
    class Status;
}

namespace kvstore::api
{

class KVstore
{
public:
    [[nodiscard]] Status Put(Key key , Value value);

    // [[nodiscard]] Status Get(Key key) const;

    // [[nodiscard]] Status Delete(Key key);

private:
    std::unique_ptr<core::wal::WALManager> wal_;
};

} // namespace kvstore
