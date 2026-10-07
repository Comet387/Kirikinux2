#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# The Kodi-derived video code (src/core/movie/ffmpeg) needs the FFmpeg 4.x API.
# Distributions newer than Ubuntu 22.04 / Debian 11 ship FFmpeg >= 5; this builds
# a private shared FFmpeg 4.4 into third_party/install/ffmpeg4 (build.sh uses it).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TP="${KR2_THIRD_PARTY:-$ROOT/third_party}"
VER=4.4.5
PREFIX="$TP/install/ffmpeg4"
COMMIT=9bcede27c26b2f7cd469ab6b5c8b9694c30cfca3 # official n4.4.5
SRC="$TP/ffmpeg-$VER-git"
mkdir -p "$SRC" "$TP/build-tmp"
if [ ! -d "$SRC/.git" ]; then git init -q "$SRC"; fi
if [ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null || true)" != "$COMMIT" ]; then
  git -C "$SRC" fetch --depth 1 https://github.com/FFmpeg/FFmpeg.git "$COMMIT"
  git -C "$SRC" checkout --detach "$COMMIT"
fi
[ "$(git -C "$SRC" rev-parse HEAD)" = "$COMMIT" ] || exit 1
export TMPDIR="${TMPDIR:-$TP/build-tmp}"
cd "$SRC"
asm=()
if ! command -v nasm >/dev/null 2>&1 && ! command -v yasm >/dev/null 2>&1; then asm=(--disable-x86asm); fi
./configure --prefix="$PREFIX" --enable-shared --disable-static --enable-pic \
  --disable-programs --disable-doc --disable-avdevice --disable-postproc \
  --disable-network --disable-debug --disable-autodetect --disable-encoders --disable-muxers \
  --enable-swresample --enable-swscale "${asm[@]}"
make -j"${KR2_BUILD_JOBS:-2}"
make install
echo "FFmpeg $VER installed to $PREFIX"
