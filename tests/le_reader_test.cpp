// SPDX-License-Identifier: GPL-3.0-only
#include "world/le_reader.hpp"

#include "check.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

std::vector<std::byte> bytesOf(std::initializer_list<unsigned> values) {
    std::vector<std::byte> result;
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

} // namespace

int main() {
    using namespace eggbert;
    constexpr auto sizeMax = std::numeric_limits<std::size_t>::max();

    const auto data = bytesOf({0x34, 0x12, 0xFF, 0xFF, 0x00, 0x80, 0xFE, 0xFF, 0xFF, 0x7F,
                               0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x80, 0xFF});
    const auto snapshot = data;
    const std::span<const std::byte> view(data);

    // Little-endian byte order and both signed interpretations.
    CHECK_EQ(readU16(view, 0).value(), std::uint16_t{0x1234});
    CHECK_EQ(readI16(view, 0).value(), std::int16_t{0x1234});
    CHECK_EQ(readU16(view, 2).value(), std::uint16_t{0xFFFF});
    CHECK_EQ(readI16(view, 2).value(), std::int16_t{-1});
    CHECK_EQ(readU16(view, 4).value(), std::uint16_t{0x8000});
    CHECK_EQ(readI16(view, 4).value(), std::numeric_limits<std::int16_t>::min());
    CHECK_EQ(readI16(view, 6).value(), std::int16_t{-2});
    CHECK_EQ(readI16(view, 8).value(), std::numeric_limits<std::int16_t>::max());
    CHECK_EQ(readU32(view, 10).value(), std::uint32_t{0x12345678});
    CHECK_EQ(readI32(view, 10).value(), std::int32_t{0x12345678});
    CHECK_EQ(readU32(view, 14).value(), std::uint32_t{0x80000000});
    CHECK_EQ(readI32(view, 14).value(), std::numeric_limits<std::int32_t>::min());
    CHECK_EQ(readI32(view, 2).value(), std::int32_t{-2147418113}); // FF FF 00 80
    CHECK_EQ(readU32(view, 6).value(), std::uint32_t{0x7FFFFFFE});
    // Unaligned reads are ordinary.
    CHECK_EQ(readU16(view, 1).value(), std::uint16_t{0xFF12});
    CHECK_EQ(readU32(view, 15).value(), std::uint32_t{0xFF800000});

    // The last complete value of each width succeeds; one byte later fails
    // and names the first unavailable offset.
    const std::size_t size = data.size(); // 19
    CHECK(readU16(view, size - 2).has_value());
    CHECK(readI16(view, size - 2).has_value());
    CHECK(readU32(view, size - 4).has_value());
    CHECK(readI32(view, size - 4).has_value());
    CHECK((readU16(view, size - 1).error() == ReadError{size - 1, 2, size}));
    CHECK((readI16(view, size - 1).error() == ReadError{size - 1, 2, size}));
    for (std::size_t short_by = 1; short_by <= 3; ++short_by) {
        const auto offset = size - 4 + short_by;
        CHECK((readU32(view, offset).error() == ReadError{offset, 4, size}));
        CHECK((readI32(view, offset).error() == ReadError{offset, 4, size}));
    }
    // Reads that start at or beyond the end report the requested offset.
    CHECK((readU16(view, size).error() == ReadError{size, 2, size}));
    CHECK((readU32(view, size + 7).error() == ReadError{size + 7, 4, size + 7}));
    // Offsets near the top of the address space must not wrap around.
    CHECK((readU16(view, sizeMax).error() == ReadError{sizeMax, 2, sizeMax}));
    CHECK((readI32(view, sizeMax - 1).error() == ReadError{sizeMax - 1, 4, sizeMax - 1}));
    CHECK((readU32(view, sizeMax - 3).error() == ReadError{sizeMax - 3, 4, sizeMax - 3}));

    // Empty and single-byte inputs.
    const std::span<const std::byte> empty;
    CHECK((readU16(empty, 0).error() == ReadError{0, 2, 0}));
    CHECK((readI32(empty, 0).error() == ReadError{0, 4, 0}));
    CHECK((readU16(view.first(1), 0).error() == ReadError{0, 2, 1}));
    CHECK((readU32(view.first(3), 0).error() == ReadError{0, 4, 3}));

    // rangeFits is the shared overflow-safe bound check.
    CHECK(rangeFits(10, 0, 10));
    CHECK(rangeFits(10, 10, 0));
    CHECK(!rangeFits(10, 11, 0));
    CHECK(!rangeFits(10, 9, 2));
    CHECK(!rangeFits(10, 1, sizeMax));
    CHECK(!rangeFits(10, sizeMax, 1));
    CHECK(!rangeFits(sizeMax, 2, sizeMax - 1));
    CHECK(rangeFits(sizeMax, 1, sizeMax - 1));

    // Reads are usable in constant expressions and never touch the input.
    static constexpr std::array<std::byte, 4> fixed{std::byte{0xFE}, std::byte{0xFF},
                                                    std::byte{0xFF}, std::byte{0xFF}};
    static_assert(readI32(fixed, 0).value() == -2);
    static_assert(!readI32(fixed, 1).has_value());
    CHECK(std::ranges::equal(data, snapshot));
    return test::finish("le_reader");
}
