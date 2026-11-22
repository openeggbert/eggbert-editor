// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_file.hpp"

#include <utility>

namespace eggbert {

WorldFile::WorldFile(std::vector<std::byte> bytes)
    : bytes_(std::move(bytes)) {}

std::span<const std::byte> WorldFile::originalBytes() const {
    return bytes_;
}

} // namespace eggbert
