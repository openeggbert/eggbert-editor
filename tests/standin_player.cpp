// SPDX-License-Identifier: GPL-3.0-only
//
// A stand-in for a MIDI player in the tests, so that they need no shell and
// run the same on every platform:
//   standin-player [--log FILE] MIDI   writes MIDI's path to FILE, then waits
//                                      until it is stopped (at most 30 s)
//   standin-player --fail MIDI         says why on the error output, exits 3
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

int main(int argc, char **argv) {
    const char *log = nullptr;
    const char *file = "";
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--fail") == 0) {
            std::fputs("no sound set\n", stderr);
            return 3;
        }
        if (std::strcmp(argv[index], "--log") == 0 && index + 1 < argc) {
            log = argv[++index];
        } else {
            file = argv[index];
        }
    }
    if (log != nullptr) {
        std::ofstream(log) << file << "\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(30));
    return 0;
}
