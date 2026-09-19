#pragma once

#include <cstddef>          //std::size_t
#include <string>
#include <string_view>
#include <optional>
#include <memory>

namespace agent_kv::api
{

//抽象接口
class IKVStore
{
public:
    virtual ~IKVStore() = default;

    //API

    virtual void Put(std::string_view key, std::string_view value) = 0;
    virtual std::optional<std::string> Get(std::string_view key) const = 0;
    virtual bool Delete(std::string_view key) = 0;
    virtual std::size_t Size() const = 0;


};

} //namespace agent_kv