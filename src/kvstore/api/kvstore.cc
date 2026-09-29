#include "../../../include/kvstore/api/kvstore.h"
#include "../../../include/kvstore/common/status.h"

using namespace kvstore::api;

kvstore::Status KVstore::Put(Key key , Value value)
{
    auto res = wal_->AppendPut(key, value);

    
    return Status(StatusCode::Ok);
}
