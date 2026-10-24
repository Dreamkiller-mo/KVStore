#pragma once

#include <kvstore/common/codec.h>
#include <kvstore/common/status.h>
#include <kvstore/core/sstable/sstable_record.h>

#include <cstdint>
#include <span>
#include <vector>

namespace kvstore::core::sstable
{

class SSTableCodec
{
public:
    // magic + record_count(uint64_t)
    static constexpr std::uint64_t kHeaderSize = 16;

    [[nodiscard]] static std::vector<std::uint8_t> EncodeHeader(std::uint64_t record_count);

    [[nodiscard]] static Status DecodeHeader(std::span<const std::uint8_t> bytes, std::uint64_t* record_count);

    [[nodiscard]] static Status EncodeRecord(const SSTableRecord& record, std::vector<std::uint8_t>* bytes);

    [[nodiscard]] static Status DecodeRecord(std::span<const std::uint8_t> bytes, SSTableRecord* record);
};

/*
┌─────────────────────────────────┐
│ Header (16B)                    │  ← 本函数负责
│   magic "KVSTBL01" (8B)         │
│   record_count (8B)             │
├─────────────────────────────────┤
│ Record 0: [klen][vlen][seq][t]  │  ← 最小 17 字节
│           [key][value]          │
│ Record 1: ...                   │
│ Record 2: ...                   │
├─────────────────────────────────┤
│ Index Block                     │  ← 键 → 偏移
├─────────────────────────────────┤
│ Footer (可选)                   │  ← 索引偏移 / 校验和
└─────────────────────────────────┘ 
*/
} // namespace kvstore::core::sstable
