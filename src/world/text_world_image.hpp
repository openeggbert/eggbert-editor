// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "world/text_world.hpp"

namespace eggbert {

// A Mobile Eggbert text world laid out for the editor.
//
// Mobile Eggbert keeps what the Windows games keep — two 100 x 100 layers
// of tile numbers, up to 200 objects with the same thirteen numbers each,
// and a start, music, backdrop and scroll settings — but as a text file.
// So that every tool of the editor works on such a world, its values are
// placed into an image with the layout of a revision 1.3 world, edited
// there, and written back into the text value by value: a value that did
// not change keeps its characters, and nothing else in the file is touched.
//
// This is how the editor holds the world in memory. It is not a file
// conversion: the image is never written to disk, and a `.blp` world never
// becomes a text world or the other way round.
//
// Evidence for the correspondence: docs/research/evidence.md, "Mobile
// Eggbert text worlds".

struct TextWorldImage {
    // The world in the layout of a revision 1.3 file.
    std::vector<std::byte> bytes;
    // For each object slot, which `MoveObject:` line of the text it is
    // stored in (counted among those lines); none for a free slot.
    std::array<std::optional<std::size_t>, textWorldMostObjects> lineOfSlot{};
    // The image as made, to tell what the text cannot store.
    std::vector<std::byte> pristine;
};

// Fails, with the reason, when the text does not have the expected shape
// (see TextWorld::problems), holds a number the layout has no room for, or
// lists an object of type 0 (the game stops reading there): such a file is
// only offered for editing as text.
[[nodiscard]] std::expected<TextWorldImage, std::string> makeTextWorldImage(const TextWorld &world);

// Writes into `world` every value of `current` that differs from what the
// text holds, removes the lines of objects whose slot became free and adds
// lines for new objects, updating `image.lineOfSlot`. Fails, changing
// neither argument, when `current` holds something a text world has no
// place for (a title, different second to fourth starts, unknown record
// fields) or is not such an image at all.
[[nodiscard]] std::expected<void, std::string>
storeTextWorldImage(TextWorld &world, TextWorldImage &image, std::span<const std::byte> current);

// Copies the first start and its direction of an image over the other
// three, as a text world has one start. An image of another size is left
// alone.
void mirrorTextWorldStart(std::span<std::byte> image);

// The text of a new Mobile Eggbert world, written as the port's own files
// are (one header line, the two layers as 100 lines of 100 comma-separated
// values, one line per object): the world the Windows games' editors start
// a mission with (world/new_world.hpp), since the port has no editor of its
// own to copy.
[[nodiscard]] std::string newTextWorld();
// The text, written the same way, of any world laid out as an image; empty
// when the image holds something a text world has no place for.
[[nodiscard]] std::string newTextWorld(std::span<const std::byte> image);

} // namespace eggbert
