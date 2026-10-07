#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Build dist/kirikinux2_<version>_amd64.deb from the AppDir produced by
# package-appimage.sh.  The AppDir's usr/ tree is already self-contained
# (engine, Resources, bundled shared libraries, launcher, desktop file, icon),
# so the .deb installs the same runtime under /usr and only depends on the
# host's glibc and OpenGL driver stack, exactly like the AppImage.
#
#   ./scripts/linux/package-appimage.sh && ./scripts/linux/package-deb.sh
#
# Environment: KR2_PACKAGE_DIR (default dist/), APPDIR (default
# $KR2_PACKAGE_DIR/kirikinux2.AppDir), KR2_DEB_VERSION (default: CMake
# project version), KR2_DEB_MAINTAINER.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${KR2_PACKAGE_DIR:-$ROOT/dist}"
APPDIR="${APPDIR:-$OUT/kirikinux2.AppDir}"
for tool in dpkg-deb python3; do
  command -v "$tool" >/dev/null || { echo "missing packaging tool: $tool" >&2; exit 1; }
done
[ "$(uname -m)" = x86_64 ] || { echo 'this package targets x86_64' >&2; exit 1; }
[ -d "$APPDIR/usr" ] || { echo "AppDir not found: $APPDIR (run package-appimage.sh first)" >&2; exit 1; }

version="${KR2_DEB_VERSION:-$(sed -n 's/^project(kirikinux2 VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")}"
[ -n "$version" ] || { echo 'cannot determine package version' >&2; exit 1; }
maintainer="${KR2_DEB_MAINTAINER:-Kirikinux2 maintainers <noreply@users.noreply.github.com>}"

# Minimum glibc: the highest GLIBC_x.y symbol version referenced by the
# engine and the bundled libraries (glibc itself is taken from the host).
glibc="$(python3 - "$APPDIR/usr" <<'PY'
import re, subprocess, sys
from pathlib import Path
best = (2, 17)
for f in Path(sys.argv[1]).rglob('*'):
    if not f.is_file() or f.is_symlink():
        continue
    with open(f, 'rb') as fh:
        if fh.read(4) != b'\x7fELF':
            continue
    try:
        out = subprocess.run(['objdump', '-T', str(f)], capture_output=True, text=True).stdout
    except FileNotFoundError:
        out = open(f, 'rb').read().decode('latin-1')
    for m in re.finditer(r'GLIBC_(\d+)\.(\d+)', out):
        best = max(best, (int(m[1]), int(m[2])))
print(f'{best[0]}.{best[1]}')
PY
)"

stage="$OUT/deb-root"
rm -rf "$stage"
mkdir -p "$stage/DEBIAN"
cp -a "$APPDIR/usr" "$stage/usr"
# The AppDir layout is the install layout: /usr/bin/kirikinux2 launcher ->
# /usr/lib*/kirikinux/kirikinux with its private lib/ directory.
[ -x "$stage/usr/bin/kirikinux2" ] || { echo 'launcher usr/bin/kirikinux2 missing in AppDir' >&2; exit 1; }
find "$stage/usr" -type d -exec chmod 0755 {} +

installed_kb="$(du -sk "$stage/usr" | cut -f1)"
cat > "$stage/DEBIAN/control" <<EOF
Package: kirikinux2
Version: $version
Section: games
Priority: optional
Architecture: amd64
Maintainer: $maintainer
Installed-Size: $installed_kb
Depends: libc6 (>= $glibc), libgl1
Recommends: libgtk-3-0
Homepage: https://github.com/Comet387/kirikinux2
Description: KiriKiri2/KiriKiriZ visual novel player for Linux
 Kirikinux2 runs games made with the KiriKiri (T Visual Presenter) engine.
 It reuses the Kirikiroid2 engine, TJS2, Cocos UI, sound, video and plugin
 code.  The package carries its own runtime libraries under
 /usr/lib/kirikinux and uses the host glibc and graphics driver stack.
EOF
cat > "$stage/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
exit 0
EOF
cp "$stage/DEBIAN/postinst" "$stage/DEBIAN/postrm"
chmod 0755 "$stage/DEBIAN/postinst" "$stage/DEBIAN/postrm"

deb="$OUT/kirikinux2_${version}_amd64.deb"
rm -f "$deb"
dpkg-deb --root-owner-group -Zxz --build "$stage" "$deb"
rm -rf "$stage"
dpkg-deb --info "$deb" >/dev/null
dpkg-deb --contents "$deb" | grep -q './usr/bin/kirikinux2$' || { echo 'deb lacks /usr/bin/kirikinux2' >&2; exit 1; }
(cd "$OUT" && sha256sum "$(basename "$deb")" >> SHA256SUMS)
printf 'deb: %s\n' "$deb"
