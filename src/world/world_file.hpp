// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace eggbert {

// Owns the exact input, including any bytes the editor cannot interpret.
class WorldFile {
public:
    explicit WorldFile(std::vector<std::byte> bytes);
    [[nodiscard]] std::span<const std::byte> originalBytes() const;

private:
    std::vector<std::byte> bytes_;
};

} // namespace eggbert
