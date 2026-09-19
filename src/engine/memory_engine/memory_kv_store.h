#pragma once

//agent_kv::api::IKVStore里面有string optional string_view
#include "../../../include/agent_kv/kv_store.h"
#include <unordered_map>

namespace agent_kv::engine::memory_engine 
{

class MemoryKVStore : public agent_kv::api::IKVStore
{
public:
    MemoryKVStore() = default;
    ~MemoryKVStore() override = default;

    void Put(std::string_view key, std::string_view value) override;
    std::optional<std::string> Get(std::string_view key) const override;
    bool Delete(std::string_view key) override;
    size_t Size() const override;

private:
    std::unordered_map<std::string, std::string> data_;

};

} //namespace memory_engine