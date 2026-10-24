#include <kvstore/core/sstable/sstable_codec.h>

#include <algorithm>
#include <array>
#include <limits>

namespace kvstore::core::sstable
{
namespace
{

constexpr std::array<std::uint8_t, 8> kMagic{'K', 'V', 'S', 'T', 'B', 'L', '0', '1'};
constexpr std::size_t kMinimumRecordSize = sizeof(std::uint32_t) * 2 + sizeof(std::uint64_t) + 1;

} // namespace

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

std::vector<std::uint8_t> SSTableCodec::EncodeHeader(std::uint64_t record_count)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(kHeaderSize);

    common::Encoder encoder(bytes);
    encoder.PutBytes(kMagic);
    encoder.PutUint64(record_count);
    return bytes;
}

Status SSTableCodec::DecodeHeader(std::span<const std::uint8_t> bytes, std::uint64_t* record_count)
{
    if (record_count == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "record count output must not be null"};
    }
    if (bytes.size() != kHeaderSize)
    {
        return Status{StatusCode::Corruption, "invalid SSTable header size"};
    }

    common::Decoder decoder(bytes);
    std::span<const std::uint8_t> magic;
    if (!decoder.GetBytes(kMagic.size(), magic) ||
        !std::equal(magic.begin(), magic.end(), kMagic.begin()) ||
        !decoder.GetUint64(*record_count) || !decoder.empty())
    {
        return Status{StatusCode::Corruption, "invalid SSTable header"};
    }
    return Status{StatusCode::Ok};
}

Status SSTableCodec::EncodeRecord(const SSTableRecord& record, std::vector<std::uint8_t>* bytes)
{
    if (bytes == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "encoded record output must not be null"};
    }
    if ((record.deleted_ && !record.value_.empty()) ||
        record.key_.size() > std::numeric_limits<std::uint32_t>::max() ||
        record.value_.size() > std::numeric_limits<std::uint32_t>::max())
    {
        return Status{StatusCode::InvalidArgument, "invalid or oversized SSTable record"};
    }

    const auto body_size = kMinimumRecordSize + record.key_.size() + record.value_.size();
    if (body_size > std::numeric_limits<std::uint32_t>::max())
    {
        return Status{StatusCode::InvalidArgument, "SSTable record exceeds format size limit"};
    }

    bytes->clear();
    bytes->reserve(sizeof(std::uint32_t) + body_size);
    
    common::Encoder encoder(*bytes);
    encoder.PutUint32(static_cast<std::uint32_t>(body_size));
    encoder.PutUint32(static_cast<std::uint32_t>(record.key_.size()));
    encoder.PutUint32(static_cast<std::uint32_t>(record.value_.size()));
    encoder.PutUint64(record.sequence_);
    encoder.PutUint8(record.deleted_ ? 1 : 0);
    encoder.PutBytes(record.key_);
    encoder.PutBytes(record.value_);
    return Status{StatusCode::Ok};
}

Status SSTableCodec::DecodeRecord(std::span<const std::uint8_t> bytes, SSTableRecord* record)
{
    if (record == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "decoded record output must not be null"};
    }
    if (bytes.size() < kMinimumRecordSize)
    {
        return Status{StatusCode::Corruption, "SSTable record is truncated"};
    }

    common::Decoder decoder(bytes);
    std::uint32_t key_size = 0;
    std::uint32_t value_size = 0;
    std::uint64_t sequence = 0;
    std::uint8_t deleted = 0;
    if (!decoder.GetUint32(key_size) || !decoder.GetUint32(value_size) ||
        !decoder.GetUint64(sequence) || !decoder.GetUint8(deleted) || deleted > 1 ||
        static_cast<std::uint64_t>(key_size) + value_size != decoder.remaining())
    {
        return Status{StatusCode::Corruption, "invalid SSTable record fields"};
    }

    std::span<const std::uint8_t> key;
    std::span<const std::uint8_t> value;
    if (!decoder.GetBytes(key_size, key) || !decoder.GetBytes(value_size, value) ||
        !decoder.empty() || (deleted != 0 && !value.empty()))
    {
        return Status{StatusCode::Corruption, "invalid SSTable record payload"};
    }

    *record = SSTableRecord(Key(key.begin(), key.end()), sequence, deleted != 0,
                            Value(value.begin(), value.end()));
    return Status{StatusCode::Ok};
}

} // namespace kvstore::core::sstable
