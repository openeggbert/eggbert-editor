// SPDX-License-Identifier: GPL-3.0-only
#include "world/adaptive.hpp"

#include <algorithm>
#include <array>

namespace eggbert {

namespace {

using MaskRow = std::array<std::int16_t, 16>;

// Border forms by adjacency mask (bit 1 below, 2 above, 4 right, 8 left).
// Read from blupi_recon e05e22b: src/decblock.cpp:757-763 (first five rows,
// identical in both games) and games/sb2/src/decblock.cpp:919-929.
constexpr MaskRow rock{153, 147, 148, 146, 40, 151, 150, 144, 39, 152, 149, 145, 38, 36, 37, 35};
constexpr MaskRow triangularRock{75, 75, 155, 155, 75, 75, 155, 155,
                                 74, 74, 154, 154, 75, 75, 155, 155};
constexpr MaskRow grass{157, 157, 157, 157, 109, 109, 109, 109,
                        108, 108, 108, 108, 107, 107, 107, 107};
constexpr MaskRow bar{202, 202, 138, 138, 202, 202, 138, 138,
                      202, 202, 138, 138, 202, 202, 138, 138};
constexpr MaskRow bricks{283, 283, 283, 283, 262, 262, 262, 262,
                         263, 263, 263, 263, 261, 261, 261, 261};
constexpr MaskRow slime{361, 359, 360, 352, 357, 353, 355, 349,
                        358, 354, 356, 350, 351, 347, 348, 341};
constexpr MaskRow palace{387, 387, 386, 386, 394, 394, 392, 392,
                         395, 395, 393, 393, 397, 397, 396, 396};
constexpr MaskRow pipes{251, 254, 254, 254, 251, 258, 260, 258,
                        251, 257, 259, 257, 251, 250, 256, 250};
// Border tiles written into empty cells next to cheese and cave
// (games/sb2/src/decblock.cpp:931-937); mask 0 means "no border": -1.
constexpr MaskRow cheeseBorder{-1,  265, 264, 268, 267, 273, 271, 275,
                               266, 272, 270, 274, 269, 277, 276, 278};
constexpr MaskRow caveBorder{-1,  286, 285, 289, 288, 294, 292, 296,
                             287, 293, 291, 295, 290, 298, 297, 299};

constexpr const MaskRow *speedyBlupi1Rows[] = {&rock, &triangularRock, &grass, &bar};
constexpr const MaskRow *speedyBlupi2Rows[] = {&rock, &triangularRock, &grass,  &bar,
                                               &bricks, &slime,        &palace, &pipes};

constexpr AdaptiveFamily speedyBlupi1Families[] = {
    {"rock", "Rock", 153, false, "src/decor.cpp:158-178, src/decblock.cpp:757-763"},
    {"triangular-rock", "Triangular rock", 75, false,
     "src/decor.cpp:158-178, src/decblock.cpp:757-763"},
    {"grass", "Grass", 157, false, "src/decor.cpp:158-178, src/decblock.cpp:757-763"},
    {"bar", "Bar to hang from", 202, false, "src/decor.cpp:158-178, src/decblock.cpp:757-763"},
};

constexpr AdaptiveFamily speedyBlupi2Families[] = {
    {"rock", "Rock", 153, false, "games/sb2/src/decor.cpp:268-346"},
    {"triangular-rock", "Triangular rock", 75, false, "games/sb2/src/decor.cpp:268-346"},
    {"grass", "Grass", 157, false, "games/sb2/src/decor.cpp:268-346"},
    {"bar", "Bar to hang from", 202, false, "games/sb2/src/decor.cpp:268-346"},
    {"bricks", "Bricks", 283, false, "games/sb2/src/decor.cpp:268-346"},
    {"slime", "Green slime", 361, false, "games/sb2/src/decor.cpp:268-346"},
    {"palace", "Palace", 387, false, "games/sb2/src/decor.cpp:190-205, 268-346"},
    {"pipes", "Pipes", 251, false, "games/sb2/src/decor.cpp:190-205, 268-346"},
    {"cheese-246", "Cheese (246)", 246, true, "games/sb2/src/decor.cpp:250-258, 347-381"},
    {"cheese-247", "Cheese (247)", 247, true, "games/sb2/src/decor.cpp:250-258, 347-381"},
    {"cheese-248", "Cheese (248)", 248, true, "games/sb2/src/decor.cpp:250-258, 347-381"},
    {"cheese-249", "Cheese (249)", 249, true, "games/sb2/src/decor.cpp:250-258, 347-381"},
    {"cheese-339", "Cheese (339)", 339, true, "games/sb2/src/decor.cpp:250-258, 347-381"},
    {"cave-284", "Cave (284)", 284, true, "games/sb2/src/decor.cpp:259-266, 382-411"},
    {"cave-301", "Cave (301)", 301, true, "games/sb2/src/decor.cpp:259-266, 382-411"},
    {"cave-337", "Cave (337)", 337, true, "games/sb2/src/decor.cpp:259-266, 382-411"},
    {"lightning", "Lightning emitter with beam", 304, false,
     "games/sb2/src/decor.cpp:435-469"},
};

std::span<const MaskRow *const> rowsFor(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return speedyBlupi1Rows;
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return speedyBlupi2Rows;
    case GameProfile::unknown:
        break;
    }
    return {};
}

bool inRange(int value, int low, int high) {
    return value >= low && value <= high;
}

bool isAny(int value, std::initializer_list<int> candidates) {
    return std::ranges::find(candidates, value) != candidates.end();
}

bool isPalace(int value) {
    return inRange(value, 386, 397) || value == 400;
}

bool isPipe(int value) {
    return inRange(value, 250, 260);
}

// Read access to the foreground with the playfield limits the engines use.
class Field {
public:
    Field(const WorldView &world, GameProfile profile)
        : layer_(world.foreground()), profile_(profile) {
        if (const auto header = world.header()) {
            limitX_ = header->dimDecor().x != 0 ? 100 : 10;
            limitY_ = header->dimDecor().y != 0 ? 100 : 8;
        }
    }

