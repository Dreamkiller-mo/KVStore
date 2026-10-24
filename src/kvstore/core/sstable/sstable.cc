#include <kvstore/common/codec.h>
#include <kvstore/core/sstable/sstable.h>
#include <kvstore/core/sstable/sstable_builder.h>
#include <kvstore/core/sstable/sstable_codec.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>

namespace kvstore::core::sstable
{
namespace
{

constexpr std::size_t kRecordLengthSize = sizeof(std::uint32_t);
constexpr std::size_t kMinimumRecordBodySize = sizeof(std::uint32_t) * 2 + sizeof(std::uint64_t) + sizeof(std::uint8_t);

bool ReadUint32(std::istream& input, std::uint32_t* value)
{
    std::array<std::uint8_t, sizeof(std::uint32_t)> bytes{};
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
    {
        return false;
    }
    common::Decoder decoder(bytes);
    return decoder.GetUint32(*value);
}

bool RecordsEqual(const SSTableRecord& left, const SSTableRecord& right)
{
    return left.key_ == right.key_ && left.sequence_ == right.sequence_ &&
           left.deleted_ == right.deleted_ && left.value_ == right.value_;
}

} // namespace

Status SSTable::Open(std::filesystem::path path)
{
    index_.clear();
    open_ = false;
    path_ = std::move(path);

    std::error_code error;
    const auto file_size = std::filesystem::file_size(path_, error);
    if (error)
    {
        return Status{StatusCode::IOError, "failed to get SSTable file size: " + error.message()};
    }
    if (file_size < SSTableCodec::kHeaderSize)
    {
        return Status{StatusCode::Corruption, "SSTable file is shorter than its header"};
    }

    std::ifstream file(path_, std::ios::binary);
    if (!file)
    {
        return Status{StatusCode::IOError, "failed to open SSTable input"};
    }

    std::array<std::uint8_t, SSTableCodec::kHeaderSize> header{};
    if (!file.read(reinterpret_cast<char*>(header.data()), header.size()))
    {
        return Status{StatusCode::IOError, "failed to read SSTable header"};
    }
    std::uint64_t record_count = 0;
    auto status = SSTableCodec::DecodeHeader(header, &record_count);
    if (!status.ok())
    {
        return status;
    }
    if (record_count > (file_size - SSTableCodec::kHeaderSize) /
                           (kRecordLengthSize + kMinimumRecordBodySize))
    {
        return Status{StatusCode::Corruption, "SSTable record count exceeds file contents"};
    }

    std::uint64_t offset = SSTableCodec::kHeaderSize;
    std::vector<std::uint8_t> body;
    Key previous_key;
    Sequence previous_sequence = 0;
    bool has_previous = false;
    for (std::uint64_t i = 0; i < record_count; ++i)
    {
        std::uint32_t body_size = 0;
        if (!ReadUint32(file, &body_size) || body_size < kMinimumRecordBodySize ||
            body_size > file_size - offset - kRecordLengthSize)
        {
            return Status{StatusCode::Corruption, "invalid SSTable record length"};
        }
        body.resize(body_size);
        if (!file.read(reinterpret_cast<char*>(body.data()), body.size()))
        {
            return Status{StatusCode::Corruption, "truncated SSTable record"};
        }

        SSTableRecord record;
        status = SSTableCodec::DecodeRecord(body, &record);
        if (!status.ok())
        {
            return status;
        }
        if (has_previous &&
            (record.key_ < previous_key ||
             (record.key_ == previous_key && record.sequence_ >= previous_sequence)))
        {
            return Status{StatusCode::Corruption, "SSTable records are not uniquely sorted"};
        }

        index_.push_back({record.key_, record.sequence_, offset});
        previous_key = std::move(record.key_);
        previous_sequence = record.sequence_;
        has_previous = true;
        offset += kRecordLengthSize + body_size;
    }
    if (offset != file_size)
    {
        return Status{StatusCode::Corruption, "SSTable contains trailing bytes"};
    }

    open_ = true;
    return Status{StatusCode::Ok};
}

Status SSTable::Lookup(const Key& key, SSTableRecord* record, Sequence snapshot) const
{
    if (record == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "record output must not be null"};
    }
    if (!open_)
    {
        return Status{StatusCode::InvalidState, "SSTable is not open"};
    }

