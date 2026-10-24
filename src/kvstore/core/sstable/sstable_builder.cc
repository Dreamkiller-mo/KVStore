#include <kvstore/common/codec.h>
#include <kvstore/core/sstable/sstable_builder.h>
#include <kvstore/core/sstable/sstable_codec.h>

#include <algorithm>
#include <fstream>
#include <limits>

namespace kvstore::core::sstable
{
namespace
{

bool RecordLess(const SSTableRecord& left, const SSTableRecord& right)
{
    if (left.key_ != right.key_)
    {
        return left.key_ < right.key_;
    }
    return left.sequence_ > right.sequence_;
}

} // namespace

SSTableBuilder::SSTableBuilder(std::filesystem::path path) : path_(std::move(path)) {}

Status SSTableBuilder::Add(const SSTableRecord& record)
{
    // check out the state
    if (finished_)
    {
        return Status{StatusCode::InvalidState, "cannot add records after SSTable finish"};
    }
    if ((record.deleted_ && !record.value_.empty()) ||
        record.key_.size() > std::numeric_limits<std::uint32_t>::max() ||
        record.value_.size() > std::numeric_limits<std::uint32_t>::max() ||
        record.key_.size() + record.value_.size() > std::numeric_limits<std::uint32_t>::max() - 17ULL
        )
    {
        return Status{StatusCode::InvalidArgument, "SSTable record is invalid or exceeds the format size limit"};
    }

    // storage the record
    records_.push_back(record);
    return Status{StatusCode::Ok};
}

Status SSTableBuilder::Add(SSTableRecord&& record)
{
    if (finished_)
    {
        return Status{StatusCode::InvalidState, "cannot add records after SSTable finish"};
    }
    if ((record.deleted_ && !record.value_.empty()) ||
        record.key_.size() > std::numeric_limits<std::uint32_t>::max() ||
        record.value_.size() > std::numeric_limits<std::uint32_t>::max() ||
        record.key_.size() + record.value_.size() > std::numeric_limits<std::uint32_t>::max() - 17ULL
        )
    {
        return Status{StatusCode::InvalidArgument, "SSTable record is invalid or exceeds the format size limit"};
    }
    records_.push_back(std::move(record));
    return Status{StatusCode::Ok};
}

Status SSTableBuilder::Finish()
{
    if (finished_)
    {
        return Status{StatusCode::InvalidState, "SSTable builder has already finished"};
    }

    // sort record with 
    std::sort(records_.begin(), records_.end(), RecordLess);

    // reject record with the same as key and sequence
    for (std::size_t i = 1; i < records_.size(); ++i)
    {
        if (records_[i - 1].key_ == records_[i].key_ && records_[i - 1].sequence_ == records_[i].sequence_)
        {
            return Status{StatusCode::InvalidArgument, "duplicate key and sequence in SSTable"};
        }
    }

    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return Status{StatusCode::IOError, "failed to open SSTable output"};
    }

    const auto header = SSTableCodec::EncodeHeader(records_.size());
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));

    std::vector<std::uint8_t> encoded_record;
    for (const auto& record : records_)
    {
        auto status = SSTableCodec::EncodeRecord(record, &encoded_record);
        if (!status.ok())
        {
            return status;
        }
        file.write(reinterpret_cast<const char*>(encoded_record.data()),
                   static_cast<std::streamsize>(encoded_record.size()));
        if (!file)
        {
            return Status{StatusCode::IOError, "failed to write SSTable record"};
        }
    }
    
    file.flush();
    if (!file)
    {
        return Status{StatusCode::IOError, "failed to flush SSTable output"};
    }

    finished_ = true;
    return Status{StatusCode::Ok};
}

} // namespace kvstore::core::sstable
