// SPDX-License-Identifier: GPL-3.0-only
#include "world/tile_tools.hpp"

#include <algorithm>
#include <cstdlib>

namespace eggbert {

namespace {

constexpr long long mapW = static_cast<long long>(layout::mapWidth);
constexpr long long mapH = static_cast<long long>(layout::mapHeight);

bool allowed(const CellMask *limit, CellPos cell) {
    return limit == nullptr || limit->test(cell);
}

// Writes one cell and reports whether its value changed.
bool writeCell(EditBuilder &builder, const TileLayerView &before, SectionKind layer, CellPos cell,
               std::int16_t value) {
    const auto old = before.cell(cell.x, cell.y);
    if (!old || *old == value) {
        return false;
    }
    return builder.setCell(layer, cell.x, cell.y, value);
}

} // namespace

CellRect rectFromCorners(CellPos first, CellPos second) {
    return {{std::min(first.x, second.x), std::min(first.y, second.y)},
            {std::max(first.x, second.x), std::max(first.y, second.y)}};
}

std::optional<CellRect> clipToMap(long long x0, long long y0, long long x1, long long y1) {
    if (x0 > x1) {
        std::swap(x0, x1);
    }
    if (y0 > y1) {
        std::swap(y0, y1);
    }
    if (x1 < 0 || y1 < 0 || x0 >= mapW || y0 >= mapH) {
        return std::nullopt;
    }
    return CellRect{{static_cast<std::size_t>(std::max(x0, 0LL)),
                     static_cast<std::size_t>(std::max(y0, 0LL))},
                    {static_cast<std::size_t>(std::min(x1, mapW - 1)),
                     static_cast<std::size_t>(std::min(y1, mapH - 1))}};
}

CellMask CellMask::fromRect(const CellRect &rect) {
    CellMask mask;
    mask.add(rect);
    return mask;
}

void CellMask::set(CellPos cell, bool value) {
    if (cell.x < layout::mapWidth && cell.y < layout::mapHeight) {
        bits_[cell.x * layout::mapHeight + cell.y] = value;
    }
}

void CellMask::add(const CellRect &rect) {
    for (std::size_t x = rect.min.x; x <= rect.max.x && x < layout::mapWidth; ++x) {
        for (std::size_t y = rect.min.y; y <= rect.max.y && y < layout::mapHeight; ++y) {
            bits_[x * layout::mapHeight + y] = true;
        }
    }
}

std::optional<CellRect> CellMask::bounds() const {
    std::optional<CellRect> result;
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            if (!bits_[x * layout::mapHeight + y]) {
                continue;
            }
            if (!result) {
                result = CellRect{{x, y}, {x, y}};
                continue;
            }
            result->min.x = std::min(result->min.x, x);
            result->min.y = std::min(result->min.y, y);
            result->max.x = std::max(result->max.x, x);
            result->max.y = std::max(result->max.y, y);
        }
    }
    return result;
}

std::vector<CellPos> CellMask::cells() const {
    std::vector<CellPos> result;
    result.reserve(count());
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            if (bits_[x * layout::mapHeight + y]) {
                result.push_back({x, y});
            }
        }
    }
    return result;
}

std::vector<CellPos> lineCells(CellPos from, CellPos to) {
    std::vector<CellPos> cells;
    auto x = static_cast<long long>(from.x);
    auto y = static_cast<long long>(from.y);
    const auto endX = static_cast<long long>(to.x);
    const auto endY = static_cast<long long>(to.y);
    const auto dx = std::llabs(endX - x);
    const auto dy = -std::llabs(endY - y);
    const auto stepX = x < endX ? 1 : -1;
    const auto stepY = y < endY ? 1 : -1;
    auto error = dx + dy;
    while (true) {
        if (x >= 0 && y >= 0 && x < mapW && y < mapH) {
            cells.push_back({static_cast<std::size_t>(x), static_cast<std::size_t>(y)});
        }
        if (x == endX && y == endY) {
            break;
        }
        const auto doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x += stepX;
        }
        if (doubled <= dx) {
            error += dx;
            y += stepY;
        }
    }
    return cells;
}

