// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Minimal assertion helpers for asset-free tests. A failed check is reported
// with its location and the test keeps running so one run shows every failure.

#include <iostream>
#include <source_location>
#include <string_view>

namespace eggbert::test {

inline int failureCount = 0;

inline void report(bool passed, std::string_view expression,
                   const std::source_location &where = std::source_location::current()) {
    if (!passed) {
        ++failureCount;
        std::cerr << where.file_name() << ':' << where.line() << ": check failed: "
                  << expression << '\n';
    }
}

template <typename Left, typename Right>
void reportEqual(const Left &left, const Right &right, std::string_view expression,
                 const std::source_location &where = std::source_location::current()) {
    if (!(left == right)) {
        ++failureCount;
        std::cerr << where.file_name() << ':' << where.line() << ": check failed: "
                  << expression << '\n';
    }
}

inline int finish(std::string_view name) {
    if (failureCount != 0) {
        std::cerr << name << ": " << failureCount << " failed check(s)\n";
        return 1;
    }
    return 0;
}

} // namespace eggbert::test

#define CHECK(condition) ::eggbert::test::report(static_cast<bool>(condition), #condition)
#define CHECK_EQ(left, right) ::eggbert::test::reportEqual((left), (right), #left " == " #right)
