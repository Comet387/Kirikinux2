#!/usr/bin/env bash
# Package only the root CMake ORIGINAL engine. No diagnostic-host target is used.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-linux}"
OUT="${KR2_PACKAGE_DIR:-$ROOT/dist}"
APPDIR="$OUT/Kirikiroid2.AppDir"
APPIMAGETOOL="${APPIMAGETOOL:-appimagetool}"
for tool in cmake python3 patchelf ldd; do
  command -v "$tool" >/dev/null || { echo "missing packaging tool: $tool" >&2; exit 1; }
done
[ "$(uname -m)" = x86_64 ] || { echo 'this package targets x86_64' >&2; exit 1; }
[ -f "$BUILD/libkrkr2core.a" ] && [ -f "$BUILD/libkrkr2plugin.a" ] || {
  echo 'build the original core and plugins with scripts/linux/build.sh first' >&2; exit 1;
}
[ ! -e "$APPDIR" ] || { echo "AppDir already exists: $APPDIR (choose a new KR2_PACKAGE_DIR)" >&2; exit 1; }
mkdir -p "$OUT"
cmake --install "$BUILD" --prefix "$APPDIR/usr"
runtime="$(python3 - "$APPDIR" <<'PY'
from pathlib import Path
import sys
items = list(Path(sys.argv[1]).glob('usr/lib*/**/kirikiroid2/kirikiroid2'))
if len(items) != 1:
    raise SystemExit('expected exactly one installed original engine')
print(items[0].parent.resolve())
PY
)"
[ -s "$runtime/Resources/DroidSansFallback.ttf" ] || { echo 'CJK font missing' >&2; exit 1; }
[ -f "$runtime/Resources/res/ui/MainFileSelector.csb" ] || { echo 'compiled Cocos UI missing' >&2; exit 1; }
python3 "$ROOT/scripts/linux/bundle-appimage-libs.py" "$runtime"
cp "$ROOT/platform/linux/kirikiroid2.desktop" "$APPDIR/kirikiroid2.desktop"
cp "$ROOT/icons/kirikiroid2-linux.png" "$APPDIR/kirikiroid2.png"
ln -s kirikiroid2.png "$APPDIR/.DirIcon"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
appdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
exec "$appdir/usr/bin/kirikiroid2" "$@"
EOF
chmod +x "$APPDIR/AppRun"
mkdir -p "$APPDIR/usr/share/doc/kirikiroid2"
cp "$ROOT/LICENSE" "$APPDIR/usr/share/doc/kirikiroid2/LICENSE"
cp "$ROOT/README.linux.md" "$APPDIR/usr/share/doc/kirikiroid2/README.linux.md"
for dir in "$ROOT/third_party/cocos2d-x" "$ROOT/third_party/unrar" "$ROOT/third_party/7zip" "$ROOT/third_party/ffmpeg-4.4.5-git"; do
  [ -d "$dir" ] || continue
  dest="$APPDIR/usr/share/doc/kirikiroid2/$(basename "$dir")"
  mkdir -p "$dest"
  find "$dir" -maxdepth 1 -type f \( -iname '*license*' -o -iname '*copying*' \) -exec cp -t "$dest" {} +
done
# Retain distribution notices for both ordinary and private-sysroot builds.
python3 - "${KR2_SYSROOT:-/}" "$APPDIR" <<'PY'
from pathlib import Path
import shutil, sys
root = Path(sys.argv[1])
for notice in Path(root, 'usr/share/doc').glob('*/copyright'):
    source = notice
    if notice.is_symlink() and not notice.exists():
        linked = Path(notice.readlink())
        if linked.is_absolute():
            source = root / str(linked).lstrip('/')
    if not source.is_file():
        continue
    dest = Path(sys.argv[2], 'usr/share/doc', notice.parent.name, 'copyright')
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)
PY
env -u LD_LIBRARY_PATH "$APPDIR/AppRun" --help > "$OUT/engine-help.txt"
grep -q -- '--size=WIDTHxHEIGHT' "$OUT/engine-help.txt"
args=(--no-appstream --mksquashfs-opt -processors --mksquashfs-opt "${KR2_PACKAGE_JOBS:-2}")
[ -z "${APPIMAGE_RUNTIME_FILE:-}" ] || args+=(--runtime-file "$APPIMAGE_RUNTIME_FILE")
ARCH=x86_64 "$APPIMAGETOOL" "${args[@]}" "$APPDIR" "$OUT/Kirikiroid2-original-x86_64.AppImage"
chmod +x "$OUT/Kirikiroid2-original-x86_64.AppImage"
(cd "$OUT" && sha256sum Kirikiroid2-original-x86_64.AppImage > SHA256SUMS)
printf 'AppImage: %s\n' "$OUT/Kirikiroid2-original-x86_64.AppImage"