    [[nodiscard]] bool valid() const { return layer_.has_value(); }
    [[nodiscard]] GameProfile profile() const { return profile_; }
    [[nodiscard]] static bool inMap(long long x, long long y) {
        return x >= 0 && y >= 0 && x < 100 && y < 100;
    }
    // Precondition: inMap(x, y).
    [[nodiscard]] int at(long long x, long long y) const {
        return *layer_->cell(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
    }

    // Does the neighbour at (x, y) join the cell at (x + dx, y + dy)?
    // (dx, dy) points from the neighbour back to the cell being adapted.
    // src/decor.cpp:116-156; games/sb2/src/decor.cpp:190-248.
    [[nodiscard]] bool joins(long long x, long long y, int dx, int dy) const {
        if (x < 0 || x >= limitX_ || y < 0 || y >= limitY_) {
            return true; // outside the playfield
        }
        const bool second = tablesOf(profile_) == GameProfile::speedyBlupi2;
        const int icon = at(x, y);
        if (second) {
            // Palace joins only palace and pipes only pipes, judged from
            // the cell being adapted.
            const int centre = at(x + dx, y + dy);
            if (isPalace(centre)) {
                return isPalace(icon);
            }
            if (isPipe(centre)) {
                return isPipe(icon);
            }
        }
        if (icon == -1 || isAny(icon, {32, 33, 34})) {
            return false;
        }
        if (isAny(icon, {92, 68})) {
            return dy != -1;
        }
        if (second && icon == 317) {
            return dy != 1;
        }
        if (isAny(icon, {19, 21, 25, 26, 28})) {
            return dy != 1;
        }
        if (icon == 27) {
            return dx == 0;
        }
        if (inRange(icon, 45, 47)) {
            return dy != 1;
        }
        if (isAny(icon, {15, 16, 75, 89, 90, 155})) {
            return !(dx == -1 || dy == 1);
        }
        if (isAny(icon, {17, 18, 74, 87, 88, 154})) {
            return !(dx == 1 || dy == 1);
        }
        if (isAny(icon, {76, 77, 199, 200})) {
            return dx == 0;
        }
        if (inRange(icon, 110, 125)) {
            return false;
        }
        if (icon == 126) {
            return dx == 1;
        }
        if (icon == 129) {
            return dx == -1;
        }
        if (icon == 132) {
            return dy == 1;
        }
        if (icon == 135) {
            return dy == -1;
        }
        if (inRange(icon, 139, 143)) {
            return false;
        }
        if (icon == 138) {
            return dy == -1;
        }
        if (icon == 202) {
            return false;
        }
        if (isAny(icon, {107, 108, 109, 157})) {
            return dx != 0;
        }
        if (inRange(icon, 158, 181) ||
            (second && (icon == 309 || icon == 310 || inRange(icon, 410, 420)))) {
            return dy == 1;
        }
        if (icon == 182 || icon == 183) {
            return second ? dy != 0 : false;
        }
        if (second) {
            if (inRange(icon, 334, 336)) {
                return dy != 0;
            }
            if (isPipe(icon) || inRange(icon, 264, 282) || icon == 378 || icon == 404) {
                return false;
            }
            if (inRange(icon, 421, 440)) {
                return dy != 0;
            }
        }
        return true;
    }

    [[nodiscard]] unsigned mask(long long x, long long y) const {
        unsigned result = 0;
        result |= joins(x, y + 1, 0, -1) ? joinBelow : 0;
        result |= joins(x, y - 1, 0, 1) ? joinAbove : 0;
        result |= joins(x + 1, y, -1, 0) ? joinRight : 0;
        result |= joins(x - 1, y, 1, 0) ? joinLeft : 0;
        return result;
    }

    [[nodiscard]] bool isCheese(long long x, long long y) const {
        return inMap(x, y) && (inRange(at(x, y), 246, 249) || at(x, y) == 339);
    }
    [[nodiscard]] bool isCave(long long x, long long y) const {
        return inMap(x, y) && isAny(at(x, y), {284, 301, 337});
    }
    template <typename Predicate>
    [[nodiscard]] unsigned overlayMask(long long x, long long y, Predicate member) const {
        unsigned result = 0;
        result |= (this->*member)(x, y + 1) ? joinBelow : 0;
        result |= (this->*member)(x, y - 1) ? joinAbove : 0;
        result |= (this->*member)(x + 1, y) ? joinRight : 0;
        result |= (this->*member)(x - 1, y) ? joinLeft : 0;
        return result;
    }

private:
    std::optional<TileLayerView> layer_;
    GameProfile profile_;
    long long limitX_ = 100;
    long long limitY_ = 100;
};

// Variant values the second game folds back before looking a value up
// (games/sb2/src/decor.cpp:290-313). The first game has no such step.
int foldVariant(GameProfile profile, int icon) {
    if (tablesOf(profile) != GameProfile::speedyBlupi2) {
        return icon;
    }
    if (icon == 156) {
        return 35;
    }
    if (icon == 252 || icon == 253) {
        return 251;
    }
    if (icon == 255) {
        return 254;
    }
    if (icon == 362) {
        return 347;
    }
    if (icon == 363) {
        return 348;
    }
    if (inRange(icon, 341, 346)) {
        return 341;
    }
    return icon;
}

const MaskRow *rowOf(GameProfile profile, int icon) {
    for (const auto *row : rowsFor(profile)) {
        if (std::ranges::find(*row, static_cast<std::int16_t>(icon)) != row->end()) {
            return row;
        }
    }
    return nullptr;
}

// Random looks applied after the lookup (src/decor.cpp:173;
// games/sb2/src/decor.cpp:319-342).
int withVariant(GameProfile profile, int icon, VariantSource &variants) {
    if (icon == 35) {
        return variants.coin() ? 156 : 35;
    }
    if (tablesOf(profile) != GameProfile::speedyBlupi2) {
        return icon;
    }
    if (icon == 251) {
        return 251 + variants.choice(3);
    }
    if (icon == 254) {
        return variants.coin() ? 255 : 254;
    }
    if (icon == 347) {
        return variants.coin() ? 362 : 347;
    }
    if (icon == 348) {
        return variants.coin() ? 363 : 348;
    }
    if (icon == 341) {
        return 341 + variants.choice(6);
    }
    return icon;
}

// One cell's refresh, as the engines' per-cell border routine.
void refreshCell(EditBuilder &builder, GameProfile profile, long long x, long long y,
                 VariantSource &variants) {
    if (!Field::inMap(x, y)) {
        return;
    }
    const auto view = builder.view();
    const Field field(view, profile);
    const auto set = [&](int value) {
        builder.setCell(SectionKind::foreground, static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y), static_cast<std::int16_t>(value));
    };
    const int stored = field.at(x, y);
    // The first game keeps a stored 156 while it is surrounded: its table
    // has a second rock row whose full-mask entry is 156 itself.
    const bool stickyVariant = profile == GameProfile::speedyBlupi1 && stored == 156;
    const int lookedUp = stickyVariant ? 35 : foldVariant(profile, stored);
    if (const auto *row = rowOf(profile, lookedUp)) {
        const auto form = (*row)[field.mask(x, y)];
        if (stickyVariant && form == 35) {
            return; // stays 156, no draw
        }
        set(withVariant(profile, form, variants));
        return;
    }
    if (tablesOf(profile) != GameProfile::speedyBlupi2) {
        return;
    }
    // Cheese border into an empty cell or over an existing cheese border.
    if (stored == -1 || inRange(stored, 264, 282)) {
        int form = cheeseBorder[field.overlayMask(x, y, &Field::isCheese)];
        if (isAny(form, {268, 269, 264, 265}) && variants.coin()) {
            form = form == 268 ? 279 : form == 269 ? 280 : form == 264 ? 281 : 282;
        }
        set(form);
    }
    // Then the cave border, on the value as it is now.
    const Field after(builder.view(), profile);
    const int current = after.at(x, y);
    if (current == -1 || (inRange(current, 285, 303) && current != 301)) {
        int form = caveBorder[after.overlayMask(x, y, &Field::isCave)];
        if (isAny(form, {289, 285, 286}) && variants.coin()) {
            form = form == 289 ? 300 : form == 285 ? 302 : 303;
        }
        set(form);
    }
}

// The engines' refresh after an edit of one cell: the cell, then its right,
// left, lower and upper neighbours, then (second game) the lightning beam.
void refreshAround(EditBuilder &builder, GameProfile profile, CellPos cell,
                   VariantSource &variants) {
    const auto x = static_cast<long long>(cell.x);
    const auto y = static_cast<long long>(cell.y);
    refreshCell(builder, profile, x, y, variants);
    refreshCell(builder, profile, x + 1, y, variants);
    refreshCell(builder, profile, x - 1, y, variants);
    refreshCell(builder, profile, x, y + 1, variants);
    refreshCell(builder, profile, x, y - 1, variants);
    if (tablesOf(profile) != GameProfile::speedyBlupi2) {
        return;
    }
    const auto valueAt = [&](long long row) {
        return *builder.view().foreground()->cell(cell.x, static_cast<std::size_t>(row));
    };
    const auto set = [&](long long row, std::int16_t value) {
        builder.setCell(SectionKind::foreground, cell.x, static_cast<std::size_t>(row), value);
    };
    const int centre = valueAt(y);
    if (centre == 304) {
        // An emitter fills up to four empty cells below it with the beam.
        for (long long row = y + 1; row <= y + 4 && row < 100 && valueAt(row) == -1; ++row) {
            set(row, 305);
        }
    } else if (centre == -1) {
        // An emptied cell takes up to four beam cells directly below with it.
        for (long long row = y + 1; row <= y + 4 && row < 100 && valueAt(row) == 305; ++row) {
            set(row, -1);
        }
    }
}

} // namespace

