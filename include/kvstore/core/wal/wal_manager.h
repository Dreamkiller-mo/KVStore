#pragma once

#include "../../common/status.h"
#include "../../common/types.h"
#include "wal_codec.h"
#include <memory>

namespace kvstore::core::wal 
{

class WALManager
{
public:
    Status AppendPut(const Key& key, const Value& value);

private:
    std::unique_ptr<WALCodec> codec_;
};

}   //namespace kvstore::core::wal
