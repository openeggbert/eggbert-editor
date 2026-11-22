// SPDX-License-Identifier: GPL-3.0-only
#include "world/hex_model.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <string>

using namespace eggbert;
using namespace eggbert::test;

int main() {
    auto bytes = blankWorld(fxRevision13, 6);
    put16(bytes, 222, -3);                                   // music
    put32(bytes, 204, 640);                                  // posDecor.x
    put32(bytes, 208, -480);                                 // posDecor.y
    put32(bytes, 4 + 4 * 17, 0x01020304);                    // reserve1[17]
    putText(bytes, 376, "Name");
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 0x1234);
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 70, 3), -2);
    const auto slot = fxSlotOffset(fxRevision13, 137);
    put16(bytes, slot, 12);
    put32(bytes, slot + 20, 1000);  // posEnd.x
    put32(bytes, slot + 24, -2000); // posEnd.y
    put16(bytes, slot + 44, 311);   // icon
    const auto probe = probeWorld(bytes);

    // Labels.
    CHECK_EQ(offsetLabel(probe, 0), std::string("header.majRev"));
    CHECK_EQ(offsetLabel(probe, 3), std::string("header.minRev"));
    CHECK_EQ(offsetLabel(probe, 4 + 4 * 17 + 3), std::string("header.reserve1[17]"));
    CHECK_EQ(offsetLabel(probe, 204), std::string("header.posDecor.x"));
    CHECK_EQ(offsetLabel(probe, 211), std::string("header.posDecor.y"));
    CHECK_EQ(offsetLabel(probe, 223), std::string("header.music"));
    CHECK_EQ(offsetLabel(probe, 328 + 8 * 2 + 4), std::string("header.blupiPos[2].y"));
    CHECK_EQ(offsetLabel(probe, 360 + 4 * 3), std::string("header.blupiDir[3]"));
    CHECK_EQ(offsetLabel(probe, 376 + 42), std::string("header.name[42]"));
    CHECK_EQ(offsetLabel(probe, 867), std::string("header.reserve3[195]"));
    CHECK_EQ(offsetLabel(probe, 1609), std::string("foreground[3][70]"));
    CHECK_EQ(offsetLabel(probe, 20'868 + 14'006), std::string("bigDecor[70][3]"));
    CHECK_EQ(offsetLabel(probe, slot), std::string("objects[137].type"));
    CHECK_EQ(offsetLabel(probe, slot + 20), std::string("objects[137].posEnd.x"));
    CHECK_EQ(offsetLabel(probe, slot + 27), std::string("objects[137].posEnd.y"));
    CHECK_EQ(offsetLabel(probe, slot + 45), std::string("objects[137].icon"));
    CHECK_EQ(offsetLabel(probe, 50'468 + 5), std::string("suffix+5"));
    CHECK_EQ(offsetLabel(probe, 50'474), std::string("unmapped"));

    // Calculations use this file's literal numbers.
    CHECK_EQ(offsetCalculation(probe, 1609),
             std::string("foreground cell (3, 70), stored [x][y]: 868 + 2 x (100 x 3 + 70) = "
                         "1608"));
    CHECK_EQ(offsetCalculation(probe, 20'868 + 14'006),
             std::string("bigDecor cell (70, 3), stored [x][y]: 20868 + 2 x (100 x 70 + 3) = "
                         "34874"));
    CHECK_EQ(offsetCalculation(probe, slot + 45),
             std::string("object slot 137: 40868 + 48 x 137 = 47444; field icon at +44 = 47488"));
    CHECK_EQ(offsetCalculation(probe, 222), std::string("header field music at 222"));
    CHECK_EQ(offsetCalculation(probe, 4 + 4 * 17 + 1),
             std::string("header field reserve1 at 4, element 17: 4 + 4 x 17 = 72"));
    CHECK(offsetCalculation(probe, 50'470).empty());
    CHECK(offsetCalculation(probe, 60'000).empty());

    // Decoded scalars, signed and unsigned.
    {
        const auto music = decodeScalarAt(bytes, probe, 223).value();
        CHECK((music.range == ByteRange{222, 2}));
        CHECK_EQ(music.signedValue, std::int64_t{-3});
        CHECK_EQ(music.unsignedValue, std::uint64_t{0xFFFD});
        const auto reserve = decodeScalarAt(bytes, probe, 72).value();
        CHECK_EQ(reserve.width, std::size_t{4});
        CHECK_EQ(reserve.unsignedValue, std::uint64_t{0x01020304});
        // POINT components decode separately.
        const auto x = decodeScalarAt(bytes, probe, 205).value();
        CHECK((x.range == ByteRange{204, 4}));
        CHECK_EQ(x.signedValue, std::int64_t{640});
        const auto y = decodeScalarAt(bytes, probe, 211).value();
        CHECK((y.range == ByteRange{208, 4}));
        CHECK_EQ(y.signedValue, std::int64_t{-480});
        CHECK_EQ(y.unsignedValue, std::uint64_t{0xFFFFFE20});
        const auto name = decodeScalarAt(bytes, probe, 377).value();
        CHECK_EQ(name.width, std::size_t{1});
        CHECK_EQ(name.unsignedValue, std::uint64_t{'a'});
        const auto cell = decodeScalarAt(bytes, probe, 1608).value();
        CHECK_EQ(cell.signedValue, std::int64_t{0x1234});
        const auto big = decodeScalarAt(bytes, probe, 34'875).value();
        CHECK_EQ(big.signedValue, std::int64_t{-2});
        CHECK_EQ(big.unsignedValue, std::uint64_t{0xFFFE});
        const auto endY = decodeScalarAt(bytes, probe, slot + 26).value();
        CHECK((endY.range == ByteRange{slot + 24, 4}));
        CHECK_EQ(endY.signedValue, std::int64_t{-2000});
        const auto icon = decodeScalarAt(bytes, probe, slot + 44).value();
        CHECK_EQ(icon.signedValue, std::int64_t{311});
        CHECK(!decodeScalarAt(bytes, probe, 50'469).has_value()); // suffix
        CHECK(!decodeScalarAt(bytes, probe, 70'000).has_value());
        // A scalar cut by truncation is not decoded.
        auto cut = bytes;
        cut.resize(1609);
        const auto cutProbe = probeWorld(cut);
        CHECK_EQ(offsetLabel(cutProbe, 1608), std::string("foreground[3][70]"));
        CHECK(!decodeScalarAt(cut, cutProbe, 1608).has_value());
        CHECK(decodeScalarAt(cut, cutProbe, 1606).has_value());
    }

    // Hex parsing and formatting.
    {
        const Bytes want{std::byte{0x0A}, std::byte{0xFF}, std::byte{0x1B}};
        CHECK(parseHexPattern("0a ff 1B").value() == want);
        CHECK(parseHexPattern("0AFF1b").value() == want);
        CHECK(parseHexPattern("0x0a, 0xFF,0x1b").value() == want);
        CHECK(!parseHexPattern("").has_value());
        CHECK(!parseHexPattern("   ").has_value());
        CHECK(!parseHexPattern("0a f").has_value());
        CHECK(!parseHexPattern("zz").has_value());
        CHECK(!parseHexPattern("0a-ff").has_value());
        CHECK_EQ(formatHex(want), std::string("0a ff 1b"));
        CHECK_EQ(formatHex(want, '\0'), std::string("0aff1b"));
        CHECK_EQ(formatHex({}), std::string());
    }

    // Search, with and without wrap-around.
    {
        const Bytes needle{std::byte{0x34}, std::byte{0x12}}; // the cell value, little-endian
        CHECK_EQ(findBytes(bytes, needle, 0, false).value(), std::size_t{1608});
        CHECK_EQ(findBytes(bytes, needle, 1608, false).value(), std::size_t{1608});
        CHECK(!findBytes(bytes, needle, 1609, false).has_value());
        CHECK_EQ(findBytes(bytes, needle, 1609, true).value(), std::size_t{1608});
        const Bytes text{std::byte{'N'}, std::byte{'a'}, std::byte{'m'}, std::byte{'e'}};
        CHECK_EQ(findBytes(bytes, text, 0, false).value(), std::size_t{376});
        CHECK(!findBytes(bytes, {}, 0, true).has_value());
        const Bytes absent{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}};
        CHECK(!findBytes(bytes, absent, 0, true).has_value());
        const Bytes tiny{std::byte{1}};
        const Bytes longer{std::byte{1}, std::byte{2}};
        CHECK(!findBytes(tiny, longer, 0, true).has_value());
        // A match at the very end, and a start beyond the end with wrap.
        Bytes tail(10, std::byte{0});
        tail[8] = std::byte{7};
        tail[9] = std::byte{9};
        const Bytes last{std::byte{7}, std::byte{9}};
        CHECK_EQ(findBytes(tail, last, 0, false).value(), std::size_t{8});
        CHECK_EQ(findBytes(tail, last, 50, true).value(), std::size_t{8});
        CHECK(!findBytes(tail, last, 50, false).has_value());
        CHECK(!findBytes(tail, last, 9, false).has_value());
    }

    // Section jumps.
    {
        const auto jumps = sectionJumps(probe);
        CHECK_EQ(jumps.size(), std::size_t{5});
        CHECK(jumps[0].name == "header" && jumps[0].offset == 0 && jumps[0].size == 868);
        CHECK(jumps[1].name == "foreground" && jumps[1].offset == 868);
        CHECK(jumps[2].name == "bigDecor" && jumps[2].offset == 20'868);
        CHECK(jumps[3].name == "objects" && jumps[3].offset == 40'868 && jumps[3].size == 9'600);
        CHECK(jumps[4].name == "suffix" && jumps[4].offset == 50'468 && jumps[4].size == 6);
        const auto old = blankWorld(fxRevision10);
        CHECK_EQ(sectionJumps(probeWorld(old)).size(), std::size_t{3});
        auto cut = bytes;
        cut.resize(900);
        const auto cutJumps = sectionJumps(probeWorld(cut));
        CHECK_EQ(cutJumps.size(), std::size_t{2});
        CHECK_EQ(cutJumps[1].size, std::size_t{32}); // partial foreground
        CHECK(sectionJumps(probeWorld(Bytes(100, std::byte{9}))).empty());
    }
    return test::finish("hex_model");
}
