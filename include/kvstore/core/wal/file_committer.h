#include <kvstore/core/wal/committer.h>
#include <kvstore/storage/file.h>

namespace kvstore::core::wal 
{

class FileCommitter : public ICommitter
{
public:
    explicit FileCommitter(storage::File file);

    [[nodiscard]] Status Commit(std::span<const std::uint8_t> data) override;

    ~FileCommitter() override = default;
    
private:
    kvstore::storage::File file_;
};

}   // namespace kvstore::core::wal 
