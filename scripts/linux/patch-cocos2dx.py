#!/usr/bin/env python3
"""Re-create, on a stock cocos2d-x 3.17.2 tree, the API differences of the
privately modified cocos2d-x zeas2 built kirikinux with (that vendor tree was
never published).  Every change only widens an API, nothing stock code relies on
changes.  Idempotent; run by CMakeLists.txt at configure time.

    patch-cocos2dx.py <cocos2d-x root>

Known differences (found from compile errors of the original sources, see
docs/linux-port.md; Kirikiroid2Yuri edited the krkr sources instead):
  1. extension::ScrollView::min/maxContainerOffset() are virtual
     (overridden by TVPWindowLayer in src/core/environ/cocos2d/MainScene.cpp).
  2. Image::initWithRawData() accepts a Texture2D::PixelFormat
     (MainScene.cpp, cursor loading).
  3. FileUtilsLinux's private members are accessible to subclasses
     (CustomFileUtils derives from the platform FileUtils, like on Android).
  4. Explicit cstdint inclusion for GCC 13.
  5. Linux executable path fallback through AT_EXECFN when /proc is unavailable.
"""
import pathlib
import re
import sys

MARK = "[kirikinux2]"


def scroll_view(s):
    for name in ("minContainerOffset", "maxContainerOffset"):
        if re.search(r"virtual\s+Vec2\s+%s\s*\(\s*\)" % name, s):
            continue
        s, n = re.subn(r"(?m)^(\s*)Vec2\s+%s\s*\(\s*\)\s*;" % name,
                       r"\1virtual Vec2 %s(); // %s overridden by kirikinux (MainScene.cpp)" % (name, MARK),
                       s, count=1)
        if n != 1:
            return None
    return s


def image(s):
    if "Texture2D::PixelFormat /*format*/" in s:
        s = s.replace("Texture2D::PixelFormat /*format*/", "Texture2D::PixelFormat format")
        s = s.replace("{ return initWithRawData(data, dataLen, width, height, 8, preMulti); }",
                      "{ return format == Texture2D::PixelFormat::RGBA8888 && initWithRawData(data, dataLen, width, height, 8, preMulti); }")
    if re.search(r"Texture2D::PixelFormat\s+format", s):
        return s
    m = re.search(r"(?m)^([ \t]*)bool\s+initWithRawData\s*\([^;{]*?int\s+bitsPerComponent\s*,"
                  r"\s*bool\s+preMulti\s*=\s*false\s*\)\s*;[^\n]*\n", s)
    if not m:
        return None
    ind = m.group(1)
    add = (f"{ind}// {MARK} kirikinux passes the pixel format (zeas2's cocos2d-x); raw data is RGBA8888 anyway\n"
           f"{ind}bool initWithRawData(const unsigned char * data, ssize_t dataLen, int width, int height,\n"
           f"{ind}                     Texture2D::PixelFormat format, bool preMulti = false)\n"
           f"{ind}{{ return format == Texture2D::PixelFormat::RGBA8888 && initWithRawData(data, dataLen, width, height, 8, preMulti); }}\n")
    return s[:m.end()] + add + s[m.end():]


def file_utils_linux(s):
    if "protected: // %s CustomFileUtils" % MARK in s:
        return s
    s2, n = re.subn(r"(?m)^(\s*)private\s*:", r"\1protected: // %s CustomFileUtils derives from FileUtilsLinux" % MARK, s)
    return s2 if n > 0 else None


def allocator_base(s):
    if '#include <cstdint>' in s:
        return s
    old = '#include <string>'
    if old not in s:
        return None
    return s.replace(old, old + '\n#include <cstdint> // [kirikinux2] uintptr_t/intptr_t on GCC 13', 1)


def file_utils_linux_impl(s):
    if '[kirikinux2] AT_EXECFN' in s:
        return s
    old = '    if (length <= 0) {\n        return false;\n    }'
    if old not in s or 'char fullpath[256]' not in s:
        return None
    s = s.replace('#include <errno.h>', '#include <errno.h>\n#include <sys/auxv.h>\n#include <limits.h>\n#include <stdlib.h>')
    s = s.replace('char fullpath[256]', 'char fullpath[PATH_MAX]')
    return s.replace(old, '''    if (length <= 0) {
        // [kirikinux2] AT_EXECFN also works when /proc is unavailable.
        const char *execfn = reinterpret_cast<const char *>(getauxval(AT_EXECFN));
        if (!execfn || !realpath(execfn, fullpath)) return false;
        length = strlen(fullpath);
    }''', 1)


PATCHES = [
    ("cocos/base/allocator/CCAllocatorBase.h", allocator_base),
    ("extensions/GUI/CCScrollView/CCScrollView.h", scroll_view),
    ("cocos/platform/CCImage.h", image),
    ("cocos/platform/linux/CCFileUtils-linux.h", file_utils_linux),
    ("cocos/platform/linux/CCFileUtils-linux.cpp", file_utils_linux_impl),
]


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = pathlib.Path(sys.argv[1])
    failed = False
    for rel, fn in PATCHES:
        path = root / rel
        if not path.exists():
            print(f"patch-cocos2dx: missing {rel}")
            failed = True
            continue
        text = path.read_text(encoding="utf-8", errors="surrogateescape")
        new = fn(text)
        if new is None:
            print(f"patch-cocos2dx: pattern not found in {rel} (not cocos2d-x 3.17.x?)")
            failed = True
        elif new != text:
            path.write_text(new, encoding="utf-8", errors="surrogateescape")
            print(f"patch-cocos2dx: patched {rel}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
