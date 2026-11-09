<div align="center">
  <p>
    <img src="assets/icons/psgame-viewer-256.png" width="128" alt="PSGame-Viewer logo" />
  </p>  <h1>PSGame-Viewer</h1>  <p>
    <strong>A cross-platform viewer for PlayStation game files.</strong><br />
    Explore PKG, ISO and PS5 disc images without extracting a thing.
  </p>  <p>
    <a href="https://github.com/AldySan/PSGame_Viewer/actions/workflows/ci.yml">
      <img src="https://github.com/AldySan/PSGame_Viewer/actions/workflows/ci.yml/badge.svg?branch=dev" alt="CI" />
    </a>
    <a href="https://github.com/AldySan/PSGame_Viewer/releases">
      <img src="https://img.shields.io/github/v/release/AldySan/PSGame_Viewer?display_name=tag&color=brightgreen" alt="Latest Release" />
    </a>
    <a href="LICENSE">
      <img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="License: GPL-3.0" />
    </a>
  </p>  <p>
    <a href="https://github.com/AldySan/PSGame_Viewer/stargazers">
      <img src="https://img.shields.io/github/stars/AldySan/PSGame_Viewer?style=flat&logo=github&color=yellow" alt="GitHub Stars" />
    </a>
    <a href="https://github.com/AldySan/PSGame_Viewer/network/members">
      <img src="https://img.shields.io/github/forks/AldySan/PSGame_Viewer?style=flat&logo=github" alt="GitHub Forks" />
    </a>
    <a href="https://github.com/AldySan/PSGame_Viewer/issues">
      <img src="https://img.shields.io/github/issues/AldySan/PSGame_Viewer?style=flat&logo=github&color=orange" alt="Open Issues" />
    </a>
    <a href="https://github.com/AldySan/PSGame_Viewer/commits/dev">
      <img src="https://img.shields.io/github/last-commit/AldySan/PSGame_Viewer/dev?style=flat&logo=github" alt="Last Commit" />
    </a>
  </p>  <p>
    <img src="https://img.shields.io/badge/C++-17-00599C?logo=cplusplus&logoColor=white" alt="C++17" />
    <img src="https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white" alt="Qt 6" />
    <img src="https://img.shields.io/badge/CMake-3.21%2B-064F8C?logo=cmake&logoColor=white" alt="CMake 3.21+" />
    <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey?logo=linux" alt="Cross-platform" />
  </p>  <p>
    <a href="https://ko-fi.com/aldysan">
      <img src="https://img.shields.io/badge/Support%20on-Ko--fi-FF5E5B?logo=ko-fi&logoColor=white" alt="Support on Ko-fi" />
    </a>
  </p>
</div>

Point it at a game file and it shows you what is inside: the title, Title ID, region, version, required firmware, cover art, the file listing and the trophy list.

It is read-only by design. Your game files are never modified, and nothing is installed system-wide: the app is portable, keeps its settings next to its own executable, and runs on Windows, macOS and Linux.

## Screenshots

Every shot below is the real app opened on synthetic demo packages built with the project's own fixture generators — original artwork, no copyrighted game data is bundled with this project.

| Dark theme · a PS4 package | Light theme · a PS5 package |
| --- | --- |
| ![A PS4 package open in the dark theme, Files tab](docs/screenshots/main-view.png) | ![A PS5 package open in the light theme, Details tab](docs/screenshots/light-theme.png) |

| The Files tree of a PS3 disc image | The trophy list of a PS4 package |
| --- | --- |
| ![The Files tab showing the folder tree of a PS3 disc image, light theme](docs/screenshots/files-tab.png) | ![The Trophies tab of a PS4 package with icons, dark theme](docs/screenshots/trophies.png) |

| Rename with a live preview | Settings |
| --- | --- |
| ![The rename dialog with a live preview of the new file name, light theme](docs/screenshots/rename-dialog.png) | ![The settings dialog with theme, language and online options, dark theme](docs/screenshots/settings.png) |

## Supported formats

| Platform | Format | What you get |
| --- | --- | --- |
| PS2 | `.iso` disc images | serial, version and VMODE from SYSTEM.CNF, file listing |
| PS3 | `.pkg` (NPDRM) | retail and debug listings, PARAM.SFO details |
| PS3 | game folders | NPDRM and PS3_GAME layouts |
| PS3 | `.iso` disc images | full file listing plus PS3_GAME metadata |
| PS4 | `.pkg` | games, DLC and updates; retail and FPKG detection, split parts |
| PS5 | `.pkg` (FIH/LIH) | param.json metadata, split parts, retail stubs |
| PS5 | `.exfat` images | read-only exFAT walk, sce_sys metadata, trophy packs |
| PS5 | `.ffpfsc` images | compressed PFS container (PFSC/zlib) with an inner exFAT or UFS2 image |
| PS5 | `.ffpkg` images | read-only UFS2 walk, sce_sys metadata |
| PS5 | app folders | param.json, AMPR/LZ4 asset detection (LIZARD dumps) |

## Features

**Reading files**