std::vector<CellPos> outlineCells(const CellRect &rect) {
    std::vector<CellPos> cells;
    for (std::size_t x = rect.min.x; x <= rect.max.x; ++x) {
        cells.push_back({x, rect.min.y});
        if (rect.max.y != rect.min.y) {
            cells.push_back({x, rect.max.y});
        }
    }
    for (std::size_t y = rect.min.y + 1; y < rect.max.y; ++y) {
        cells.push_back({rect.min.x, y});
        if (rect.max.x != rect.min.x) {
            cells.push_back({rect.max.x, y});
        }
    }
    return cells;
}

std::size_t paintCells(EditBuilder &builder, SectionKind layer, std::span<const CellPos> cells,
                       std::int16_t value, const CellMask *limit) {
    // The view reads the builder's working copy live, so a cell listed twice
    // counts once.
    const auto before = builder.view().layer(layer);
    if (!before) {
        return 0;
    }
    std::size_t changed = 0;
    for (const auto &cell : cells) {
        if (allowed(limit, cell) && writeCell(builder, *before, layer, cell, value)) {
            ++changed;
        }
    }
    return changed;
}

std::size_t fillRect(EditBuilder &builder, SectionKind layer, const CellRect &rect,
                     std::int16_t value, const CellMask *limit) {
    const auto before = builder.view().layer(layer);
    if (!before) {
        return 0;
    }
    std::size_t changed = 0;
    for (std::size_t x = rect.min.x; x <= rect.max.x && x < layout::mapWidth; ++x) {
        for (std::size_t y = rect.min.y; y <= rect.max.y && y < layout::mapHeight; ++y) {
            if (allowed(limit, {x, y}) && writeCell(builder, *before, layer, {x, y}, value)) {
                ++changed;
            }
        }
    }
    return changed;
}

std::size_t fillMask(EditBuilder &builder, SectionKind layer, const CellMask &mask,
                     std::int16_t value) {
    const auto bounds = mask.bounds();
    return bounds ? fillRect(builder, layer, *bounds, value, &mask) : 0;
}

std::vector<CellPos> floodRegion(const WorldView &world, SectionKind layer, CellPos start,
                                 const CellMask *limit) {
    std::vector<CellPos> region;
    const auto view = world.layer(layer);
    if (!view || !TileLayerView::contains(start.x, start.y) || !allowed(limit, start)) {
        return region;
    }
    const auto target = *view->cell(start.x, start.y);
    CellMask visited;
    std::vector<CellPos> pending{start};
    visited.set(start);
    while (!pending.empty()) {
        const auto cell = pending.back();
        pending.pop_back();
        region.push_back(cell);
        const auto visit = [&](long long x, long long y) {
            if (x < 0 || y < 0 || x >= mapW || y >= mapH) {
                return;
            }
            const CellPos next{static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
            if (visited.test(next) || !allowed(limit, next) ||
                *view->cell(next.x, next.y) != target) {
                return;
            }
            visited.set(next);
            pending.push_back(next);
        };
        const auto x = static_cast<long long>(cell.x);
        const auto y = static_cast<long long>(cell.y);
        visit(x - 1, y);
        visit(x + 1, y);
        visit(x, y - 1);
        visit(x, y + 1);
    }
    std::ranges::sort(region, [](const CellPos &left, const CellPos &right) {
        return left.x != right.x ? left.x < right.x : left.y < right.y;
    });
    return region;
}

std::size_t floodFill(EditBuilder &builder, SectionKind layer, CellPos start, std::int16_t value,
                      const CellMask *limit) {
    const auto region = floodRegion(builder.view(), layer, start, limit);
    return paintCells(builder, layer, region, value);
}

std::size_t replaceValue(EditBuilder &builder, SectionKind layer, std::int16_t from,
                         std::int16_t to, const CellMask *limit) {
    const auto before = builder.view().layer(layer);
    if (!before || from == to) {
        return 0;
    }
    std::size_t changed = 0;
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            if (allowed(limit, {x, y}) && *before->cell(x, y) == from &&
                builder.setCell(layer, x, y, to)) {
                ++changed;
            }
        }
    }
    return changed;
}

