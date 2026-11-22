# Eggbert Editor

A desktop editor in development for 2D worlds from the Windows games Speedy Blupi and Speedy Blupi II, also released as Speedy Eggbert and Speedy Eggbert 2. The plan covers individual world files, installed world sets and manually authored campaigns.

The Qt Widgets scaffold currently lets you select a file with **File → Inspect World File** and compare its size with the canonical v1.3 world size. This is a metadata-only inspection. The separate C++ world library can retain an arbitrary input byte sequence. Decoding, world editing and saving are planned. The format notes, compatibility matrix and implementation milestones are in [plan.md](plan.md). A fresh implementation session should also read [AGENTS.md](AGENTS.md) and continue from [NEXT.md](NEXT.md).

## Build and run

Install a C++23-capable compiler, CMake 3.21 or newer and the Qt 6 Widgets development package, version 6.2 or newer. From the project directory, using a single-configuration generator on Linux, run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/eggbert-editor
```

If CMake cannot find Qt, set `CMAKE_PREFIX_PATH` to your Qt kit directory when configuring. For a multi-configuration generator such as Visual Studio, build with `--config Release` and pass `-C Release` to CTest. On macOS, launch the generated application bundle.

## Project layout

| Location | Contents |
| --- | --- |
| `src/world/` | Original byte storage, profile and revision types, v1.3 size constants. Independent of Qt. |
| `src/app/` | Application entry point and Qt Widgets main window. |
| `tests/` | Asset-free checks for the world library. |
| `plan.md` | Binary layout, original game behavior, feature design and release criteria. |
| `NEXT.md` | Current implementation state and the next bounded, testable slice. |
| `AGENTS.md` | Engineering invariants and fresh-context handoff rules. |

## License

Eggbert Editor is licensed under the GNU General Public License version 3 only, SPDX identifier `GPL-3.0-only`. Qt and the original games have their own licenses. No game assets are required to build or run this scaffold. [License text](COPYING)
