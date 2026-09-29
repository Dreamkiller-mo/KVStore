#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <cstdint>
#include <span>

namespace kvstore::core::wal
{

class Encoder
{
public:

    explicit Encoder( std::vector<uint8_t>& buffer ) : buffer_(buffer) {}

    void PutUint64(uint64_t value);

    void PutUint64At(std::size_t offset, std::uint64_t value);

    void PutUint32(uint32_t value);

    void PutUint8(uint8_t value);

    void PutBytes(std::span<const uint8_t> data);

    [[nodiscard]] decltype(auto) Size() const { return buffer_.size(); }

    [[nodiscard]] decltype(auto) View(size_t begin, size_t end) const { return std::span<const uint8_t>(buffer_.begin(),buffer_.end()); }

private:

    std::vector<uint8_t>& buffer_;
};

}   //namespace kvstore::core::wak
