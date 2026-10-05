#!/usr/bin/env bash
# Package only the root CMake ORIGINAL engine. No diagnostic-host target is used.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-linux}"
OUT="${KR2_PACKAGE_DIR:-$ROOT/dist}"
APPDIR="$OUT/kirikinux2.AppDir"
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
items = list(Path(sys.argv[1]).glob('usr/lib*/**/kirikinux/kirikinux'))
if len(items) != 1:
    raise SystemExit('expected exactly one installed original engine')
print(items[0].parent.resolve())
PY
)"
[ -s "$runtime/Resources/DroidSansFallback.ttf" ] || { echo 'CJK font missing' >&2; exit 1; }
[ -s "$runtime/Resources/default.cur" ] || { echo 'default cursor missing' >&2; exit 1; }
"$BUILD/kr2_check_resources" "$runtime/Resources/DroidSansFallback.ttf" "$runtime/Resources/default.cur"
[ -f "$runtime/Resources/res/ui/MainFileSelector.csb" ] || { echo 'compiled Cocos UI missing' >&2; exit 1; }
python3 "$ROOT/scripts/linux/bundle-appimage-libs.py" "$runtime"
cp "$ROOT/platform/linux/kirikinux.desktop" "$APPDIR/kirikinux2.desktop"
cp "$ROOT/icons/kirikinux-linux.png" "$APPDIR/kirikinux2.png"
ln -s kirikinux2.png "$APPDIR/.DirIcon"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
appdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
exec "$appdir/usr/bin/kirikinux2" "$@"
EOF
chmod +x "$APPDIR/AppRun"
mkdir -p "$APPDIR/usr/share/doc/kirikinux2"
cp "$ROOT/LICENSE" "$APPDIR/usr/share/doc/kirikinux2/LICENSE"
cp "$ROOT/LICENSE-AGPL-3.0" "$ROOT/LICENSING.md" "$ROOT/README.md" "$APPDIR/usr/share/doc/kirikinux2/"
cp "$ROOT/README.linux.md" "$APPDIR/usr/share/doc/kirikinux2/README.linux.md"
mkdir -p "$APPDIR/usr/share/doc/kirikinux2/docs"
cp "$ROOT/docs/ui-desktop-fixes.md" "$APPDIR/usr/share/doc/kirikinux2/docs/"
mkdir -p "$APPDIR/usr/share/doc/kirikinux2/tools"
cp "$ROOT"/tools/xp3-* "$ROOT/tools/README.md" "$APPDIR/usr/share/doc/kirikinux2/tools/"
for dir in "$ROOT/third_party/cocos2d-x" "$ROOT/third_party/unrar" "$ROOT/third_party/7zip" "$ROOT/third_party/ffmpeg-4.4.5-git"; do
  [ -d "$dir" ] || continue
  dest="$APPDIR/usr/share/doc/kirikinux2/$(basename "$dir")"
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
ARCH=x86_64 "$APPIMAGETOOL" "${args[@]}" "$APPDIR" "$OUT/kirikinux2-x86_64.AppImage"
# appimagetool has emitted mode 0644 in some runner images.  The smoke test
# executes the artifact directly, so make the contract explicit and fail in
# the packaging job if the mode is lost again.
chmod 0755 "$OUT/kirikinux2-x86_64.AppImage"
[ -x "$OUT/kirikinux2-x86_64.AppImage" ] || {
  echo 'packaged AppImage is not executable' >&2
  exit 1
}
(cd "$OUT" && sha256sum kirikinux2-x86_64.AppImage > SHA256SUMS)
printf 'AppImage: %s\n' "$OUT/kirikinux2-x86_64.AppImage"
