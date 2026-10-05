#pragma once

#include <cstdint>
#include <string>

namespace kvstore
{

//指定底层是一个字节 ---> 节省空间 
enum class StatusCode : uint8_t 
{
    Ok,

    // 业务/查询
    NotFound,

    // 底层 I/O
    IOError,

    // 数据损坏
    Corruption,

    // 对象状态错误
    InvalidState,

    // 配置问题
    InvalidConfig
};

class Status 
{
public:
    explicit Status(StatusCode code, std::string error = "") noexcept : code_(code), error_(std::move(error)) {}

    [[nodiscard]] constexpr bool ok() const noexcept { return code_ == StatusCode::Ok; }

    [[nodiscard]] constexpr bool is_not_found() const noexcept { return code_ == StatusCode::NotFound; }

    [[nodiscard]] constexpr const std::string& Error() const noexcept { return error_; }
private:
    StatusCode code_;

    std::string error_;
};

}  // namespace kvstore::command