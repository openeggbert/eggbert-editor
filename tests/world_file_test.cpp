// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_file.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    using namespace eggbert;
    std::vector<std::byte> bytes(50'468 + 3);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(index % 256);
    }
    const WorldFile original(bytes);
    if (!std::ranges::equal(original.originalBytes(), bytes)) {
        std::cerr << "WorldFile changed the input bytes.\n";
        return 1;
    }
    if (!WorldFile({}).originalBytes().empty()) {
        std::cerr << "An empty input must stay empty.\n";
        return 1;
    }
    return 0;
}
