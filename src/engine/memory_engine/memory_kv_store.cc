#include "memory_kv_store.h"

using namespace agent_kv::engine::memory_engine;

void MemoryKVStore::Put(std::string_view key, std::string_view value)
{
    data_[std::string(key)] = std::string(value);
}

std::optional<std::string> MemoryKVStore::Get(std::string_view key) const
{
    auto it = data_.find(std::string(key));

    if(it != data_.end()) { return it->second; }
    
    return std::nullopt;

} 

bool MemoryKVStore::Delete(std::string_view key)
{
    //unoreder_map return_value is size_t; 1 -> success 0 -> no
    return data_.erase(std::string(key)) > 0;
}

std::size_t MemoryKVStore::Size() const
{
    return data_.size();
}