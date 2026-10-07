#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Copies the published UI files of the original release (Cocos Studio .csb files,
# fallback font, cursor, locale) out of Kirikiroid2_1.3.9.apk into the build tree.
# Use this if the build-time .csd -> .csb conversion (kr2_csd2csb) misbehaves.
#   import-apk-assets.sh Kirikiroid2_1.3.9.apk [build-linux/bin/Resources]
set -euo pipefail
apk="${1:?usage: $0 kirikinux_1.3.9.apk [Resources dir]}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dest="${2:-$ROOT/build-linux/bin/Resources}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
unzip -q -o "$apk" 'assets/*' -d "$tmp"
mkdir -p "$dest"
cp -a "$tmp/assets/." "$dest/"
echo "APK assets copied to $dest"
