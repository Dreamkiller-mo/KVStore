#pragma once

#include "encoder.h"
#include "wal_record.h"
#include <concepts>
#include <cstdint>
#include <vector>
#include <span>

namespace kvstore::core::wal 
{

// concept Encode type
template <typename T>
concept EncodeType = requires (const T& type, Encoder& enc) 
{
    { type.Encode(enc) } -> std::same_as<bool>;
};

// WAL Codec( EnCoder(编码器) + DeCoder(解码器) )
class WALCodec
{
public:
    // WALRecord ---> bytes
    void Encode(const WALRecord& record, std::vector<std::uint8_t>& buffer) const;
    
    // bytes ---> WALRecord
    [[nodiscard]] WALRecord Decode(std::span<const uint8_t>);

};

}   // namespace kvstore::core::wal 
