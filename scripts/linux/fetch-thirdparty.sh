#!/usr/bin/env bash
# Fetches what the public Kirikiroid2 tree does not contain (see docs/linux-port.md):
#   - cocos2d-x 3.17.2 + its prebuilt external/ libraries (what download-deps.py does)
#   - p7zip 16.02 (LZMA SDK C code for "7zip/C/7z.h"), with the older 7z API
#     files Kirikiroid2 was written for (third_party/patches/p7zip, from Kirikiroid2Yuri)
#   - unrarsrc 6.0.7, unless the system has libunrar headers
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TP="${KR2_THIRD_PARTY:-$ROOT/third_party}"
DL="$TP/downloads"
mkdir -p "$DL"

fetch() { # url output expected-sha256
  if [ ! -s "$2" ]; then
    echo "fetch: $1"
    curl -fL --retry 3 -o "$2.part" "$1"
    mv "$2.part" "$2"
  fi
  if ! echo "$3  $2" | sha256sum -c --status; then
    echo "dependency checksum mismatch: $2; remove the cached file and retry" >&2
    return 1
  fi
}

COCOS_TAG=cocos2d-x-3.17.2
if [ ! -f "$TP/cocos2d-x/cocos/cocos2d.h" ]; then
  fetch "https://github.com/cocos2d/cocos2d-x/archive/refs/tags/$COCOS_TAG.tar.gz" "$DL/$COCOS_TAG.tar.gz" "f4be12d1637ffd1b9ee5fe0f8072a7681ec709127e942cfaa8d3f13285bac6de"
  rm -rf "$TP/cocos2d-x" "$TP/cocos2d-x-$COCOS_TAG"
  tar --no-same-owner -xzf "$DL/$COCOS_TAG.tar.gz" -C "$TP"
  mv "$TP/cocos2d-x-$COCOS_TAG" "$TP/cocos2d-x"
fi
if [ ! -f "$TP/cocos2d-x/external/.kr2-deps-done" ]; then
  ver="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$TP/cocos2d-x/external/config.json")"
  [ "$ver" = v3-deps-158 ] || { echo "unexpected Cocos external version: $ver" >&2; exit 1; }
  fetch "https://github.com/cocos2d/cocos2d-x-3rd-party-libs-bin/archive/$ver.zip" "$DL/cocos2d-x-external-$ver.zip" "d591197016ff6f16fb8fff07b13bc7cb381efbe9a177ea9a0c4089c303139757"
  tmp="$(mktemp -d "$DL/.external.XXXXXX")"
  unzip -q "$DL/cocos2d-x-external-$ver.zip" -d "$tmp"
  cp -a "$tmp"/cocos2d-x-3rd-party-libs-bin-*/. "$TP/cocos2d-x/external/"
  rm -rf "$tmp"
  touch "$TP/cocos2d-x/external/.kr2-deps-done"
fi

if [ ! -f "$TP/7zip/C/7z.h" ]; then
  fetch "https://downloads.sourceforge.net/project/p7zip/p7zip/16.02/p7zip_16.02_src_all.tar.bz2" "$DL/p7zip_16.02_src_all.tar.bz2" "5eb20ac0e2944f6cb9c2d51dd6c4518941c185347d4089ea89087ffdd6e2341f"
  rm -rf "$TP/7zip" "$TP/p7zip_16.02"
  tar --no-same-owner -xjf "$DL/p7zip_16.02_src_all.tar.bz2" -C "$TP"
  mv "$TP/p7zip_16.02" "$TP/7zip"
  # Keep the patch files in a source-deliverable path. The historical
  # third_party/ location is excluded from source packages and may be absent
  # in clean CI checkouts made from those packages.
  p7zip_patch_dir="$ROOT/platform/linux/compat/p7zip"
  if [ ! -d "$p7zip_patch_dir" ]; then
    p7zip_patch_dir="$ROOT/third_party/patches/p7zip"
  fi
  compgen -G "$p7zip_patch_dir/*" >/dev/null || {
    echo "missing p7zip compatibility files: $p7zip_patch_dir" >&2
    exit 1
  }
  cp "$p7zip_patch_dir/"* "$TP/7zip/C/"
fi

if [ ! -f /usr/include/unrar/dll.hpp ] && [ ! -f "$TP/unrar/dll.hpp" ]; then
  fetch "https://www.rarlab.com/rar/unrarsrc-6.0.7.tar.gz" "$DL/unrarsrc-6.0.7.tar.gz" "a7029942006cbcced3f3b7322ec197683f8e7be408972ca08099b196c038f518"
  tar --no-same-owner -xzf "$DL/unrarsrc-6.0.7.tar.gz" -C "$TP"   # -> unrar/
fi

echo "third-party sources ready in $TP"
