#include "memory_kv_store.h"

using namespace agent_kv::engine::memory_engine;

void MemoryKVStore::Put(std::string_view key, std::string_view value)
{
    // 这里必须发生一次拷贝（因为我们要接管所有权存储起来）
    // 但 insert_or_assign 依然比 operator[] 少一次默认构造
    data_.insert_or_assign(std::string(key), std::string(value));
}

std::optional<std::string> MemoryKVStore::Get(std::string_view key) const
{
    // C++20 异构查找（P0919R3）：直接用 string_view 查，无临时 std::string 构造！
    auto it = data_.find(key);

    if (it != data_.end()) { return it->second; } //copy return
    return std::nullopt;
}

bool MemoryKVStore::Delete(std::string_view key)
{
    // 注意：unordered_map 的异构 erase 属于 C++23（P2077R3），
    // 且 libstdc++ 13 至今未实现，直接 data_.erase(key) 会编译失败。
    // 改用 C++20 的异构 find + erase(iterator)，同样不构造临时 std::string。
    auto it = data_.find(key);
    if (it == data_.end()) { return false; }
    data_.erase(it);
    return true;
}

std::size_t MemoryKVStore::Size() const
{
    return data_.size();
}