int RecordedVariants::take() {
    if (next_ >= values_.size()) {
        return 0;
    }
    return values_[next_++];
}

bool RecordedVariants::coin() {
    return take() != 0;
}

int RecordedVariants::choice(int count) {
    return std::clamp(take(), 0, count - 1);
}

std::span<const AdaptiveFamily> adaptiveFamilies(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return speedyBlupi1Families;
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return speedyBlupi2Families;
    case GameProfile::unknown:
        break;
    }
    return {};
}

const AdaptiveFamily *findAdaptiveFamily(GameProfile profile, std::string_view id) {
    for (const auto &family : adaptiveFamilies(profile)) {
        if (family.id == id) {
            return &family;
        }
    }
    return nullptr;
}

unsigned adjacencyMask(const WorldView &world, GameProfile profile, CellPos cell) {
    const Field field(world, profile);
    if (!field.valid() || profile == GameProfile::unknown || cell.x >= 100 || cell.y >= 100) {
        return 0;
    }
    return field.mask(static_cast<long long>(cell.x), static_cast<long long>(cell.y));
}

std::int16_t baseBorderForm(GameProfile profile, std::int16_t value) {
    // 156 is a variant of 35 in both games (sticky in the first, re-drawn
    // in the second).
    if (value == 156 && profile != GameProfile::unknown) {
        return 35;
    }
    return static_cast<std::int16_t>(foldVariant(profile, value));
}

