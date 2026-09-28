> 历史诊断宿主记录。当前主移植路线已改为保留 Cocos2d、构建原版核心；见根目录 README.linux.md 和 docs/linux-port.md。这里的测试和阻塞点不代表完整引擎状态。

# Kirikiroid2 native Linux target

This directory is the first native desktop target for Kirikiroid2. It uses the
SDL2 Linux backend, so X11 is supported by SDL2's `x11` video driver (and the
same binary can also use Wayland when available). It does **not** ask users to
install Waydroid or another Android emulator.

## Build

On Debian/Ubuntu:

```sh
sudo apt-get install cmake ninja-build libsdl2-dev
cmake -S linux-native -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/kirikiroid2-linux [path/to/game-or-game.xp3]
```

The executable opens a native resizable window, accepts a dropped directory or
XP3 file, recognizes `startup.tjs` and the XP3 signature, and handles keyboard
and window events. `--help` and `--version` are available. F11 toggles
fullscreen and Esc/Ctrl-Q exits.

## Why this target is separate from the historical Android build

The public repository's only build description is an Android NDK `Android.mk`.
That file references Cocos2d-x, FFmpeg, FreeType, OpenAL, OpenCV, jxrlib,
libarchive and other `vendor/` trees that are not present in this checkout.
The Android host layer (`src/core/environ/android/AndroidUtils.cpp`) also
requires JNI, EGL and Android storage APIs. Compiling those files as Linux
would produce a binary that merely embeds an Android host, so this target keeps
the native window/input layer honest while the engine host is ported.

The parser and archive implementation in `src/core` remain the compatibility
source of truth. `KIRIKIROID2_ENABLE_LEGACY_CORE` is exposed in CMake to make
the dependency gap explicit; enabling it currently stops with a diagnostic
rather than silently linking a different engine.

## Packages

`cpack -G DEB` creates a Debian package. The repository workflow builds that
package and an AppImage on Ubuntu. Both packages contain the same native
SDL2/X11 executable and declare the SDL2 runtime dependency.
