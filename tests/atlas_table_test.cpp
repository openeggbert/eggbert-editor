// SPDX-License-Identifier: GPL-3.0-only
#include "world/atlas_table.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <array>

using namespace eggbert;
using namespace eggbert::test;

namespace {

// Appends a hand-encoded table: count, then six 16-bit values per sprite.
void appendTable(Bytes &bytes, const std::vector<SpriteRect> &sprites, int declaredCount = -1,
                 bool dropLastHeight = false) {
    const auto put = [&](int value) {
        bytes.push_back(static_cast<std::byte>(value & 0xFF));
        bytes.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
    };
    put(declaredCount < 0 ? static_cast<int>(sprites.size()) : declaredCount);
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const auto &sprite = sprites[index];
        put(sprite.srcX);
        put(sprite.srcY);
        put(sprite.offsetX);
        put(sprite.offsetY);
        put(sprite.width);
        if (!(dropLastHeight && index + 1 == sprites.size())) {
            put(sprite.height);
        }
    }
}

// Noise that cannot contain an accidental table: bytes 0xE0..0xFF make
// every 16-bit value negative.
void appendNoise(Bytes &bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        bytes.push_back(static_cast<std::byte>(0xE0 + (index * 7) % 32));
    }
}

// A plausible packed sheet: sprites of varying size on a 256 x 256 sheet,
// each inside a 64 x 64 logical cell.
std::vector<SpriteRect> sampleSprites(std::size_t count) {
    std::vector<SpriteRect> sprites;
    for (std::size_t index = 0; index < count; ++index) {
        SpriteRect sprite;
        sprite.width = static_cast<std::int16_t>(20 + (index * 5) % 44);
        sprite.height = static_cast<std::int16_t>(16 + (index * 11) % 48);
        sprite.offsetX = static_cast<std::int16_t>((64 - sprite.width) / 2);
        sprite.offsetY = static_cast<std::int16_t>(64 - sprite.height);
        sprite.srcX = static_cast<std::int16_t>((index * 37) % (256 - sprite.width));
        sprite.srcY = static_cast<std::int16_t>((index * 53) % (256 - sprite.height));
        sprites.push_back(sprite);
    }
    return sprites;
}

constexpr AtlasSearch search20{20, 256, 256, 64, 64};

} // namespace

