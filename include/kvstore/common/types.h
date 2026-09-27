#pragma once

#include <cstdint>
#include <vector>

namespace kvstore
{
    using Key = std::vector<uint8_t>;
    using Value = std::vector<uint8_t>;
    using Sequence = std::uint64_t;
}   // namespace kvstore::types