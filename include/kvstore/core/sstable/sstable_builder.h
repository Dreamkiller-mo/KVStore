#pragma once

#include <kvstore/common/status.h>
#include <kvstore/core/sstable/sstable_record.h>

#include <filesystem>
#include <vector>

namespace kvstore::core::sstable
{

class SSTable;

class SSTableBuilder
{
/*

SSTableBuilder's core responsibilities : it take responsibility for a batch records to organized and write int the disk
and build a SSTable format file

*/
public:
    explicit SSTableBuilder(std::filesystem::path path);
    
    [[nodiscard]] Status Add(const SSTableRecord& record);
    [[nodiscard]] Status Add(SSTableRecord&& record);
    [[nodiscard]] Status Finish();

private:
    std::filesystem::path path_;
    std::vector<SSTableRecord> records_;
    bool finished_{false};
};

} // namespace kvstore::core::sstable