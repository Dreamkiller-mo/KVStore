#include <kvstore/core/wal/crc32c.h>
#include <absl/crc/crc32c.h>

using namespace kvstore::core::wal;

std::uint32_t CRC32C::Compute(std::span<const std::uint8_t> data)
{
    auto crc = absl::ComputeCrc32c(absl::string_view(reinterpret_cast<const char*>(data.data()), data.size()));

    return static_cast<std::uint32_t>(crc);
}