// SPDX-License-Identifier: GPL-3.0-only
#include "world/object_animation_data.hpp"

namespace eggbert {

namespace {

// Which picture an object shows at each tick, per stored type, read from
// blupi_recon at commit e05e22b990abb782c37a0d9fafa864d86a27e29f
// (MoveObjectStepIcon in src/decmove.cpp and games/sb2/src/decmove.cpp, tables in
// the decblock.cpp files; docs/research/evidence.md, "Animation"). Only the
// types that the games' editors store in worlds are listed. A rule that adds a
// counter to a base number is written out as its frames.

constexpr std::int16_t first_0[] = {12, 13, 14, 15, 16, 17, 18, 19, 20};
constexpr std::int16_t first_1[] = {48, 49, 50, 51, 52, 53, 54, 55, 56};
constexpr std::int16_t first_2[] = {69, 70, 71, 72, 73, 74, 75, 76, 77};
constexpr std::int16_t first_3[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t first_4[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t first_5[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t first_6[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t first_7[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
constexpr std::int16_t first_8[] = {21, 22, 23, 24, 25, 26, 27, 28};
constexpr std::int16_t first_9[] = {29, 30, 31, 32, 33, 34, 35, 36};
constexpr std::int16_t first_10[] = {122, 123, 124, 125, 126, 127, 128, 127, 126, 125, 124, 123};
constexpr std::int16_t first_11[] = {129, 129, 129, 129, 130, 130, 130, 131, 131, 132, 132, 133, 133, 134, 134, 134, 135, 135, 135, 135, 134, 134, 134, 133, 133, 132, 132, 131, 131, 131, 130, 130, 130, 130};
constexpr std::int16_t first_12[] = {144, 145, 146, 147, 148, 149, 150, 151, 150, 149, 148, 147, 146, 145};
constexpr std::int16_t first_13[] = {136, 137, 138, 139, 140, 141, 142, 143};
constexpr std::int16_t first_14[] = {238, 239, 240, 241, 242, 243};
constexpr std::int16_t first_15[] = {58, 59, 59, 59, 60, 60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 64, 65, 65, 65, 66, 66, 66};
constexpr std::int16_t first_16[] = {66, 65, 65, 65, 64, 64, 64, 63, 63, 62, 62, 61, 61, 60, 60, 60, 59, 59, 59, 58, 58, 58};
constexpr std::int16_t first_17[] = {66, 66, 67, 67, 66, 66, 65, 65};
constexpr std::int16_t first_18[] = {58, 58, 57, 57, 58, 58, 59, 59};
constexpr std::int16_t first_19[] = {66, 65, 65, 65, 64, 64, 64, 63, 63, 62, 62, 61, 61, 60, 60, 60, 59, 59, 59, 58, 58, 58};
constexpr std::int16_t first_20[] = {58, 59, 59, 59, 60, 60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 64, 65, 65, 65, 66, 66, 66};
constexpr std::int16_t first_21[] = {58, 58, 57, 57, 58, 58, 59, 59};
constexpr std::int16_t first_22[] = {66, 66, 67, 67, 66, 66, 65, 65};
constexpr std::int16_t first_23[] = {79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83};
constexpr std::int16_t first_24[] = {82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79};
constexpr std::int16_t first_25[] = {82, 82, 81, 81, 82, 82, 83, 83};
constexpr std::int16_t first_26[] = {79, 79, 78, 78, 79, 79, 80, 80};
constexpr std::int16_t first_27[] = {82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79};
constexpr std::int16_t first_28[] = {79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83};
constexpr std::int16_t first_29[] = {79, 79, 78, 78, 79, 79, 80, 80};
constexpr std::int16_t first_30[] = {82, 82, 81, 81, 82, 82, 83, 83};
constexpr std::int16_t first_31[] = {106, 107, 108, 109, 110, 111, 112, 113, 105, 105};
constexpr std::int16_t first_32[] = {114, 115, 116, 117, 118, 119, 120, 121, 97, 97};
constexpr std::int16_t first_33[] = {98, 99, 100, 101, 102, 103, 104, 105};
constexpr std::int16_t first_34[] = {90, 91, 92, 93, 94, 95, 96, 97};
constexpr std::int16_t first_35[] = {114, 115, 116, 117, 118, 119, 120, 121, 97, 97};
constexpr std::int16_t first_36[] = {106, 107, 108, 109, 110, 111, 112, 113, 105, 105};
constexpr std::int16_t first_37[] = {90, 91, 92, 93, 94, 95, 96, 97};
constexpr std::int16_t first_38[] = {98, 99, 100, 101, 102, 103, 104, 105};
constexpr std::int16_t first_39[] = {71, 71, 72, 72, 73, 73, 74, 74, 75, 75, 68, 68, 275, 275, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 275, 275};
constexpr std::int16_t first_40[] = {75, 75, 74, 74, 73, 73, 72, 72, 71, 71, 63, 63, 274, 274, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 274, 274};
constexpr std::int16_t first_41[] = {66, 67, 68, 67, 66, 69, 70, 69};
constexpr std::int16_t first_42[] = {61, 62, 63, 62, 61, 64, 65, 64};
constexpr std::int16_t first_43[] = {75, 75, 74, 74, 73, 73, 72, 72, 71, 71, 63, 63, 274, 274, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 274, 274};
constexpr std::int16_t first_44[] = {71, 71, 72, 72, 73, 73, 74, 74, 75, 75, 68, 68, 275, 275, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 275, 275};
constexpr std::int16_t first_45[] = {61, 62, 63, 62, 61, 64, 65, 64};
constexpr std::int16_t first_46[] = {66, 67, 68, 67, 66, 69, 70, 69};
constexpr std::int16_t first_47[] = {238, 238, 251, 251, 238, 238, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 249, 249, 252, 252, 249, 249};
constexpr std::int16_t first_48[] = {249, 249, 252, 252, 249, 249, 249, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 238, 238, 251, 251, 238, 238};
constexpr std::int16_t first_49[] = {249, 249, 250, 250, 249, 249, 248, 248};
constexpr std::int16_t first_50[] = {238, 238, 237, 237, 238, 238, 239, 239};
constexpr std::int16_t first_51[] = {249, 249, 252, 252, 249, 249, 249, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 238, 238, 251, 251, 238, 238};
constexpr std::int16_t first_52[] = {238, 238, 251, 251, 238, 238, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 249, 249, 252, 252, 249, 249};
constexpr std::int16_t first_53[] = {238, 238, 237, 237, 238, 238, 239, 239};
constexpr std::int16_t first_54[] = {249, 249, 250, 250, 249, 249, 248, 248};
constexpr std::int16_t first_55[] = {68};
constexpr std::int16_t first_56[] = {89};
constexpr std::int16_t first_57[] = {167};
constexpr std::int16_t first_58[] = {177};
constexpr std::int16_t first_59[] = {178};

constexpr IconRule firstGameRules[] = {
    {2, 0, 0, false, 2, first_0, 10,
     "src/decmove.cpp:624"},
    {3, 0, 0, false, 2, first_1, 10,
     "src/decmove.cpp:629"},
    {16, 0, 0, false, 1, first_2, 10,
     "src/decmove.cpp:634"},
    {200, 0, 0, false, 1, first_3, 2,
     "src/decmove.cpp:639"},
    {201, 0, 0, false, 1, first_4, 11,
     "src/decmove.cpp:644"},
    {202, 0, 0, false, 1, first_5, 12,
     "src/decmove.cpp:649"},
    {203, 0, 0, false, 1, first_6, 13,
     "src/decmove.cpp:654"},
    {5, 0, 0, false, 3, first_7, 10,
     "src/decmove.cpp:661, 665"},
    {6, 0, 0, false, 4, first_8, 10,
     "src/decmove.cpp:671"},
    {7, 0, 0, false, 3, first_9, 10,
     "src/decmove.cpp:676"},
    {21, 0, 0, false, 3, first_10, 10,
     "src/decmove.cpp:681; src/decblock.cpp:712-716"},
    {24, 0, 0, false, 1, first_11, 10,
     "src/decmove.cpp:686; src/decblock.cpp:718-724"},
    {25, 0, 0, false, 2, first_12, 10,
     "src/decmove.cpp:691; src/decblock.cpp:9"},
    {26, 0, 0, false, 2, first_13, 10,
     "src/decmove.cpp:696; src/decblock.cpp:13"},
    {31, 0, 0, false, 2, first_14, 1,
     "src/decmove.cpp:701; src/decblock.cpp:15"},
    {4, 1, 1, true, 1, first_15, 10,
     "src/decmove.cpp:808; src/decblock.cpp:606-611"},
    {4, 3, 1, true, 1, first_16, 10,
     "src/decmove.cpp:812; src/decblock.cpp:613-618"},
    {4, 2, 1, true, 1, first_17, 10,
     "src/decmove.cpp:816; src/decblock.cpp:602"},
    {4, 4, 1, true, 1, first_18, 10,
     "src/decmove.cpp:820; src/decblock.cpp:604"},
    {4, 1, 2, true, 1, first_19, 10,
     "src/decmove.cpp:827; src/decblock.cpp:613-618"},
    {4, 3, 2, true, 1, first_20, 10,
     "src/decmove.cpp:831; src/decblock.cpp:606-611"},
    {4, 2, 2, true, 1, first_21, 10,
     "src/decmove.cpp:835; src/decblock.cpp:604"},
    {4, 4, 2, true, 1, first_22, 10,
     "src/decmove.cpp:839; src/decblock.cpp:602"},
    {17, 1, 1, true, 1, first_23, 10,
     "src/decmove.cpp:850; src/decblock.cpp:624-631"},
    {17, 3, 1, true, 1, first_24, 10,
     "src/decmove.cpp:854; src/decblock.cpp:633-640"},
    {17, 2, 1, true, 1, first_25, 10,
     "src/decmove.cpp:858; src/decblock.cpp:620"},
    {17, 4, 1, true, 1, first_26, 10,
     "src/decmove.cpp:862; src/decblock.cpp:622"},
    {17, 1, 2, true, 1, first_27, 10,
     "src/decmove.cpp:869; src/decblock.cpp:633-640"},
    {17, 3, 2, true, 1, first_28, 10,
     "src/decmove.cpp:873; src/decblock.cpp:624-631"},
    {17, 2, 2, true, 1, first_29, 10,
     "src/decmove.cpp:877; src/decblock.cpp:622"},
    {17, 4, 2, true, 1, first_30, 10,
     "src/decmove.cpp:881; src/decblock.cpp:620"},
    {20, 1, 1, true, 1, first_31, 10,
     "src/decmove.cpp:892; src/decblock.cpp:646"},
    {20, 3, 1, true, 1, first_32, 10,
     "src/decmove.cpp:896; src/decblock.cpp:648"},
    {20, 2, 1, true, 1, first_33, 10,
     "src/decmove.cpp:900; src/decblock.cpp:642"},
    {20, 4, 1, true, 1, first_34, 10,
     "src/decmove.cpp:904; src/decblock.cpp:644"},
    {20, 1, 2, true, 1, first_35, 10,
     "src/decmove.cpp:911; src/decblock.cpp:648"},
    {20, 3, 2, true, 1, first_36, 10,
     "src/decmove.cpp:915; src/decblock.cpp:646"},
    {20, 2, 2, true, 1, first_37, 10,
     "src/decmove.cpp:919; src/decblock.cpp:644"},
    {20, 4, 2, true, 1, first_38, 10,
     "src/decmove.cpp:923; src/decblock.cpp:642"},
    {32, 1, 1, true, 1, first_39, -1,
     "src/decmove.cpp:934; src/decblock.cpp:654-659"},
    {32, 3, 1, true, 1, first_40, -1,
     "src/decmove.cpp:938; src/decblock.cpp:661-666"},
    {32, 2, 1, true, 1, first_41, -1,
     "src/decmove.cpp:942; src/decblock.cpp:650"},
    {32, 4, 1, true, 1, first_42, -1,
     "src/decmove.cpp:946; src/decblock.cpp:652"},
    {32, 1, 2, true, 1, first_43, -1,
     "src/decmove.cpp:953; src/decblock.cpp:661-666"},
    {32, 3, 2, true, 1, first_44, -1,
     "src/decmove.cpp:957; src/decblock.cpp:654-659"},
    {32, 2, 2, true, 1, first_45, -1,
     "src/decmove.cpp:961; src/decblock.cpp:652"},
    {32, 4, 2, true, 1, first_46, -1,
     "src/decmove.cpp:965; src/decblock.cpp:650"},
    {33, 1, 1, true, 1, first_47, -1,
     "src/decmove.cpp:984; src/decblock.cpp:672-677"},
    {33, 3, 1, true, 1, first_48, -1,
     "src/decmove.cpp:988; src/decblock.cpp:679-684"},
    {33, 2, 1, true, 1, first_49, -1,
     "src/decmove.cpp:992; src/decblock.cpp:668"},
    {33, 4, 1, true, 1, first_50, -1,
     "src/decmove.cpp:996; src/decblock.cpp:670"},
    {33, 1, 2, true, 1, first_51, -1,
     "src/decmove.cpp:1003; src/decblock.cpp:679-684"},
    {33, 3, 2, true, 1, first_52, -1,
     "src/decmove.cpp:1007; src/decblock.cpp:672-677"},
    {33, 2, 2, true, 1, first_53, -1,
     "src/decmove.cpp:1011; src/decblock.cpp:670"},
    {33, 4, 2, true, 1, first_54, -1,
     "src/decmove.cpp:1015; src/decblock.cpp:668"},
    {13, 0, 0, false, 1, first_55, 10,
     "src/decmove.cpp:1097"},
    {19, 0, 0, false, 1, first_56, 10,
     "src/decmove.cpp:1102"},
    {28, 0, 0, false, 1, first_57, 10,
     "src/decmove.cpp:1107"},
    {29, 0, 0, false, 1, first_58, 10,
     "src/decmove.cpp:1117"},
    {30, 0, 0, false, 1, first_59, 10,
     "src/decmove.cpp:1122"},
};

constexpr std::int16_t second_0[] = {311, 312, 313, 314, 315, 316};
constexpr std::int16_t second_1[] = {316, 315, 314, 313, 312, 311};
constexpr std::int16_t second_2[] = {12, 13, 14, 15, 16, 17, 18, 19, 20};
constexpr std::int16_t second_3[] = {48, 49, 50, 51, 52, 53, 54, 55, 56};
constexpr std::int16_t second_4[] = {69, 70, 71, 72, 73, 74, 75, 76, 77};
constexpr std::int16_t second_5[] = {256, 256, 256, 257, 257, 258, 259, 260, 261, 262, 263, 264, 264, 265, 265, 265, 264, 264, 263, 262, 261, 260, 259, 258, 257, 257};
constexpr std::int16_t second_6[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t second_7[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t second_8[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t second_9[] = {257, 258, 259, 260, 261, 262};
constexpr std::int16_t second_10[] = {252};
constexpr std::int16_t second_11[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
constexpr std::int16_t second_12[] = {21, 22, 23, 24, 25, 26, 27, 28};
constexpr std::int16_t second_13[] = {29, 30, 31, 32, 33, 34, 35, 36};
constexpr std::int16_t second_14[] = {122, 123, 124, 125, 126, 127, 128, 127, 126, 125, 124, 123};
constexpr std::int16_t second_15[] = {209, 210, 211, 212, 213, 214, 215, 214, 213, 212, 211, 210};
constexpr std::int16_t second_16[] = {220, 221, 222, 221, 220, 219, 218, 217, 216, 217, 218, 219};
constexpr std::int16_t second_17[] = {229, 228, 227, 226, 225, 224, 223, 224, 225, 226, 227, 228};
constexpr std::int16_t second_18[] = {129, 129, 129, 129, 130, 130, 130, 131, 131, 132, 132, 133, 133, 134, 134, 134, 135, 135, 135, 135, 134, 134, 134, 133, 133, 132, 132, 131, 131, 131, 130, 130, 130, 130};
constexpr std::int16_t second_19[] = {144, 145, 146, 147, 148, 149, 150, 151, 266, 267, 268, 269, 270, 271, 272, 273};
constexpr std::int16_t second_20[] = {136, 137, 138, 139, 140, 141, 142, 143};
constexpr std::int16_t second_21[] = {187, 187, 187, 188, 189, 190, 191, 192, 193, 194, 187, 187, 187, 194, 193, 192, 191, 190, 189, 188};
constexpr std::int16_t second_22[] = {238, 239, 240, 241, 242, 243};
constexpr std::int16_t second_23[] = {58, 59, 59, 59, 60, 60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 64, 65, 65, 65, 66, 66, 66};
constexpr std::int16_t second_24[] = {66, 65, 65, 65, 64, 64, 64, 63, 63, 62, 62, 61, 61, 60, 60, 60, 59, 59, 59, 58, 58, 58};
constexpr std::int16_t second_25[] = {66, 66, 67, 67, 66, 66, 65, 65};
constexpr std::int16_t second_26[] = {58, 58, 57, 57, 58, 58, 59, 59};
constexpr std::int16_t second_27[] = {66, 65, 65, 65, 64, 64, 64, 63, 63, 62, 62, 61, 61, 60, 60, 60, 59, 59, 59, 58, 58, 58};
constexpr std::int16_t second_28[] = {58, 59, 59, 59, 60, 60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 64, 65, 65, 65, 66, 66, 66};
constexpr std::int16_t second_29[] = {58, 58, 57, 57, 58, 58, 59, 59};
constexpr std::int16_t second_30[] = {66, 66, 67, 67, 66, 66, 65, 65};
constexpr std::int16_t second_31[] = {79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83};
constexpr std::int16_t second_32[] = {82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79};
constexpr std::int16_t second_33[] = {82, 82, 81, 81, 82, 82, 83, 83};
constexpr std::int16_t second_34[] = {79, 79, 78, 78, 79, 79, 80, 80};
constexpr std::int16_t second_35[] = {82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79};
constexpr std::int16_t second_36[] = {79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83, 82, 82, 83, 83, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 79, 79, 79, 79, 80, 80, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 83, 83};
constexpr std::int16_t second_37[] = {79, 79, 78, 78, 79, 79, 80, 80};
constexpr std::int16_t second_38[] = {82, 82, 81, 81, 82, 82, 83, 83};
constexpr std::int16_t second_39[] = {106, 107, 108, 109, 110, 111, 112, 113, 105, 105};
constexpr std::int16_t second_40[] = {114, 115, 116, 117, 118, 119, 120, 121, 97, 97};
constexpr std::int16_t second_41[] = {98, 99, 100, 101, 102, 103, 104, 105};
constexpr std::int16_t second_42[] = {90, 91, 92, 93, 94, 95, 96, 97};
constexpr std::int16_t second_43[] = {114, 115, 116, 117, 118, 119, 120, 121, 97, 97};
constexpr std::int16_t second_44[] = {106, 107, 108, 109, 110, 111, 112, 113, 105, 105};
constexpr std::int16_t second_45[] = {90, 91, 92, 93, 94, 95, 96, 97};
constexpr std::int16_t second_46[] = {98, 99, 100, 101, 102, 103, 104, 105};
constexpr std::int16_t second_47[] = {207, 206, 205, 204, 203};
constexpr std::int16_t second_48[] = {203, 204, 205, 206, 207};
constexpr std::int16_t second_49[] = {195, 196, 197, 198, 197, 196};
constexpr std::int16_t second_50[] = {199, 200, 201, 202, 201, 200};
constexpr std::int16_t second_51[] = {203, 204, 205, 206, 207};
constexpr std::int16_t second_52[] = {207, 206, 205, 204, 203};
constexpr std::int16_t second_53[] = {199, 200, 201, 202, 201, 200};
constexpr std::int16_t second_54[] = {195, 196, 197, 198, 197, 196};
constexpr std::int16_t second_55[] = {244, 244, 244, 244, 244, 244, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 244, 244, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 250, 250, 249, 249, 248, 248, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244};
constexpr std::int16_t second_56[] = {244, 244, 244, 244, 244, 244, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 244, 244, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 250, 250, 249, 249, 248, 248, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244};
constexpr std::int16_t second_57[] = {247, 248, 249, 250, 251, 250, 249, 248};
constexpr std::int16_t second_58[] = {247, 248, 249, 250, 251, 250, 249, 248};
constexpr std::int16_t second_59[] = {244, 244, 244, 244, 244, 244, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 244, 244, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 250, 250, 249, 249, 248, 248, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244};
constexpr std::int16_t second_60[] = {244, 244, 244, 244, 244, 244, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 244, 244, 247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 250, 250, 249, 249, 248, 248, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 245, 245, 244, 244, 243, 243, 242, 242, 242, 242, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 246, 246, 246, 245, 245, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244};
constexpr std::int16_t second_61[] = {247, 248, 249, 250, 251, 250, 249, 248};
constexpr std::int16_t second_62[] = {247, 248, 249, 250, 251, 250, 249, 248};
constexpr std::int16_t second_63[] = {71, 71, 72, 72, 73, 73, 74, 74, 75, 75, 68, 68, 275, 275, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 275, 275};
constexpr std::int16_t second_64[] = {75, 75, 74, 74, 73, 73, 72, 72, 71, 71, 63, 63, 274, 274, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 274, 274};
constexpr std::int16_t second_65[] = {66, 67, 68, 67, 66, 69, 70, 69};
constexpr std::int16_t second_66[] = {61, 62, 63, 62, 61, 64, 65, 64};
constexpr std::int16_t second_67[] = {75, 75, 74, 74, 73, 73, 72, 72, 71, 71, 63, 63, 274, 274, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 274, 274};
constexpr std::int16_t second_68[] = {71, 71, 72, 72, 73, 73, 74, 74, 75, 75, 68, 68, 275, 275, 271, 271, 271, 271, 272, 272, 273, 273, 273, 273, 275, 275};
constexpr std::int16_t second_69[] = {61, 62, 63, 62, 61, 64, 65, 64};
constexpr std::int16_t second_70[] = {66, 67, 68, 67, 66, 69, 70, 69};
constexpr std::int16_t second_71[] = {238, 238, 251, 251, 238, 238, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 249, 249, 252, 252, 249, 249};
constexpr std::int16_t second_72[] = {249, 249, 252, 252, 249, 249, 249, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 238, 238, 251, 251, 238, 238};
constexpr std::int16_t second_73[] = {249, 249, 250, 250, 249, 249, 248, 248};
constexpr std::int16_t second_74[] = {238, 238, 237, 237, 238, 238, 239, 239};
constexpr std::int16_t second_75[] = {249, 249, 252, 252, 249, 249, 249, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238, 238, 238, 251, 251, 238, 238};
constexpr std::int16_t second_76[] = {238, 238, 251, 251, 238, 238, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 249, 249, 252, 252, 249, 249};
constexpr std::int16_t second_77[] = {238, 238, 237, 237, 238, 238, 239, 239};
constexpr std::int16_t second_78[] = {249, 249, 250, 250, 249, 249, 248, 248};
constexpr std::int16_t second_79[] = {68};
constexpr std::int16_t second_80[] = {208};
constexpr std::int16_t second_81[] = {89};
constexpr std::int16_t second_82[] = {167};
constexpr std::int16_t second_83[] = {177};
constexpr std::int16_t second_84[] = {178};

constexpr IconRule secondGameRules[] = {
    {47, 0, 0, false, 1, second_0, -1,
     "games/sb2/src/decmove.cpp:716; games/sb2/src/decblock.cpp:909-911"},
    {48, 0, 0, false, 1, second_1, -1,
     "games/sb2/src/decmove.cpp:720; games/sb2/src/decblock.cpp:913-915"},
    {2, 0, 0, false, 2, second_2, 10,
     "games/sb2/src/decmove.cpp:724"},
    {3, 0, 0, false, 2, second_3, 10,
     "games/sb2/src/decmove.cpp:729"},
    {16, 0, 0, false, 1, second_4, 10,
     "games/sb2/src/decmove.cpp:734"},
    {96, 0, 0, false, 1, second_5, 10,
     "games/sb2/src/decmove.cpp:739; games/sb2/src/decblock.cpp:833-836"},
    {200, 0, 0, false, 1, second_6, 2,
     "games/sb2/src/decmove.cpp:749"},
    {201, 0, 0, false, 1, second_7, 11,
     "games/sb2/src/decmove.cpp:754"},
    {202, 0, 0, false, 1, second_8, 12,
     "games/sb2/src/decmove.cpp:759"},
    {203, 0, 0, false, 1, second_9, 13,
     "games/sb2/src/decmove.cpp:764"},
    {55, 0, 0, false, 1, second_10, 10,
     "games/sb2/src/decmove.cpp:769"},
    {5, 0, 0, false, 3, second_11, 10,
     "games/sb2/src/decmove.cpp:821, 825"},
    {6, 0, 0, false, 4, second_12, 10,
     "games/sb2/src/decmove.cpp:831"},
    {7, 0, 0, false, 3, second_13, 10,
     "games/sb2/src/decmove.cpp:836"},
    {21, 0, 0, false, 3, second_14, 10,
     "games/sb2/src/decmove.cpp:841; games/sb2/src/decblock.cpp:842-846"},
    {49, 0, 0, false, 3, second_15, 10,
     "games/sb2/src/decmove.cpp:846; games/sb2/src/decblock.cpp:848-850"},
    {50, 0, 0, false, 3, second_16, 10,
     "games/sb2/src/decmove.cpp:851; games/sb2/src/decblock.cpp:852-854"},
    {51, 0, 0, false, 3, second_17, 10,
     "games/sb2/src/decmove.cpp:856; games/sb2/src/decblock.cpp:856-858"},
    {24, 0, 0, false, 1, second_18, 10,
     "games/sb2/src/decmove.cpp:861; games/sb2/src/decblock.cpp:870-876"},
    {25, 0, 0, false, 2, second_19, 10,
     "games/sb2/src/decmove.cpp:866; games/sb2/src/decblock.cpp:11"},
    {26, 0, 0, false, 2, second_20, 10,
     "games/sb2/src/decmove.cpp:871; games/sb2/src/decblock.cpp:15"},
    {40, 0, 0, false, 2, second_21, 10,
     "games/sb2/src/decmove.cpp:876; games/sb2/src/decblock.cpp:17-20"},
    {31, 0, 0, false, 2, second_22, 1,
     "games/sb2/src/decmove.cpp:881; games/sb2/src/decblock.cpp:22"},
    {4, 1, 1, true, 1, second_23, 10,
     "games/sb2/src/decmove.cpp:1158; games/sb2/src/decblock.cpp:636-641"},
    {4, 3, 1, true, 1, second_24, 10,
     "games/sb2/src/decmove.cpp:1162; games/sb2/src/decblock.cpp:643-648"},
    {4, 2, 1, true, 1, second_25, 10,
     "games/sb2/src/decmove.cpp:1166; games/sb2/src/decblock.cpp:632"},
    {4, 4, 1, true, 1, second_26, 10,
     "games/sb2/src/decmove.cpp:1170; games/sb2/src/decblock.cpp:634"},
    {4, 1, 2, true, 1, second_27, 10,
     "games/sb2/src/decmove.cpp:1177; games/sb2/src/decblock.cpp:643-648"},
    {4, 3, 2, true, 1, second_28, 10,
     "games/sb2/src/decmove.cpp:1181; games/sb2/src/decblock.cpp:636-641"},
    {4, 2, 2, true, 1, second_29, 10,
     "games/sb2/src/decmove.cpp:1185; games/sb2/src/decblock.cpp:634"},
    {4, 4, 2, true, 1, second_30, 10,
     "games/sb2/src/decmove.cpp:1189; games/sb2/src/decblock.cpp:632"},
    {17, 1, 1, true, 1, second_31, 10,
     "games/sb2/src/decmove.cpp:1200; games/sb2/src/decblock.cpp:654-661"},
    {17, 3, 1, true, 1, second_32, 10,
     "games/sb2/src/decmove.cpp:1204; games/sb2/src/decblock.cpp:663-670"},
    {17, 2, 1, true, 1, second_33, 10,
     "games/sb2/src/decmove.cpp:1208; games/sb2/src/decblock.cpp:650"},
    {17, 4, 1, true, 1, second_34, 10,
     "games/sb2/src/decmove.cpp:1212; games/sb2/src/decblock.cpp:652"},
    {17, 1, 2, true, 1, second_35, 10,
     "games/sb2/src/decmove.cpp:1219; games/sb2/src/decblock.cpp:663-670"},
    {17, 3, 2, true, 1, second_36, 10,
     "games/sb2/src/decmove.cpp:1223; games/sb2/src/decblock.cpp:654-661"},
    {17, 2, 2, true, 1, second_37, 10,
     "games/sb2/src/decmove.cpp:1227; games/sb2/src/decblock.cpp:652"},
    {17, 4, 2, true, 1, second_38, 10,
     "games/sb2/src/decmove.cpp:1231; games/sb2/src/decblock.cpp:650"},
    {20, 1, 1, true, 1, second_39, 10,
     "games/sb2/src/decmove.cpp:1242; games/sb2/src/decblock.cpp:676"},
    {20, 3, 1, true, 1, second_40, 10,
     "games/sb2/src/decmove.cpp:1246; games/sb2/src/decblock.cpp:678"},
    {20, 2, 1, true, 1, second_41, 10,
     "games/sb2/src/decmove.cpp:1250; games/sb2/src/decblock.cpp:672"},
    {20, 4, 1, true, 1, second_42, 10,
     "games/sb2/src/decmove.cpp:1254; games/sb2/src/decblock.cpp:674"},
    {20, 1, 2, true, 1, second_43, 10,
     "games/sb2/src/decmove.cpp:1261; games/sb2/src/decblock.cpp:678"},
    {20, 3, 2, true, 1, second_44, 10,
     "games/sb2/src/decmove.cpp:1265; games/sb2/src/decblock.cpp:676"},
    {20, 2, 2, true, 1, second_45, 10,
     "games/sb2/src/decmove.cpp:1269; games/sb2/src/decblock.cpp:674"},
    {20, 4, 2, true, 1, second_46, 10,
     "games/sb2/src/decmove.cpp:1273; games/sb2/src/decblock.cpp:672"},
    {44, 1, 1, true, 1, second_47, 10,
     "games/sb2/src/decmove.cpp:1284; games/sb2/src/decblock.cpp:688"},
    {44, 3, 1, true, 1, second_48, 10,
     "games/sb2/src/decmove.cpp:1288; games/sb2/src/decblock.cpp:690"},
    {44, 2, 1, true, 1, second_49, 10,
     "games/sb2/src/decmove.cpp:1292; games/sb2/src/decblock.cpp:680-682"},
    {44, 4, 1, true, 1, second_50, 10,
     "games/sb2/src/decmove.cpp:1296; games/sb2/src/decblock.cpp:684-686"},
    {44, 1, 2, true, 1, second_51, 10,
     "games/sb2/src/decmove.cpp:1303; games/sb2/src/decblock.cpp:690"},
    {44, 3, 2, true, 1, second_52, 10,
     "games/sb2/src/decmove.cpp:1307; games/sb2/src/decblock.cpp:688"},
    {44, 2, 2, true, 1, second_53, 10,
     "games/sb2/src/decmove.cpp:1311; games/sb2/src/decblock.cpp:684-686"},
    {44, 4, 2, true, 1, second_54, 10,
     "games/sb2/src/decmove.cpp:1315; games/sb2/src/decblock.cpp:680-682"},
    {54, 1, 1, true, 1, second_55, 10,
     "games/sb2/src/decmove.cpp:1326; games/sb2/src/decblock.cpp:696-710"},
    {54, 3, 1, true, 1, second_56, 10,
     "games/sb2/src/decmove.cpp:1330; games/sb2/src/decblock.cpp:696-710"},
    {54, 2, 1, true, 1, second_57, 10,
     "games/sb2/src/decmove.cpp:1334; games/sb2/src/decblock.cpp:692"},
    {54, 4, 1, true, 1, second_58, 10,
     "games/sb2/src/decmove.cpp:1338; games/sb2/src/decblock.cpp:694"},
    {54, 1, 2, true, 1, second_59, 10,
     "games/sb2/src/decmove.cpp:1345; games/sb2/src/decblock.cpp:696-710"},
    {54, 3, 2, true, 1, second_60, 10,
     "games/sb2/src/decmove.cpp:1349; games/sb2/src/decblock.cpp:696-710"},
    {54, 2, 2, true, 1, second_61, 10,
     "games/sb2/src/decmove.cpp:1353; games/sb2/src/decblock.cpp:694"},
    {54, 4, 2, true, 1, second_62, 10,
     "games/sb2/src/decmove.cpp:1357; games/sb2/src/decblock.cpp:692"},
    {32, 1, 1, true, 1, second_63, -1,
     "games/sb2/src/decmove.cpp:1368; games/sb2/src/decblock.cpp:716-721"},
    {32, 3, 1, true, 1, second_64, -1,
     "games/sb2/src/decmove.cpp:1372; games/sb2/src/decblock.cpp:723-728"},
    {32, 2, 1, true, 1, second_65, -1,
     "games/sb2/src/decmove.cpp:1376; games/sb2/src/decblock.cpp:712"},
    {32, 4, 1, true, 1, second_66, -1,
     "games/sb2/src/decmove.cpp:1380; games/sb2/src/decblock.cpp:714"},
    {32, 1, 2, true, 1, second_67, -1,
     "games/sb2/src/decmove.cpp:1387; games/sb2/src/decblock.cpp:723-728"},
    {32, 3, 2, true, 1, second_68, -1,
     "games/sb2/src/decmove.cpp:1391; games/sb2/src/decblock.cpp:716-721"},
    {32, 2, 2, true, 1, second_69, -1,
     "games/sb2/src/decmove.cpp:1395; games/sb2/src/decblock.cpp:714"},
    {32, 4, 2, true, 1, second_70, -1,
     "games/sb2/src/decmove.cpp:1399; games/sb2/src/decblock.cpp:712"},
    {33, 1, 1, true, 1, second_71, -1,
     "games/sb2/src/decmove.cpp:1418; games/sb2/src/decblock.cpp:734-739"},
    {33, 3, 1, true, 1, second_72, -1,
     "games/sb2/src/decmove.cpp:1422; games/sb2/src/decblock.cpp:741-746"},
    {33, 2, 1, true, 1, second_73, -1,
     "games/sb2/src/decmove.cpp:1426; games/sb2/src/decblock.cpp:730"},
    {33, 4, 1, true, 1, second_74, -1,
     "games/sb2/src/decmove.cpp:1430; games/sb2/src/decblock.cpp:732"},
    {33, 1, 2, true, 1, second_75, -1,
     "games/sb2/src/decmove.cpp:1437; games/sb2/src/decblock.cpp:741-746"},
    {33, 3, 2, true, 1, second_76, -1,
     "games/sb2/src/decmove.cpp:1441; games/sb2/src/decblock.cpp:734-739"},
    {33, 2, 2, true, 1, second_77, -1,
     "games/sb2/src/decmove.cpp:1445; games/sb2/src/decblock.cpp:732"},
    {33, 4, 2, true, 1, second_78, -1,
     "games/sb2/src/decmove.cpp:1449; games/sb2/src/decblock.cpp:730"},
    {13, 0, 0, false, 1, second_79, 10,
     "games/sb2/src/decmove.cpp:1531"},
    {46, 0, 0, false, 1, second_80, 10,
     "games/sb2/src/decmove.cpp:1536"},
    {19, 0, 0, false, 1, second_81, 10,
     "games/sb2/src/decmove.cpp:1541"},
    {28, 0, 0, false, 1, second_82, 10,
     "games/sb2/src/decmove.cpp:1546"},
    {29, 0, 0, false, 1, second_83, 10,
     "games/sb2/src/decmove.cpp:1556"},
    {30, 0, 0, false, 1, second_84, 10,
     "games/sb2/src/decmove.cpp:1561"},
};

} // namespace

std::span<const IconRule> objectIconRules(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return firstGameRules;
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return secondGameRules;
    case GameProfile::unknown:
        break;
    }
    return {};
}

} // namespace eggbert