std::size_t TilePatch::presentCount() const {
    return static_cast<std::size_t>(std::ranges::count(present, true));
}

TilePatch copyPatch(const WorldView &world, SectionKind layer, const CellMask &mask) {
    TilePatch patch;
    patch.sourceLayer = layer;
    const auto view = world.layer(layer);
    const auto bounds = mask.bounds();
    if (!view || !bounds) {
        return patch;
    }
    patch.width = bounds->width();
    patch.height = bounds->height();
    patch.values.assign(patch.width * patch.height, layout::emptyCell);
    patch.present.assign(patch.width * patch.height, false);
    for (std::size_t y = 0; y < patch.height; ++y) {
        for (std::size_t x = 0; x < patch.width; ++x) {
            const CellPos cell{bounds->min.x + x, bounds->min.y + y};
            if (mask.test(cell)) {
                patch.values[y * patch.width + x] = *view->cell(cell.x, cell.y);
                patch.present[y * patch.width + x] = true;
            }
        }
    }
    return patch;
}

std::vector<CellPos> pasteFootprint(const TilePatch &patch, long long x, long long y) {
    std::vector<CellPos> cells;
    for (std::size_t patchX = 0; patchX < patch.width; ++patchX) {
        for (std::size_t patchY = 0; patchY < patch.height; ++patchY) {
            const auto targetX = x + static_cast<long long>(patchX);
            const auto targetY = y + static_cast<long long>(patchY);
            if (targetX < 0 || targetY < 0 || targetX >= mapW || targetY >= mapH ||
                !patch.present[patchY * patch.width + patchX]) {
                continue;
            }
            cells.push_back(
                {static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)});
        }
    }
    return cells;
}

std::size_t pastePatch(EditBuilder &builder, SectionKind layer, const TilePatch &patch,
                       long long x, long long y, bool skipEmpty) {
    const auto before = builder.view().layer(layer);
    if (!before) {
        return 0;
    }
    std::size_t changed = 0;
    for (std::size_t patchX = 0; patchX < patch.width; ++patchX) {
        for (std::size_t patchY = 0; patchY < patch.height; ++patchY) {
            const auto index = patchY * patch.width + patchX;
            const auto targetX = x + static_cast<long long>(patchX);
            const auto targetY = y + static_cast<long long>(patchY);
            if (targetX < 0 || targetY < 0 || targetX >= mapW || targetY >= mapH ||
                !patch.present[index] ||
                (skipEmpty && patch.values[index] == layout::emptyCell)) {
                continue;
            }
            const CellPos target{static_cast<std::size_t>(targetX),
                                 static_cast<std::size_t>(targetY)};
            if (writeCell(builder, *before, layer, target, patch.values[index])) {
                ++changed;
            }
        }
    }
    return changed;
}

TilePatch flippedHorizontally(const TilePatch &patch) {
    auto result = patch;
    for (std::size_t y = 0; y < patch.height; ++y) {
        for (std::size_t x = 0; x < patch.width; ++x) {
            const auto from = y * patch.width + (patch.width - 1 - x);
            result.values[y * patch.width + x] = patch.values[from];
            result.present[y * patch.width + x] = patch.present[from];
        }
    }
    return result;
}

TilePatch flippedVertically(const TilePatch &patch) {
    auto result = patch;
    for (std::size_t y = 0; y < patch.height; ++y) {
        for (std::size_t x = 0; x < patch.width; ++x) {
            const auto from = (patch.height - 1 - y) * patch.width + x;
            result.values[y * patch.width + x] = patch.values[from];
            result.present[y * patch.width + x] = patch.present[from];
        }
    }
    return result;
}

} // namespace eggbert
