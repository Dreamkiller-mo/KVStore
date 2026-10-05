#pragma once

#include <kvstore/common/types.h>
#include <kvstore/core/wal/wal_manager.h>

namespace kvstore 
{
    class Status;
}

namespace kvstore::api
{

class KVStore
{
public:
    KVStore(core::wal::WALManager wal);
    
    [[nodiscard]] Status Put(Key key , Value value);

    // [[nodiscard]] Status Get(Key key) const;

    // [[nodiscard]] Status Delete(Key key);

private:
    core::wal::WALManager wal_;
};

} // namespace kvstore
