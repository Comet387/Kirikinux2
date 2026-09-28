#!/usr/bin/env bash
# Build dependencies on Debian/Ubuntu (tested package names: Ubuntu 22.04).
set -euo pipefail
privilege=()
if [ "$(id -u)" != 0 ]; then privilege=(sudo); fi
"${privilege[@]}" apt-get update
have() { apt-cache show "$1" >/dev/null 2>&1; }
pick() { for p in "$@"; do if have "$p"; then echo "$p"; return; fi; done; }
pkgs=(build-essential cmake ninja-build pkg-config python3 git curl unzip bzip2 xz-utils nasm
  # cocos2d-x 3.17.2 (its build/install-deps-linux.sh)
  libx11-dev libxmu-dev libglu1-mesa-dev libgl2ps-dev libxi-dev libzip-dev libpng-dev
  libcurl4-gnutls-dev libfontconfig1-dev libsqlite3-dev libglew-dev libssl-dev libgtk-3-dev xorg-dev
  # Kirikiroid2 core (src/core/Android.mk)
  libsdl2-dev libopenal-dev libvorbis-dev libogg-dev libopus-dev libopusfile-dev libonig-dev
  libarchive-dev liblz4-dev libopencv-dev libturbojpeg0-dev libwebp-dev zlib1g-dev libbz2-dev
  libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libavfilter-dev
  fonts-noto-cjk fonts-droid-fallback xvfb mesa-utils libxtst6)
pkgs+=("$(pick libjpeg-turbo8-dev libjpeg62-turbo-dev libjpeg-dev)")
pkgs+=("$(pick libfreetype-dev libfreetype6-dev)")
# unrar lives in multiverse/non-free; without it fetch-thirdparty.sh builds unrarsrc
if have libunrar-headers; then pkgs+=(libunrar-headers "$(pick libunrar5t64 libunrar5)"); fi
"${privilege[@]}" apt-get install -y "${pkgs[@]}"
