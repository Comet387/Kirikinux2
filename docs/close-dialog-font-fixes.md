# Close button, exit crash, message box and win32dialog fixes (2026-10-07)

Status: compiled (syntax-checked against cocos2d-x 3.17.2 headers) and partly
tested in the sandbox. The full engine/AppImage was NOT built or run.

| Change | File | Verified |
|---|---|---|
| Exit with `_exit()` after flushing; no C++ static destructors (fixes SIGABRT in `tTVPGraphicType::~tTVPGraphicType`) | `platform/linux/LinuxUtils.cpp`, `LinuxMain.cpp` | compiles |
| Window "X": in a game, runs the game's own close query (same as in-game menu Exit); in the launcher asks `sure_to_exit`; if the game never picks up the request, the next X offers a forced quit | `LinuxMain.cpp` (GLFW close callback), `LinuxUtils.cpp` (`TVPLinuxRequestClose`) | compiles |
| Crash handler kills `addr2line` after 15 s so a crashed player does not linger | `LinuxCrashHandler.cpp` | compiles |
| Short message boxes: label gets a real width and CJK wrapping; trailing newlines trimmed | `LinuxDialogs.cpp` | measured under Xvfb |
| `win32dialog.dll`: full scripting surface plus a GTK renderer for modal dialog templates, item messaging, list/combo controls, buttons, sliders and preview images | `src/plugins/win32dialog.cpp`, `platform/linux/LinuxWin32Dialog.cpp` | `tests/plugins/compat_harness`, GTK GUI harness under Xvfb |
| Windows-compatible font faces: localized family/full names, tolerant lookup, installed-font enumeration, language-aware collection defaults, and exact `.ttc` face selection | `src/core/visual/FontNameTable.*`, `FontImpl.cpp`, `FontSystem.cpp`, `FreeType.cpp` | `tests/linux/font_name_table` and focused syntax checks |

Known gaps:
- Messages with more than 8 lines still open in a fixed 720x520 box.
- `force_quit_game` has no locale entries yet (English fallback text).
