#pragma once

#include <kvstore/common/status.h>
#include <kvstore/common/types.h>
#include <kvstore/core/sstable/sstable_record.h>

#include <filesystem>
#include <limits>
#include <vector>

namespace kvstore::core::sstable
{

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

class SSTable
{
public:
    // open file and check file , scane record and build index
    [[nodiscard]] Status Open(std::filesystem::path path);

    // according ti key and record version to find the corresponding snapshot
    [[nodiscard]] Status Lookup( const Key& key, SSTableRecord* record, Sequence snapshot = std::numeric_limits<Sequence>::max()) const;

    // check out the record according to key
    [[nodiscard]] Status Get( const Key& key, Value* value, Sequence snapshot = std::numeric_limits<Sequence>::max()) const;

    // read all record of the SSTable
    [[nodiscard]] Status ReadAll(std::vector<SSTableRecord>* records) const;

    [[nodiscard]] bool IsOpen() const noexcept { return open_; }
    [[nodiscard]] std::size_t Size() const noexcept { return index_.size(); }

    [[nodiscard]] static Status Compact(const std::vector<const SSTable*>& inputs, const std::filesystem::path& output_path);

private:
    struct IndexEntry
    {
        Key key;
        Sequence sequence;
        std::uint64_t offset;
    };

    // accroding to offset , find and encode the corresponding bytes -> record
    [[nodiscard]] Status ReadRecordAt(std::uint64_t offset, SSTableRecord* record) const;

    std::filesystem::path path_;
    std::vector<IndexEntry> index_;
    bool open_{false};
};

} // namespace kvstore::core::sstable