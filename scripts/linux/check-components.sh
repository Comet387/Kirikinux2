#!/usr/bin/env bash
# Exercises fixes that need no Cocos/FFmpeg development installation.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${KR2_COMPONENT_BUILD_DIR:-$ROOT/build-port-components}"
mkdir -p "$OUT"
for script in "$ROOT"/scripts/linux/*.sh; do bash -n "$script"; done
python3 -m py_compile "$ROOT/scripts/linux/patch-cocos2dx.py"
"${CXX:-g++}" -std=c++14 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I "$ROOT/src/core/environ" -I "$ROOT/src/core/tjs2" -I "$ROOT/platform/linux/compat" \
  "$ROOT/tests/linux/port_components.cpp" -o "$OUT/port_components"
"$OUT/port_components"
echo 'allocator and tVectorList regressions passed (ASan/UBSan)'
"${CXX:-g++}" -std=c++14 -O2 \
  -I "$ROOT/platform/linux/compat" -I "$ROOT/src/core/tjs2" \
  -I "$ROOT/src/core/visual" -I "$ROOT/src/core/visual/gl" \
  "$ROOT/src/core/visual/tvpgl.cpp" "$ROOT/src/core/visual/gl/blend_function.cpp" \
  "$ROOT/tests/linux/tvpgl_components.cpp" -o "$OUT/tvpgl_components"
"$OUT/tvpgl_components"

python3 "$ROOT/tests/linux/tool_regressions.py"
