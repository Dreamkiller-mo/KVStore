#pragma once

//agent_kv::api::IKVStore里面有string optional string_view
#include "../../../include/agent_kv/kv_store.h"
#include <unordered_map>

namespace agent_kv::engine::memory_engine 
{

// 自定义透明哈希
struct StringHash 
{
    // 开启异构查找的开关
    using is_transparent = void; 

    // 针对 string_view 的哈希（string 会自动转换为 string_view）
    std::size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    
    // 针对 const char* 的哈希
    std::size_t operator()(const char* str) const { return std::hash<std::string_view>{}(str); }
};

// 自定义透明相等谓词
struct StringEqual 
{
    using is_transparent = void;

    // 只要支持 string_view 的比较，std::string 和 string_view 之间的比较也能隐式推导
    bool operator()(std::string_view lhs, std::string_view rhs) const { return lhs == rhs; }
};

class MemoryKVStore : public agent_kv::api::IKVStore
{
public:
    using Map = std::unordered_map<std::string, std::string, StringHash, StringEqual>;

    MemoryKVStore() = default;
    ~MemoryKVStore() override = default;

    void Put(std::string_view key, std::string_view value) override;
    std::optional<std::string> Get(std::string_view key) const override;
    bool Delete(std::string_view key) override;
    size_t Size() const override;

private:
    Map data_;
};

} //namespace memory_engine