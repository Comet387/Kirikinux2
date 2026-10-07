// SPDX-License-Identifier: AGPL-3.0-only
// Checks that Windows (GDI) face names are read from the font name table.
//   font_name_table FONT [INDEX] [LOCALE] [EXPECTED_NAME...]
// Prints every name; fails if an EXPECTED_NAME is not among the aliases.
#include "FontNameTable.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string u8(const std::u16string &s) {
	std::string o;
	for (size_t i = 0; i < s.size(); ++i) {
		unsigned c = s[i];
		if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size()) c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
		if (c < 0x80) o += (char)c;
		else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
		else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
		else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
	}
	return o;
}
static std::u16string u16(const char *s) {
	std::u16string o;
	const unsigned char *p = (const unsigned char *)s;
	while (*p) {
		unsigned c = *p++; int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
		if (n) c &= (0x3F >> n);
		while (n-- && *p) c = (c << 6) | (*p++ & 0x3F);
		if (c >= 0x10000) { c -= 0x10000; o += (char16_t)(0xD800 + (c >> 10)); o += (char16_t)(0xDC00 + (c & 0x3FF)); }
		else o += (char16_t)c;
	}
	return o;
}

int main(int argc, char **argv) {
	if (argc < 2) { std::fprintf(stderr, "usage: %s FONT [INDEX] [LOCALE] [EXPECTED...]\n", argv[0]); return 2; }
	FT_Library lib; FT_Face face;
	if (FT_Init_FreeType(&lib) || FT_New_Face(lib, argv[1], argc > 2 ? std::atol(argv[2]) : 0, &face)) return 2;
	const KR2FontFaceNames n = KR2CollectFontFaceNames(face, KR2FontPreferredLangs(argc > 3 ? argv[3] : nullptr));
	std::printf("display: %s\nnativeLang: 0x%x regular: %d\n", u8(n.display).c_str(), n.nativeLang, n.regular);
	for (auto &s : n.familyNames) std::printf("family: %s\n", u8(s).c_str());
	for (auto &s : n.fullNames) std::printf("full:   %s\n", u8(s).c_str());
	int fails = 0;
	for (int i = 4; i < argc; ++i) {
		const std::u16string want = u16(argv[i]);
		bool found = false;
		for (auto &s : n.familyNames) found |= s == want;
		for (auto &s : n.fullNames) found |= s == want;
		std::printf("%s expected %s\n", found ? "ok  " : "FAIL", argv[i]);
		fails += !found;
	}
	if (KR2NormalizeFontName(u"ＭＳ Ｐゴシック") != u"mspゴシック" || KR2NormalizeFontName(u"Source Han Sans CN Bold") != u"sourcehansanscnbold") fails++;
	if (KR2NormalizeFontName(u"Source Han Sans CN Bold") != KR2NormalizeFontName(u"source han sans cn bold")) fails++;
	return fails ? 1 : 0;
}
