#pragma once

#include "../../common/types.h"
#include "memtable.h"

namespace kvstore::memtable::iterator 
{

class IMemTableIterator 
{
public:
    virtual ~IMemTableIterator() = default;

    virtual void SeekToFirst() = 0;

    virtual void Next() = 0;

    virtual bool Valid() const = 0;

    virtual const Key& key() const = 0;

    virtual const Value& value() const = 0;
};

} // kvstore::memtable::iterator