int main() {
    const auto sprites = sampleSprites(20);

    // Found at an arbitrary, odd offset inside unrelated bytes.
    {
        Bytes bytes;
        appendNoise(bytes, 1001);
        const auto offset = bytes.size();
        appendTable(bytes, sprites);
        appendNoise(bytes, 300);
        const auto snapshot = bytes;
        const auto table = findAtlasTable(bytes, search20);
        CHECK(table.has_value());
        if (table) {
            CHECK_EQ(table->fileOffset, offset);
            CHECK(table->sprites == sprites);
            CHECK(!table->lastHeightAdjusted);
        }
        CHECK(bytes == snapshot);
    }
    // At the very start and at the very end of the bytes.
    {
        Bytes atStart;
        appendTable(atStart, sprites);
        CHECK_EQ(findAtlasTable(atStart, search20)->fileOffset, std::size_t{0});
        Bytes atEnd;
        appendNoise(atEnd, 64);
        appendTable(atEnd, sprites);
        CHECK(findAtlasTable(atEnd, search20).has_value());
        // One byte short of a full table: not found, no read past the end.
        atEnd.pop_back();
        CHECK(findAtlasTable(atEnd, search20).error() == AtlasSearchError::notFound);
        CHECK(findAtlasTable({}, search20).error() == AtlasSearchError::notFound);
    }
    // Empty sprites are allowed as long as at least half are real.
    {
        auto some = sprites;
        for (std::size_t index = 0; index < 10; ++index) {
            some[index * 2] = SpriteRect{};
        }
        Bytes bytes;
        appendTable(bytes, some);
        CHECK(findAtlasTable(bytes, search20).has_value());
        some[1] = SpriteRect{}; // now 9 of 20
        Bytes fewer;
        appendTable(fewer, some);
        CHECK(findAtlasTable(fewer, search20).error() == AtlasSearchError::notFound);
        // All zero (a zero-filled region after a matching count) is rejected.
        Bytes zeros;
        appendTable(zeros, std::vector<SpriteRect>(20));
        CHECK(findAtlasTable(zeros, search20).error() == AtlasSearchError::notFound);
        // The same rectangle repeated is not a sprite table.
        Bytes repeated;
        appendTable(repeated, std::vector<SpriteRect>(20, sprites[3]));
        CHECK(findAtlasTable(repeated, search20).error() == AtlasSearchError::notFound);
    }
    // Near misses: one value out of bounds in one record rejects the table.
    {
        const auto reject = [&](std::size_t index, auto mutate) {
            auto broken = sprites;
            mutate(broken[index]);
            Bytes bytes;
            appendNoise(bytes, 50);
            appendTable(bytes, broken);
            appendNoise(bytes, 50);
            return findAtlasTable(bytes, search20).error() == AtlasSearchError::notFound;
        };
        CHECK(reject(0, [](SpriteRect &s) { s.srcX = -1; }));
        CHECK(reject(7, [](SpriteRect &s) { s.srcY = -1; }));
        CHECK(reject(7, [](SpriteRect &s) { s.offsetX = -1; }));
        CHECK(reject(7, [](SpriteRect &s) { s.width = -5; }));
        CHECK(reject(7, [](SpriteRect &s) { s.srcX = static_cast<std::int16_t>(257 - s.width); }));
        CHECK(reject(7, [](SpriteRect &s) { s.srcY = static_cast<std::int16_t>(257 - s.height); }));
        CHECK(reject(7, [](SpriteRect &s) { s.offsetX = static_cast<std::int16_t>(65 - s.width); }));
        CHECK(reject(12, [](SpriteRect &s) { s.offsetY = static_cast<std::int16_t>(65 - s.height); }));
        CHECK(reject(19, [](SpriteRect &s) { s.width = 300; })); // last record: width must fit
        // Exactly on the boundary is accepted.
        auto edge = sprites;
        edge[7].srcX = static_cast<std::int16_t>(256 - edge[7].width);
        edge[7].offsetX = static_cast<std::int16_t>(64 - edge[7].width);
        Bytes bytes;
        appendTable(bytes, edge);
        CHECK(findAtlasTable(bytes, search20).has_value());
    }
    // A wrong declared count, or a different sheet size, does not match.
    {
        Bytes bytes;
        appendTable(bytes, sprites, 21);
        CHECK(findAtlasTable(bytes, search20).error() == AtlasSearchError::notFound);
        Bytes good;
        appendTable(good, sprites);
        auto smaller = search20;
        smaller.sheetWidth = 100;
        CHECK(findAtlasTable(good, smaller).error() == AtlasSearchError::notFound);
        auto tighter = search20;
        tighter.maxExtentY = 40;
        CHECK(findAtlasTable(good, tighter).error() == AtlasSearchError::notFound);
        auto invalid = search20;
        invalid.count = 0;
        CHECK(findAtlasTable(good, invalid).error() == AtlasSearchError::notFound);
    }
    // The last sprite's height may be missing from the table: whatever
    // follows is read instead and reduced to what fits.
    {
        Bytes bytes;
        appendNoise(bytes, 10);
        appendTable(bytes, sprites, -1, true);
        bytes.push_back(std::byte{0xFF}); // next data word 0x7FFF: far too tall
        bytes.push_back(std::byte{0x7F});
        const auto table = findAtlasTable(bytes, search20);
        CHECK(table.has_value());
        if (table) {
            CHECK(table->lastHeightAdjusted);
            const auto &last = table->sprites.back();
            CHECK_EQ(last.width, sprites.back().width);
            CHECK(last.height > 0);
            CHECK(last.srcY + last.height <= 256);
            CHECK(last.offsetY + last.height <= 64);
            CHECK(std::equal(sprites.begin(), sprites.end() - 1, table->sprites.begin()));
        }
        // A negative following word leaves the last sprite empty.
        Bytes negative;
        appendTable(negative, sprites, -1, true);
        negative.push_back(std::byte{0x00});
        negative.push_back(std::byte{0x80});
        const auto empty = findAtlasTable(negative, search20);
        CHECK(empty.has_value());
        if (empty) {
            CHECK(empty->sprites.back().empty());
        }
    }
    // Two different matching tables: neither is trusted. Two identical ones
    // are the same answer.
    {
        auto other = sprites;
        other[4].srcX = static_cast<std::int16_t>(other[4].srcX + 1);
        Bytes bytes;
        appendTable(bytes, sprites);
        appendNoise(bytes, 33);
        appendTable(bytes, other);
        CHECK(findAtlasTable(bytes, search20).error() == AtlasSearchError::ambiguous);
        Bytes twice;
        appendTable(twice, sprites);
        appendNoise(twice, 33);
        appendTable(twice, sprites);
        const auto table = findAtlasTable(twice, search20);
        CHECK(table.has_value());
        CHECK_EQ(table->fileOffset, std::size_t{0});
    }
    // A larger table can contain a look-alike of a smaller one: a record
    // whose height equals the wanted count, followed by enough records that
    // also fit. Excluding the larger table's bytes leaves the real one.
    {
        auto large = sampleSprites(40);
        large[7].height = 20; // the bytes after it read as a 20-sprite table
        large[7].offsetY = 0;
        Bytes bytes;
        appendNoise(bytes, 101);
        const auto largeOffset = bytes.size();
        appendTable(bytes, large);
        appendNoise(bytes, 57);
        const auto realOffset = bytes.size();
        appendTable(bytes, sprites);
        appendNoise(bytes, 40);

        constexpr AtlasSearch search40{40, 256, 256, 64, 64};
        const auto found40 = findAtlasTable(bytes, search40);
        CHECK(found40.has_value());
        CHECK_EQ(found40->fileOffset, largeOffset);
        CHECK(found40->byteRange() == (ByteRange{largeOffset, 2 + 40 * 12}));
        CHECK(findAtlasTable(bytes, search20).error() == AtlasSearchError::ambiguous);

        const std::array occupied{found40->byteRange()};
        auto narrowed = search20;
        narrowed.excluded = occupied;
        const auto real = findAtlasTable(bytes, narrowed);
        CHECK(real.has_value());
        if (real) {
            CHECK_EQ(real->fileOffset, realOffset);
            CHECK(real->sprites == sprites);
        }
        // A candidate that only partly overlaps an excluded range is also
        // set aside; excluding everything finds nothing.
        const std::array tail{ByteRange{realOffset + 2 + 20 * 12 - 1, 1}};
        narrowed.excluded = tail;
        const auto onlyAlike = findAtlasTable(bytes, narrowed);
        CHECK(onlyAlike.has_value());
        if (onlyAlike) {
            // 2 count bytes + 7 records + 5 values of record 7 precede its height.
            CHECK_EQ(onlyAlike->fileOffset, largeOffset + 2 + 7 * 12 + 10);
        }
        const std::array everything{ByteRange{0, bytes.size()}};
        narrowed.excluded = everything;
        CHECK(findAtlasTable(bytes, narrowed).error() == AtlasSearchError::notFound);
        // Excluded ranges that touch nothing change nothing.
        const std::array elsewhere{ByteRange{0, 50}, ByteRange{bytes.size() - 10, 10}};
        narrowed.excluded = elsewhere;
        CHECK(findAtlasTable(bytes, narrowed).error() == AtlasSearchError::ambiguous);
    }
    // Realistic sizes: a 441-sprite table in a megabyte of other data.
    {
        const auto many = sampleSprites(441);
        Bytes bytes;
        appendNoise(bytes, 700'000);
        appendTable(bytes, many);
        appendNoise(bytes, 300'000);
        const auto table = findAtlasTable(bytes, {441, 256, 256, 64, 64});
        CHECK(table.has_value());
        CHECK_EQ(table->fileOffset, std::size_t{700'000});
        CHECK(table->sprites == many);
    }
    return test::finish("atlas_table");
}
