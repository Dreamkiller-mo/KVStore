#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace kvstore::common
{

class Encoder
{
public:
    explicit Encoder(std::vector<std::uint8_t>& buffer) noexcept;

    void PutUint64(std::uint64_t value);
    void PutUint64At(std::size_t offset, std::uint64_t value);
    void PutUint32(std::uint32_t value);
    void PutUint8(std::uint8_t value);
    void PutBytes(std::span<const std::uint8_t> data);

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> View(std::size_t begin, std::size_t end) const noexcept;

private:
    // encode should operate buffer -> &
    std::vector<std::uint8_t>& buffer_;
};

class Decoder
{
public:
    explicit Decoder(std::span<const std::uint8_t> data) noexcept;

    [[nodiscard]] bool GetUint64(std::uint64_t& value) noexcept;
    [[nodiscard]] bool GetUint32(std::uint32_t& value) noexcept;
    [[nodiscard]] bool GetUint8(std::uint8_t& value) noexcept;
    [[nodiscard]] bool GetBytes(std::size_t size,
                                std::span<const std::uint8_t>& bytes) noexcept;
    [[nodiscard]] std::size_t position() const noexcept;
    [[nodiscard]] std::size_t remaining() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    // decode not should operate data -> span + const
    std::span<const std::uint8_t> data_;
    std::size_t position_ {0};
};

} // namespace kvstore::common
