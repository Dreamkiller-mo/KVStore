#pragma once

#include "../common/status.h"
#include "../common/types.h"

namespace kvstore::api
{

class kvstore
{
public:
    Status Put(Key key , Value value);

    Status Get(Key key);

    Status Delete(Key key);

private:


};

} // namespace kvstore
