// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace kirikinux {
// Filenames remain unchanged on disk. Display controls on one line, replace
// invalid UTF-8, and only truncate at Unicode codepoint boundaries.
inline std::string FileNameForDisplay(const std::string &source) {
    std::string result;
    for (size_t i = 0; i < source.size();) {
        const unsigned char c = source[i];
        size_t length = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 :
            c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        bool valid = length && i + length <= source.size();
        for (size_t j = 1; valid && j < length; ++j)
            valid = (static_cast<unsigned char>(source[i + j]) & 0xc0) == 0x80;
        if (valid && length > 2) {
            const unsigned char second = source[i + 1];
            valid = !(c == 0xe0 && second < 0xa0) && !(c == 0xed && second >= 0xa0) &&
                !(c == 0xf0 && second < 0x90) && !(c == 0xf4 && second >= 0x90);
        }
        if (!valid) { result += "\xef\xbf\xbd"; ++i; }
        else if (c < 0x20 || c == 0x7f) { result += ' '; ++i; }
        else { result.append(source, i, length); i += length; }
    }
    return result;
}

template <typename Measure>
inline std::string FitFileName(const std::string &source, float width, Measure measure) {
    const std::string text = FileNameForDisplay(source);
    if (width <= 0) return "";
    if (measure(text) <= width) return text;
    std::string suffix = "...";
    while (!suffix.empty() && measure(suffix) > width) suffix.pop_back();
    if (suffix.empty()) return "";
    std::vector<size_t> boundaries(1, 0);
    for (size_t i = 1; i <= text.size(); ++i)
        if (i == text.size() || (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
            boundaries.push_back(i);
    size_t low = 0, high = boundaries.size() - 1;
    while (low < high) {
        const size_t mid = (low + high + 1) / 2;
        if (measure(text.substr(0, boundaries[mid]) + suffix) <= width) low = mid;
        else high = mid - 1;
    }
    return text.substr(0, boundaries[low]) + suffix;
}
} // namespace kirikinux
