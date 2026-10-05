#pragma once

#include <kvstore/common/status.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace kvstore::storage
{

class File
{
public:
    File() = default;
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    [[nodiscard]] Status Open(std::string_view path);

    [[nodiscard]] Status Append(std::span<const std::uint8_t> data);

    [[nodiscard]] Status Sync();

    [[nodiscard]] bool IsOpen() const noexcept;

    void Close() noexcept;

private:
    int fd_{-1};
};

} // namespace kvstore::storage