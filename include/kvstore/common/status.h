#pragma once

#include <cstdint>
#include <string>

namespace kvstore
{

//指定底层是一个字节 ---> 节省空间 
enum class StatusCode : uint8_t 
{
    Ok,
    NotFound,
    IOError,
    Corruption,
    InvalidArgument,
    InvalidState,
    InvalidConfig,
    ResourceExhausted
};

class Status 
{
public:
    explicit Status(StatusCode code, std::string error = "") noexcept : code_(code), error_(std::move(error)) {}

    [[nodiscard]] constexpr bool ok() const noexcept { return code_ == StatusCode::Ok; }

    [[nodiscard]] constexpr bool is_not_found() const noexcept { return code_ == StatusCode::NotFound; }

    [[nodiscard]] constexpr const std::string& Error() const noexcept { return error_; }

    [[nodiscard]] constexpr StatusCode Code() const noexcept { return code_; }

private:
    StatusCode code_;

    std::string error_;
};

}  // namespace kvstore::command