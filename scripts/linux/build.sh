#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# One-shot Linux build:  scripts/linux/build.sh [extra cmake args]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-linux}"
for tool in cmake pkg-config python3 git curl tar unzip make sha256sum; do
  command -v "$tool" >/dev/null 2>&1 || { echo "missing build tool: $tool (see docs/building.md)" >&2; exit 1; }
done
"$ROOT/scripts/linux/fetch-thirdparty.sh"

# The Kodi-derived video code needs FFmpeg 4.x
avc="$(pkg-config --modversion libavcodec 2>/dev/null || echo 0)"
FFMPEG4="${KR2_THIRD_PARTY:-$ROOT/third_party}/install/ffmpeg4"
if { [ "${avc%%.*}" -ge 59 ] || [ "$avc" = 0 ]; } && [ ! -d "$FFMPEG4" ]; then
  "$ROOT/scripts/linux/build-ffmpeg4.sh"
fi
extra=("-DKR2_THIRD_PARTY=${KR2_THIRD_PARTY:-$ROOT/third_party}")
[ -d "$FFMPEG4" ] && extra+=("-DCMAKE_PREFIX_PATH=$FFMPEG4")
gen=()
if [ ! -e "$BUILD/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then gen=(-G Ninja); fi
# Use CMake's native compiler, as in the verified GCC build. CC/CXX remain
# available for explicitly selecting another compiler.
cmake -S "$ROOT" -B "$BUILD" "${gen[@]}" "${extra[@]}" "$@"
cmake --build "$BUILD" --parallel "${KR2_BUILD_JOBS:-2}"
echo
echo "Kirikinux2 run: $BUILD/bin/kirikinux2 [game-dir | game.xp3]"