    auto entry = std::lower_bound(index_.begin(), index_.end(), key,
        [](const IndexEntry& candidate, const Key& sought) { return candidate.key < sought; });
    for (; entry != index_.end() && entry->key == key; ++entry)
    {
        if (entry->sequence <= snapshot)
        {
            return ReadRecordAt(entry->offset, record);
        }
    }
    return Status{StatusCode::NotFound, "key has no version at the requested sequence"};
}

Status SSTable::Get(const Key& key, Value* value, Sequence snapshot) const
{
    if (value == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "value output must not be null"};
    }
    SSTableRecord record;
    auto status = Lookup(key, &record, snapshot);
    if (!status.ok())
    {
        return status;
    }
    if (record.deleted_)
    {
        return Status{StatusCode::NotFound, "key is deleted"};
    }
    *value = std::move(record.value_);
    return Status{StatusCode::Ok};
}

Status SSTable::ReadAll(std::vector<SSTableRecord>* records) const
{
    if (records == nullptr)
    {
        return Status{StatusCode::InvalidArgument, "records output must not be null"};
    }
    if (!open_)
    {
        return Status{StatusCode::InvalidState, "SSTable is not open"};
    }
    records->clear();
    records->reserve(index_.size());
    for (const auto& entry : index_)
    {
        SSTableRecord record;
        auto status = ReadRecordAt(entry.offset, &record);
        if (!status.ok())
        {
            records->clear();
            return status;
        }
        records->push_back(std::move(record));
    }
    return Status{StatusCode::Ok};
}

Status SSTable::Compact(const std::vector<const SSTable*>& inputs,
                        const std::filesystem::path& output_path)
{
    std::vector<SSTableRecord> records;
    for (const auto* input : inputs)
    {
        if (input == nullptr)
        {
            return Status{StatusCode::InvalidArgument, "compaction input must not be null"};
        }
        std::vector<SSTableRecord> table_records;
        auto status = input->ReadAll(&table_records);
        if (!status.ok())
        {
            return status;
        }
        records.insert(records.end(), std::make_move_iterator(table_records.begin()),
                       std::make_move_iterator(table_records.end()));
    }

    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right)
    {
        if (left.key_ != right.key_)
        {
            return left.key_ < right.key_;
        }
        return left.sequence_ > right.sequence_;
    });
    std::vector<SSTableRecord> unique_records;
    unique_records.reserve(records.size());
    for (auto& record : records)
    {
        if (!unique_records.empty() && unique_records.back().key_ == record.key_ &&
            unique_records.back().sequence_ == record.sequence_)
        {
            if (!RecordsEqual(unique_records.back(), record))
            {
                return Status{StatusCode::Corruption, "conflicting records share a sequence"};
            }
            continue;
        }
        unique_records.push_back(std::move(record));
    }

    SSTableBuilder builder(output_path);
    for (auto& record : unique_records)
    {
        auto status = builder.Add(std::move(record));
        if (!status.ok())
        {
            return status;
        }
    }
    return builder.Finish();
}

Status SSTable::ReadRecordAt(std::uint64_t offset, SSTableRecord* record) const
{
    std::ifstream file(path_, std::ios::binary);
    if (!file)
    {
        return Status{StatusCode::IOError, "failed to open SSTable input"};
    }
    file.seekg(static_cast<std::streamoff>(offset));
    std::uint32_t body_size = 0;
    if (!ReadUint32(file, &body_size) || body_size < kMinimumRecordBodySize)
    {
        return Status{StatusCode::Corruption, "invalid SSTable record length"};
    }

    std::vector<std::uint8_t> body(body_size);
    if (!file.read(reinterpret_cast<char*>(body.data()), body.size()))
    {
        return Status{StatusCode::Corruption, "truncated SSTable record"};
    }
    return SSTableCodec::DecodeRecord(body, record);
}

} // namespace kvstore::core::sstable
