# Notices

## Eggbert Editor

Copyright the Eggbert Editor contributors. Licensed under the GNU General Public License version 3 only (SPDX `GPL-3.0-only`); the full text is in `COPYING`. The corresponding source code and build instructions are in the project repository.

Eggbert Editor contains no artwork, sound, world file, executable code or data table taken from Speedy Blupi, Speedy Eggbert, Speedy Blupi II or Speedy Eggbert 2. Its icon and its schematic drawing are original. Knowledge of the world file format and of the games' level-editor behavior was obtained by reading the publicly available reconstruction listed in `docs/research/evidence.md`; the implementation and all test data were written independently.

"Speedy Blupi", "Speedy Eggbert" and related names belong to their respective owners. This project is not affiliated with or endorsed by them.

## Third-party software

| Component | Use | License |
| --- | --- | --- |
| Qt 6 (Core, Gui, Widgets) | application framework, dynamically linked | GNU LGPL version 3 (also available under GPL and commercial licenses), <https://www.qt.io/licensing> |
| Qt 6 Test | automated tests only, not part of the installed program | GNU LGPL version 3 / GPL |

Qt is used unmodified through the system's shared libraries; the packages produced by this project's build do not bundle Qt, its plugins or any fonts and depend on the distribution's Qt packages instead. If you redistribute a build that does bundle Qt, include Qt's license texts and attribution notices and keep the libraries replaceable as the LGPL requires.

No other third-party code, fonts or assets are included.
