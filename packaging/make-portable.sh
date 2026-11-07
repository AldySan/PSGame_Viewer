#!/usr/bin/env bash
# Build a portable Linux archive of PSGame-Viewer: the executable, its
# runtime libraries, Qt plugins and QML modules — no installer, nothing
# installed system-wide. Unpack anywhere and run psgame-viewer.sh.
#
# Usage:
#   packaging/make-portable.sh [build-dir] [qt-dir]
# Environment:
#   BUILD_DIR  (default: build)
#   QT_DIR     (default: qmake6 -query QT_INSTALL_PREFIX)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-${BUILD_DIR:-build}}"
QT_DIR="${2:-${QT_DIR:-$(qmake6 -query QT_INSTALL_PREFIX)}}"
VERSION="$(sed -n 's/^project(PSGameViewer VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"

if [ -z "$VERSION" ]; then
    echo "cannot read the project version from CMakeLists.txt" >&2
    exit 1
fi
if [ ! -x "$BUILD_DIR/psgame-viewer" ]; then
    echo "building first..." >&2
    cmake --build "$BUILD_DIR" || exit 1
fi

NAME="PSGame-Viewer-$VERSION-linux-portable"
STAGE="$(mktemp -d)/$NAME"
mkdir -p "$STAGE/bin" "$STAGE/lib/qt6/plugins" "$STAGE/lib/qt6/qml" \
         "$STAGE/share/applications" \
         "$STAGE/share/icons/hicolor"

# 1. the executable (stripped: the payload does not ship debug symbols)
cp "$BUILD_DIR/psgame-viewer" "$STAGE/bin/"
strip --strip-unneeded "$STAGE/bin/psgame-viewer" 2>/dev/null || true

# 2. Qt plugins and QML imports (the app's own module is compiled in).
#    sqldrivers: only qsqlite — the portable library.db store; tls: HTTPS
#    for the update checks and online cover lookups.
for p in platforms iconengines networkinformation tls; do
    if [ -d "$QT_DIR/plugins/$p" ]; then cp -a "$QT_DIR/plugins/$p" "$STAGE/lib/qt6/plugins/"; fi
done
if [ -d "$QT_DIR/plugins/sqldrivers" ]; then
    mkdir -p "$STAGE/lib/qt6/plugins/sqldrivers"
    cp -a "$QT_DIR/plugins/sqldrivers/libqsqlite.so" "$STAGE/lib/qt6/plugins/sqldrivers/" 2>/dev/null || true
fi
# image handlers the app actually reads: PNG/JPEG covers, GIF/ICO/SVG
if [ -d "$QT_DIR/plugins/imageformats" ]; then
    mkdir -p "$STAGE/lib/qt6/plugins/imageformats"
    for f in libqjpeg.so libqgif.so libqico.so libqsvg.so; do
        cp -a "$QT_DIR/plugins/imageformats/$f" "$STAGE/lib/qt6/plugins/imageformats/" 2>/dev/null || true
    done
fi
cp -a "$QT_DIR/qml/." "$STAGE/lib/qt6/qml/"

# 2b. trim what the app never imports. Every QML file pins
#     QtQuick.Controls.Basic, so the other styles (and their root
#     libraries, once the style plugins are gone before the dependency
#     closure runs) are dead weight; same for the designer-only
#     folders, the asset downloader and the test/wayland QML modules.
rm -rf "$STAGE/lib/qt6/qml/QtQuick/Controls/FluentWinUI3" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/Fusion" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/Imagine" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/Material" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/Universal" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/Windows" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/macOS" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/iOS" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/designer" \
       "$STAGE/lib/qt6/qml/QtQuick/NativeStyle" \
       "$STAGE/lib/qt6/qml/QtQuick/Controls/Basic/designer" \
       "$STAGE/lib/qt6/qml/Assets" \
       "$STAGE/lib/qt6/qml/QtTest" \
       "$STAGE/lib/qt6/qml/QtWayland" \
       "$STAGE/lib/qt6/qml/QtNetwork"

# 3. every non-system shared library the payload needs. The QML style
#    plugins and the platform plugins load their own Qt libraries at
#    runtime, so resolve the dependency closure of the executable AND of
#    every staged .so, and repeat until no new library appears.
copy_deps() {
    LD_LIBRARY_PATH="$STAGE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        ldd "$1" 2>/dev/null | awk '/=> \//{print $3}' | sort -u | while read -r so; do
        case "$so" in
            "$STAGE"/*) ;;                     # already staged: keep it
            /lib/*|/usr/lib/*|/lib64/*) ;;      # base system: leave in place
            *) [ -e "$STAGE/lib/$(basename "$so")" ] || cp -L "$so" "$STAGE/lib/" ;;
        esac
    done
}
prev=-1
while :; do
    # shellcheck disable=SC2046
    for so in "$STAGE/bin/psgame-viewer" $(find "$STAGE/lib" -name '*.so*' -type f); do
        copy_deps "$so"
    done
    cur=$(find "$STAGE/lib" -type f | wc -l)
    [ "$cur" -eq "$prev" ] && break
    prev=$cur
done

# 4. optional desktop entry + icons (register manually if wanted)
cp "$ROOT/assets/psgame-viewer.desktop" "$STAGE/share/applications/"
for s in 16 32 48 64 128 256; do
    d="$STAGE/share/icons/hicolor/${s}x${s}/apps"
    mkdir -p "$d"
    cp "$ROOT/assets/icons/psgame-viewer-${s}.png" "$d/psgame-viewer.png"
done

# 5. qt.conf next to the executable: the canonical Qt portable layout.
#    No QT_PLUGIN_PATH/QML2_IMPORT_PATH env vars — those disturb the
#    compiled-in module resolution. The ELF loader only needs
#    LD_LIBRARY_PATH.
cat > "$STAGE/bin/qt.conf" <<'CONF'
[Paths]
Prefix=..
Plugins=lib/qt6/plugins
Qml2Imports=lib/qt6/qml
Libraries=lib
CONF

# 6. launcher that keeps the payload self-contained
cat > "$STAGE/psgame-viewer.sh" <<'LAUNCH'
#!/bin/sh
# Portable launcher: resolves the runtime library path relative to this
# folder. Plugin and QML paths come from bin/qt.conf.
DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
exec "$DIR/bin/psgame-viewer" "$@"
LAUNCH
chmod +x "$STAGE/psgame-viewer.sh"

# 7. README for the archive users
cat > "$STAGE/README.txt" <<NOTE
PSGame-Viewer (portable)

Run ./psgame-viewer.sh. All settings and data stay inside this folder,
next to the executable in bin/:
  bin/PSGame-Viewer.ini   settings
  bin/library.db          game library
  bin/cache/              cover art cache
  bin/batch_rename_*.log  rename revert logs

Optionally register the menu entry:
  xdg-desktop-menu install share/applications/psgame-viewer.desktop
NOTE

cd "$(dirname "$STAGE")"
tar -czf "$ROOT/$NAME.tar.gz" "$NAME"
echo "wrote $NAME.tar.gz"
