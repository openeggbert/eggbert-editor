// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace eggbert {

// A read that did not fit inside the input. Nothing was read or modified.
struct ReadError {
    std::size_t requestedOffset = 0;
    std::size_t requestedBytes = 0;
    // First byte of the requested range that the input does not contain.
    std::size_t firstUnavailableOffset = 0;

    friend constexpr bool operator==(const ReadError &, const ReadError &) = default;
};

template <typename Value>
using ReadResult = std::expected<Value, ReadError>;

// True when [offset, offset + count) lies inside an input of `size` bytes.
// Written without additions so that it cannot overflow.
[[nodiscard]] constexpr bool rangeFits(std::size_t size, std::size_t offset,
                                       std::size_t count) noexcept {
    return offset <= size && count <= size - offset;
}

namespace detail {

template <typename Unsigned>
[[nodiscard]] constexpr ReadResult<Unsigned> readLittleEndian(std::span<const std::byte> bytes,
                                                              std::size_t offset) noexcept {
    if (!rangeFits(bytes.size(), offset, sizeof(Unsigned))) {
        return std::unexpected(ReadError{
            offset, sizeof(Unsigned), offset > bytes.size() ? offset : bytes.size()});
    }
    Unsigned value = 0;
    for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
        value = static_cast<Unsigned>(
            value | static_cast<Unsigned>(std::to_integer<Unsigned>(bytes[offset + index])
                                          << (8 * index)));
    }
    return value;
}

} // namespace detail

[[nodiscard]] constexpr ReadResult<std::uint16_t> readU16(std::span<const std::byte> bytes,
                                                          std::size_t offset) noexcept {
    return detail::readLittleEndian<std::uint16_t>(bytes, offset);
}

[[nodiscard]] constexpr ReadResult<std::uint32_t> readU32(std::span<const std::byte> bytes,
                                                          std::size_t offset) noexcept {
    return detail::readLittleEndian<std::uint32_t>(bytes, offset);
}

// Two's-complement reinterpretation; well defined since C++20.
[[nodiscard]] constexpr ReadResult<std::int16_t> readI16(std::span<const std::byte> bytes,
                                                         std::size_t offset) noexcept {
    return readU16(bytes, offset).transform([](std::uint16_t value) {
        return static_cast<std::int16_t>(value);
    });
}

[[nodiscard]] constexpr ReadResult<std::int32_t> readI32(std::span<const std::byte> bytes,
                                                         std::size_t offset) noexcept {
    return readU32(bytes, offset).transform([](std::uint32_t value) {
        return static_cast<std::int32_t>(value);
    });
}

} // namespace eggbert