std::optional<std::int16_t> expectedBorderForm(const WorldView &world, GameProfile profile,
                                               CellPos cell) {
    const Field field(world, profile);
    if (!field.valid() || profile == GameProfile::unknown || cell.x >= 100 || cell.y >= 100) {
        return std::nullopt;
    }
    const auto x = static_cast<long long>(cell.x);
    const auto y = static_cast<long long>(cell.y);
    const auto *row = rowOf(profile, baseBorderForm(profile, static_cast<std::int16_t>(field.at(x, y))));
    if (row == nullptr) {
        return std::nullopt;
    }
    return (*row)[field.mask(x, y)];
}

AdaptiveResult adaptiveStroke(EditBuilder &builder, GameProfile profile,
                              std::span<const CellPos> footprint, std::int16_t value,
                              VariantSource &variants) {
    AdaptiveResult result;
    if (profile == GameProfile::unknown || !builder.view().foreground()) {
        return result;
    }
    // Snapshot the layer to report exactly which cells ended up different.
    std::vector<std::int16_t> before(layout::mapWidth * layout::mapHeight);
    {
        const auto layer = *builder.view().foreground();
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                before[x * layout::mapHeight + y] = *layer.cell(x, y);
            }
        }
    }
    for (const auto &cell : footprint) {
        if (cell.x >= layout::mapWidth || cell.y >= layout::mapHeight) {
            continue;
        }
        builder.setCell(SectionKind::foreground, cell.x, cell.y, value);
        refreshAround(builder, profile, cell, variants);
    }
    const auto layer = *builder.view().foreground();
    const auto inFootprint = [&](CellPos cell) {
        return std::ranges::find(footprint, cell) != footprint.end();
    };
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            if (*layer.cell(x, y) == before[x * layout::mapHeight + y]) {
                continue;
            }
            (inFootprint({x, y}) ? result.changedFootprint : result.sideEffects).push_back({x, y});
        }
    }
    return result;
}

} // namespace eggbert
