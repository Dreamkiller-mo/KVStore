#pragma once

#include <cstdint>
#include <span>

namespace kvstore::core::wal
{

class CRC32
{
public:

    [[nodiscard]] static std::uint32_t Compute(std::span<const std::uint8_t> data);
};

}