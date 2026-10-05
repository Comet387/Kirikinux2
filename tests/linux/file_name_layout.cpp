// SPDX-License-Identifier: AGPL-3.0-only
#include "FileNameLayout.h"
#include <cassert>
#include <iostream>
static float width(const std::string &value) {
    float sum = 0;
    for (unsigned char c : value) if ((c & 0xc0) != 0x80) sum += c < 0x80 ? 1 : 2;
    return sum;
}
int main() {
    const std::string name = "超长目录与😀 emoji 混合以及换行\nfilename.xp3";
    for (int available = 0; available < 100; ++available) {
        const auto result = kirikinux::FitFileName(name, available, width);
        assert(width(result) <= available);
        assert(kirikinux::FileNameForDisplay(result) == result);
        assert(result.find('\n') == std::string::npos);
    }
    const auto invalid = kirikinux::FileNameForDisplay(std::string("bad\xc0\xaf\xed\xa0\x80"));
    assert(kirikinux::FileNameForDisplay(invalid) == invalid);
    assert(kirikinux::FitFileName("small.xp3", 100, width) == "small.xp3");
    std::cout << "UTF-8 filename truncation and narrow widths passed\n";
}
