# Claude Code Cloud setup

Eggbert Editor can be developed in an Anthropic-hosted Claude Code Cloud session. The cloud VM is suitable for C++23, CMake, Qt Widgets, headless tests and offscreen rendering. Final interactive desktop behavior and original-game playtests still require an appropriate local machine.

## Repository and branch

Connect the Claude GitHub App to `openeggbert/eggbert-editor` and start from the `develop` branch. Cloud work should use a task branch and return a reviewed pull request to `develop`; do not push unfinished experiments to `main`.

The repository-level `CLAUDE.md` directs a fresh agent to `AGENTS.md`, `README.md`, `NEXT.md` and the complete plan. Do not replace that reading step with a prompt summary.

## Cloud environment

Create or edit a Claude Code cloud environment. Keep package-manager access enabled. Add this exact domain to **Allowed domains** only when a task needs optional retail-data research:

```text
drive.openeggbert.com
```

Use this environment setup script:

```bash
#!/bin/bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
    qt6-base-dev \
    qt6-base-dev-tools \
    libgl1-mesa-dev \
    p7zip-full \
    xvfb
```

GCC, Clang, CMake, Ninja, Git and ripgrep are normally preinstalled in the hosted environment. Ask the agent to run `check-tools`, `cmake --version`, `c++ --version` and `qmake6 --version` once rather than assuming versions.

## Baseline commands

Run from the repository root:

```bash
cmake -S . -B build-cloud -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON
cmake --build build-cloud --parallel
ctest --test-dir build-cloud --output-on-failure
```

For an offscreen startup smoke test, launch the application under a virtual display. A timeout exit is expected because the window waits for user input:

```bash
xvfb-run -a timeout 5s ./build-cloud/eggbert-editor
test "$?" -eq 124
```

Prefer automated Qt tests, generated atlases and deterministic screenshot fixtures over manual clicks in the cloud. Do final checks for native dialogs, drag-and-drop, fonts, high DPI and desktop integration locally.

## Optional game data

The ordinary build and CTest suite must not download or require game data. When a current verification task explicitly needs an owned/authorized edition, download only that archive from:

```text
https://drive.openeggbert.com/Blupi/Games/Speedy_Blupi_%28Windows%29/
```

Store it outside the clone, for example under `/tmp/eggbert-game-data`. Record its exact URL, retrieval date, size and SHA-256 checksum in non-committed test notes. List and validate archive members before bounded extraction into a fresh directory. Reject absolute/parent paths, links, special files, case-folded collisions and excessive expanded size. Do not execute downloaded binaries.

Never commit, push, attach to a pull request, retain in CI artifacts or package retail archives, worlds, executables, graphics, audio or exact-render screenshots. A missing network domain or unavailable archive skips the opt-in compatibility check with a clear reason; it must not block asset-free CI.

## Recommended first prompt

```text
Read CLAUDE.md, AGENTS.md, README.md, NEXT.md and all of plan.md. Inspect the current code and tests. Implement only the next bounded task from NEXT.md, using independent synthetic fixtures and preserving every unrelated input byte. Run the Release build and CTest. Update NEXT.md with exact evidence and one next task. Do not implement Post-1.0 roadmap items or commit proprietary game data.
```
