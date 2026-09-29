#include <cstddef>
#include <kvstore/core/wal/encoder.h>
#include <cstdint>
#include <span>

using namespace kvstore::core::wal;

void Encoder::PutUint64(uint64_t value)
{
    // 小端序 64位
    for (int i = 0; i < 8; i++)
    {
        buffer_.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
}

void Encoder::PutUint32(uint32_t value)
{
    // 小端序 32位
    for (int i = 0; i < 4; i++)
    {
        buffer_.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
}

void Encoder::PutUint64At(std::size_t offset, std::uint64_t value)
{
    for(int i = 0; i < 8; i++)
    {
        buffer_[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

void Encoder::PutUint8(uint8_t value)
{
    buffer_.push_back(value);
}

void Encoder::PutBytes(std::span<const uint8_t> data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}
