# Linux native port status

The `linux/` target is the native desktop host for the 1.3.9 source line. It does not use Waydroid or any Android emulator.

It currently provides:

- X11 window/input backend, with SDL2 fallback when Xlib is unavailable;
- command-line game directory and XP3 selection plus XP3 magic validation;
- drag-and-drop selection in the SDL2 backend, fullscreen toggle, Escape/close handling;
- a small `dlopen` bridge (`krkr2_linux_tick` / `krkr2_linux_shutdown`) so the platform host can be exercised independently from the engine;
- CMake + CPack Debian packaging and a GitHub Actions workflow that produces `.deb` and AppImage artifacts.

The original Android APK bundles an ARM-only `libgame.so` and vendor libraries that are not present in the public repository. The full TJS/KAG renderer and audio engine therefore still require a native rebuild of those vendor dependencies; this target keeps the parser and archive sources as the compatibility reference and does not silently substitute Kirikiri2/KirikiriZ or an emulator. See `docs/apk-1.3.9-analysis.md` and `docs/linux-port-research.md`.
