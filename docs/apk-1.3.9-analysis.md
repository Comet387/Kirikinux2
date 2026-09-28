# Kirikiroid2 1.3.9 APK analysis

The release APK used as the behavior reference is
`Kirikiroid2_1.3.9.apk` (`versionName=1.3.9`, `versionCode=68`). Its binary
manifest identifies package `org.tvp.kirikiri2_free_10309` and app label
`吉里吉里2模拟器 1.3.9`; the JNI symbols in the native library retain the
`org.tvp.kirikiri2.KR2Activity_*` namespace. The APK was built against Android
platform 25 (7.1.1), uses GLES 2.0, and declares minSdk 11.

The package contains no game content. Assets are the Kirikiroid UI skin,
locale XML files, fallback font/cursor, Cocos UI `.csb` files, and the
`res/raw/kirikiri2.txt` license. Games are selected from external directories
or XP3 archives at runtime.

## Native payload

Only ARM ABIs are shipped; there is no x86 or x86_64 build:

| ABI | `libgame.so` | `libSDL2.so` | `libffmpeg.so` |
| --- | ---: | ---: | ---: |
| armeabi-v7a | 17,819,392 bytes | 861,812 bytes | 8,591,996 bytes |
| arm64-v8a | 27,917,400 bytes | 1,138,192 bytes | 10,412,024 bytes |

The arm64 `libgame.so` links `libffmpeg.so`, `libSDL2.so`, GLES 1/2, EGL,
Android/log, OpenSLES, `libdl`, libc, libm, and libstdc++. It exports the JNI
input/message callbacks plus the TVP and Cocos2d symbols, so it is the complete
engine and Android frontend in one shared object rather than a portable Linux
library. A Linux build must replace these Android framework/audio dependencies
with native X11/SDL2/OpenGL/audio implementations and rebuild the core.

The APK timestamp (2018-07-10) follows the repository tag `1.3.9` commit
`384e22f` (2018-07-05), and exported symbols/assets match that source tree.
The source Android manifest is stale (`1.3.4`), which explains its different
package/version fields; the release build changed them during packaging.
