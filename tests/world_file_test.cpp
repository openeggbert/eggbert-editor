// SPDX-License-Identifier: GPL-3.0-only
#include "world/layout.hpp"
#include "world/world_file.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    using namespace eggbert;
    static_assert(v1_3::tileLayerBytes == 20'000);
    static_assert(v1_3::objectCount * v1_3::objectBytes == 9'600);

    std::vector<std::byte> bytes(v1_3::worldBytes + 3);
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
