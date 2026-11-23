// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "world/layout.hpp"

namespace eggbert {

// A Mobile Eggbert world: a text file with one `DescFile:` line of
// `name=value ` fields, a `Decor:` line followed by one line per map column
// (comma-separated tile numbers, an empty entry meaning an empty cell), a
// `BigDecor:` block of the same shape, and one `MoveObject:` line per
// object. See docs/research/evidence.md, "Mobile Eggbert text worlds".
//
// This is a separate kind of document from the BLP worlds of the Windows
// games. It is never converted to or from BLP.
//
// The file is held as its lines exactly as read; an edit replaces one token
// of one line, so everything else, including spacing, unknown fields and
// unknown lines, is written back byte for byte.

// The game reads at most this many `MoveObject:` lines (its object pool).
inline constexpr std::size_t textWorldMostObjects = 200;

// True when the bytes begin like such a file.
[[nodiscard]] bool looksLikeTextWorld(std::string_view text);

class TextWorld {
public:
    // Always succeeds: whatever is not understood is kept and reported by
    // problems().
    [[nodiscard]] static TextWorld parse(std::string_view text);
    // The file; identical to the parsed text when nothing was edited.
    [[nodiscard]] std::string write() const;

    [[nodiscard]] std::size_t lineCount() const { return lines_.size(); }
    [[nodiscard]] const std::string &line(std::size_t index) const { return lines_[index]; }

    // The `DescFile:` line. A value is what stands between `name=` and the
    // next space, as the game reads it; nullopt when the line or the field
    // is missing, has no terminating space, or is not a number.
    [[nodiscard]] std::optional<long long> headerValue(std::string_view name) const;
    [[nodiscard]] std::optional<Point> headerPoint(std::string_view name) const;
    bool setHeaderValue(std::string_view name, long long value);
    bool setHeaderPoint(std::string_view name, Point value);

    // Tile of column x, row y of the `Decor:` (or `BigDecor:`) block; -1 for
    // an empty entry; nullopt when the block, the line or the entry is
    // missing or not a number.
    [[nodiscard]] std::optional<long long> tile(SectionKind layer, std::size_t x,
                                                std::size_t y) const;
    // Writes the number (or an empty entry for -1) into an existing entry.
    bool setTile(SectionKind layer, std::size_t x, std::size_t y, long long value);

    [[nodiscard]] std::size_t objectCount() const { return objectLines_.size(); }
    [[nodiscard]] std::optional<long long> objectValue(std::size_t index,
                                                       std::string_view name) const;
    [[nodiscard]] std::optional<Point> objectPoint(std::size_t index, std::string_view name) const;
    bool setObjectValue(std::size_t index, std::string_view name, long long value);
    bool setObjectPoint(std::size_t index, std::string_view name, Point value);
    // Whole object lines. The game takes the n-th `MoveObject:` line as its
    // n-th object, so removing a line renumbers the ones after it.
    // Removes the line of an object, line break included.
    bool removeObject(std::size_t index);
    // Inserts an exact copy of an object's line right after it. False when
    // the world already has as many objects as the game reads.
    bool duplicateObject(std::size_t index);
    // Appends a line with the thirteen fields the game reads, after the last
    // object line (or at the end of the file): the given type (not 0, which
    // ends the game's list), channel and icon, the three positions equal,
    // steps 1 and the rest 0. False when the world is full or the type is 0.
    bool addObject(long long type, Point position, long long channel, long long icon);
    // The same with every field given: the thirteen the game reads, in the
    // order the game's files have them.
    struct ObjectLine {
        long long type = 0;
        long long stepAdvance = 0;
        long long stepRecede = 0;
        long long timeStopStart = 0;
        long long timeStopEnd = 0;
        Point posStart;
        Point posEnd;
        Point posCurrent;
        long long step = 0;
        long long time = 0;
        long long phase = 0;
        long long channel = 0;
        long long icon = 0;
    };
    bool addObject(const ObjectLine &fields);

    // What does not have the expected shape, one sentence each with its
    // line number. Empty for a well-formed world.
    [[nodiscard]] std::vector<std::string> problems() const;
    // A short description for people: header values, how many cells each
    // layer uses, how many objects of each type, then the problems.
    [[nodiscard]] std::string describe() const;

private:
    [[nodiscard]] std::optional<std::size_t> sectionLine(std::string_view section) const;
    [[nodiscard]] std::optional<std::size_t> blockLine(SectionKind layer, std::size_t x) const;

    // The text split at '\n'; joined with '\n' again by write(). A final
    // empty element stands for a file that ends with a line break.
    std::vector<std::string> lines_;
    std::vector<std::size_t> objectLines_;
};

} // namespace eggbert