- Spec card with Title ID, Content ID, version, region, Min. System, SDK, DRM and an OFC/FPKG badge — split-part sets are recognized and shown as one item
- Cover art straight from the container, with a gallery when the file carries more than one image; save or copy any of them
- Files tab: expandable folder tree with per-folder sizes, double-click extract for single entries, and Extract-all
- Trophies tab: decrypted trophy lists (name, grade, hidden) with carved 240×240 icons for PS4/PS5 `.trp` and `.ucp` packs
- Details tab: curated param.sfo / param.json lines, plus a *Show all* dump for everything else

**Online lookups** (can be turned off in Settings; covers are cached on disk)

- Cover art for PS2 and PS3 titles (the ps2-covers project and GameTDB)
- Latest patch version for PS4/PS5 titles (orbispatches / prosperopatches), shown next to the spec card

**Organizing**

- Rename to a clean uniform name (`Title - TID - vVersion - Region`) or your own token pattern (`{title} {tid} {cid} {version} {region} {type} {orig}`), with live preview and one-click Undo
- Batch rename across whole folders — recursive scans, split-set aware, with a revert log for every run
- Game library (SQLite) that remembers what you opened — searchable, with recent files one click away on the start screen

**The app itself**

- Light and dark themes, applied live
- Interface languages: English, 简体中文 and Bahasa Indonesia
- Update checks against the GitHub releases — manual from the About dialog, plus an optional silent check on startup
- Portable layout: `PSGame-Viewer.ini`, `library.db`, the cover cache and rename logs all stay next to the executable, so the folder can travel on a USB stick
- Drag & drop files or folders onto the window

## Getting started

### Requirements

- CMake 3.21 or newer
- A C++17 compiler
- Qt 6.5 or newer with the Core, Gui, Quick, Quick Controls 2, Network, Sql and Xml modules (plus the Test module to run the tests)
- zlib

On recent distributions whose Qt 6 packages are new enough, the system packages work fine. For example, on Debian 13:

```sh
sudo apt install build-essential cmake ninja-build \
     qt6-base-dev qt6-declarative-dev qt6-tools-dev libgl1-mesa-dev zlib1g-dev
```

If your distribution ships an older Qt (Ubuntu 24.04 still has 6.4), install Qt 6.5+ through the [Qt Online Installer](https://www.qt.io/download-open-source) or [aqtinstall](https://github.com/miurahr/aqtinstall) instead, and point CMake at it with `CMAKE_PREFIX_PATH`.

### Build and run

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
./build/psgame-viewer
```

On Windows the same works with any generator (Visual Studio, Ninja, MinGW); on macOS the build produces an app bundle. To package a portable archive — the executable plus everything it needs to run from any folder — use `packaging/make-portable.ps1` (Windows) or `packaging/make-portable.sh` (Linux).

### Run the tests

```sh
ctest --test-dir build --output-on-failure
```

All test fixtures are synthetic files generated by `tests/fixtures/generate.py`. No copyrighted data is involved, and the fixtures double as small format examples if you want to see how the parsers behave.

## Usage

### The graphical interface

Open a file with the toolbar button, drag it onto the window, or pick it from the library. The left panel shows the cover art; the right side has the spec card and the Files, Details and Trophies tabs.

The Settings dialog covers the theme, the interface language, online lookups and the startup update check. The rename tools are toolbar buttons: **Rename** works on the open file, **Batch** scans folders.

### The command line

The same viewer core is available for scripts:

```sh
# print title, spec rows and the entry list, then exit
psgame-viewer --info game.pkg more.pkg

# batch rename a folder (dry run by default, --apply to execute)
psgame-viewer --batch-rename ~/games [--recursive] [--apply]
psgame-viewer --batch-rename ~/games --apply --name-pattern "{title} [{tid}]"

# undo a batch rename using the log it wrote
psgame-viewer --revert batch_rename_20260101_120000123.log --apply
```

`--info` writes plain text to stdout, so it pipes cleanly into other tools.

## Project layout

```
src/core/     the parsing library — one module per platform, no UI code
src/app/      QML-facing controllers, models and online lookups
src/qml/      the Qt Quick interface
tests/        unit tests and the fixture generator
packaging/    portable-archive scripts for Linux and Windows
i18n/         translations (English source, 简体中文, Bahasa Indonesia)
```

PSGame-Viewer is written in C++17 with Qt 6 and QML, built with CMake, and uses SQLite for the library and zlib for compressed containers.

## Contributing

Issues and pull requests are welcome — including small ones like a typo or an untested file variant. [CONTRIBUTING.md](CONTRIBUTING.md) walks through the setup, the code conventions and the pull request flow, and it is written with first-time contributors in mind. For the expected behaviour in discussions, see the [Code of Conduct](CODE_OF_CONDUCT.md).

## Support development

If PSGame-Viewer saved you some time, there are a few easy ways to support it:

- Give the repository a ⭐ star — it helps other people find the project
- Share it with anyone who keeps a PlayStation collection and might need it
- Chip in on [Ko-fi](https://ko-fi.com/aldysan) if you'd like to support development directly

None of this is required. The app is free, it stays free, and reports of broken files are just as valuable as donations.

## License

GPL-3.0 — see [LICENSE](LICENSE).

The PFS/PFSC container reader follows the mkpfs image format (PSBrew, GPLv3). The UFS2 reader follows the FreeBSD UFS2 on-disk layout.
