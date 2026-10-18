#include <kvstore/core/memtable/mem_record.h>
#include <kvstore/core/memtable/i_memtable.h>
#include <kvstore/core/memtable/SkipList.h>

namespace kvstore::core::memtable
{

class SkipMemTable : public IMemTable
{
public:
    SkipMemTable() = default;
    ~SkipMemTable() override = default;

    [[nodiscard]] Status Put(const Key& key, const Value& value, Sequence sequence) override;
    
    [[nodiscard]] Status Get(const Key& key, const Value** value) const override;

    [[nodiscard]] Status Delete(const Key& key, Sequence sequence) override;

    std::size_t Size() const override;

    [[nodiscard]] Status Put(const Key& key, Value&& value, Sequence sequence);

private:
    SkipList<Key, MemTableRecord> skip_list_;
};

}   // namespace kvstore::core::memtable
