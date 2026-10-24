#include "kvstore/common/codec.h"

namespace kvstore::common
{

Encoder::Encoder(std::vector<std::uint8_t>& buffer) noexcept : buffer_(buffer) {}

void Encoder::PutUint64(std::uint64_t value)
{
    for (unsigned int i = 0; i < 8; ++i)
    {
        buffer_.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }
}

void Encoder::PutUint64At(std::size_t offset, std::uint64_t value)
{
    for (unsigned int i = 0; i < 8; ++i)
    {
        buffer_[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

void Encoder::PutUint32(std::uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i)
    {
        buffer_.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }
}

void Encoder::PutUint8(std::uint8_t value)
{
    buffer_.push_back(value);
}

void Encoder::PutBytes(std::span<const std::uint8_t> data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

std::size_t Encoder::Size() const noexcept
{
    return buffer_.size();
}

std::span<const std::uint8_t> Encoder::View(std::size_t begin, std::size_t end) const noexcept
{
    if (begin > end || end > buffer_.size())
    {
        return {};
    }
    return std::span<const std::uint8_t>(buffer_).subspan(begin, end - begin);
}

Decoder::Decoder(std::span<const std::uint8_t> data) noexcept : data_(data) {}

bool Decoder::GetUint64(std::uint64_t& value) noexcept
{
    if (remaining() < 8)
    {
        return false;
    }
    value = 0;
    for (unsigned int i = 0; i < 8; ++i)
    {
        value |= static_cast<std::uint64_t>(data_[position_++]) << (i * 8);
    }
    return true;
}

bool Decoder::GetUint32(std::uint32_t& value) noexcept
{
    if (remaining() < 4)
    {
        return false;
    }
    value = 0;
    for (unsigned int i = 0; i < 4; ++i)
    {
        value |= static_cast<std::uint32_t>(data_[position_++]) << (i * 8);
    }
    return true;
}

bool Decoder::GetUint8(std::uint8_t& value) noexcept
{
    if (remaining() < 1)
    {
        return false;
    }
    value = data_[position_++];
    return true;
}

bool Decoder::GetBytes(std::size_t size, std::span<const std::uint8_t>& bytes) noexcept
{
    if (size > remaining())
    {
        return false;
    }
    bytes = data_.subspan(position_, size);
    position_ += size;
    return true;
}

std::size_t Decoder::position() const noexcept
{
    return position_;
}

std::size_t Decoder::remaining() const noexcept
{
    return data_.size() - position_;
}

bool Decoder::empty() const noexcept
{
    return remaining() == 0;
}

} // namespace kvstore::common
