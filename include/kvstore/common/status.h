#pragma once

#include <cstdint>

namespace kvstore
{

//指定底层是一个字节 ---> 节省空间 
enum class StatusCode : uint8_t 
{
    Ok,
    NotFound
};

class Status 
{
public:
    constexpr explicit Status(StatusCode code) noexcept : code_(code) {}

    [[nodiscard]] constexpr bool ok() const noexcept { return code_ == StatusCode::Ok; }

    [[nodiscard]] constexpr bool is_not_found() const noexcept { return code_ == StatusCode::NotFound; }

private:
    StatusCode code_;
};

}  // namespace kvstore::command