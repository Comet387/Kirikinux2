# platform/linux/compat

Headers the original sources include but the public Kirikiroid2 tree does not
contain (or that live at a different path on Linux distributions). This
directory is first on the include path of the Linux build only, so no file in
`src/` had to change for them.

| header | why |
| --- | --- |
| `Protect.h` | license hooks of the private build; `TVPProtectInit()` always succeeds |
| `tvpgl_route.h`, `tvpgl_arm_route.h` | missing upstream; empty, as in Kirikiroid2Yuri |
| `aligned_allocator.h` | missing upstream; used by `visual/gl/ResampleImage.cpp` |
| `xmmlib.h` | missing upstream; copy of the Risa/krkrz header restored by Kirikiroid2Yuri |
| `jinclude.h` | libjpeg internal header `LoadJPEG.cpp` includes; distributions do not install it |
| `libarchive/*.h`, `lz4/lz4.h` | Android vendor layout -> system headers |
| `kr2_ffmpeg_compat.h` | macro spellings removed in FFmpeg 4.x, force-included into the Kodi-derived video code |
