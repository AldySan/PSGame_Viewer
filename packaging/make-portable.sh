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
VERSION="$(rg -o 'project\(PSGameViewer VERSION ([0-9.]+)' -r '$1' "$ROOT/CMakeLists.txt" | head -1)"

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

# 1. the executable
cp "$BUILD_DIR/psgame-viewer" "$STAGE/bin/"

# 2. every non-system shared library it needs
ldd "$STAGE/bin/psgame-viewer" | awk '/=> \//{print $3}' | sort -u | while read -r so; do
    case "$so" in
        /lib/*|/usr/lib/*|/lib64/*) ;;      # base system: leave in place
        *) cp -L "$so" "$STAGE/lib/" ;;
    esac
done

# 3. Qt plugins and QML imports (the app's own module is compiled in).
#    sqldrivers: the portable library.db; tls: HTTPS for the update checks
#    and online cover lookups.
for p in platforms imageformats iconengines networkinformation sqldrivers tls; do
    [ -d "$QT_DIR/plugins/$p" ] && cp -a "$QT_DIR/plugins/$p" "$STAGE/lib/qt6/plugins/"
done
cp -a "$QT_DIR/qml/." "$STAGE/lib/qt6/qml/"

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

Run ./psgame-viewer.sh. All settings and data stay inside this folder:
  PSGame-Viewer.ini   settings
  library.db          game library
  cache/              cover art cache
  batch_rename_*.log  rename revert logs

Optionally register the menu entry:
  xdg-desktop-menu install share/applications/psgame-viewer.desktop
NOTE

cd "$(dirname "$STAGE")"
tar -czf "$ROOT/$NAME.tar.gz" "$NAME"
echo "wrote $NAME.tar.gz"
