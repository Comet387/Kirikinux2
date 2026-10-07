// SPDX-License-Identifier: AGPL-3.0-only
#include "FontNameTable.h"
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

const int kLangEnglish = 0x409;

void appendCodePoint(std::u16string &out, unsigned cp) {
	if (cp == 0) return;
	if (cp >= 0x10000 && cp <= 0x10FFFF) {
		cp -= 0x10000;
		out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
		out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
	} else if (cp < 0x10000) {
		out.push_back(static_cast<char16_t>(cp));
	}
}

// Decodes one name-table record; returns false for encodings we do not read.
bool decodeName(const FT_SfntName &name, std::u16string &out, int &langId) {
	out.clear();
	langId = kLangEnglish;
	const FT_Byte *s = name.string;
	const FT_UInt n = name.string_len;
	if (name.platform_id == TT_PLATFORM_MICROSOFT) {
		langId = name.language_id;
		if (name.encoding_id == TT_MS_ID_UCS_4) {
			for (FT_UInt i = 0; i + 3 < n; i += 4)
				appendCodePoint(out, (unsigned(s[i]) << 24) | (unsigned(s[i + 1]) << 16) | (unsigned(s[i + 2]) << 8) | s[i + 3]);
		} else if (name.encoding_id == TT_MS_ID_UNICODE_CS || name.encoding_id == TT_MS_ID_SYMBOL_CS) {
			for (FT_UInt i = 0; i + 1 < n; i += 2) {
				const char16_t c = static_cast<char16_t>((s[i] << 8) | s[i + 1]);
				if (c) out.push_back(c);
			}
		} else {
			return false;
		}
	} else if (name.platform_id == TT_PLATFORM_APPLE_UNICODE) {
		for (FT_UInt i = 0; i + 1 < n; i += 2) {
			const char16_t c = static_cast<char16_t>((s[i] << 8) | s[i + 1]);
			if (c) out.push_back(c);
		}
	} else if (name.platform_id == TT_PLATFORM_MACINTOSH && name.encoding_id == TT_MAC_ID_ROMAN &&
	           name.language_id == TT_MAC_LANGID_ENGLISH) {
		for (FT_UInt i = 0; i < n; ++i)
			if (s[i] && s[i] < 0x80) out.push_back(static_cast<char16_t>(s[i]));
	} else {
		return false;
	}
	// trim
	while (!out.empty() && (out.back() == u' ' || out.back() == 0x3000)) out.pop_back();
	size_t b = 0;
	while (b < out.size() && (out[b] == u' ' || out[b] == 0x3000)) ++b;
	out.erase(0, b);
	return !out.empty();
}

bool isCJKLang(int lang) {
	switch (lang) {
	case 0x411: case 0x804: case 0x404: case 0xC04: case 0x1004: case 0x1404: case 0x412: case 0x812:
		return true;
	default:
		return false;
	}
}

void addUnique(std::vector<std::u16string> &v, const std::u16string &s) {
	if (!s.empty() && std::find(v.begin(), v.end(), s) == v.end()) v.push_back(s);
}

std::u16string fromUtf8(const char *s) {
	std::u16string out;
	if (!s) return out;
	const unsigned char *p = reinterpret_cast<const unsigned char *>(s);
	while (*p) {
		unsigned cp = *p++;
		int extra = 0;
		if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
		else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
		else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
		else if (cp >= 0x80) continue; // stray continuation byte
		for (; extra > 0 && (*p & 0xC0) == 0x80; --extra) cp = (cp << 6) | (*p++ & 0x3F);
		if (extra == 0) appendCodePoint(out, cp);
	}
	return out;
}

} // namespace

std::vector<int> KR2FontPreferredLangs(const char *locale) {
	std::string l = locale ? locale : "";
	std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	std::replace(l.begin(), l.end(), '-', '_');
	auto starts = [&](const char *p) { return l.compare(0, std::strlen(p), p) == 0; };
	if (starts("zh_tw") || starts("zh_hant"))
		return {0x404, 0xC04, 0x1404, 0x804, 0x411, kLangEnglish};
	if (starts("zh_hk") || starts("zh_mo"))
		return {0xC04, 0x1404, 0x404, 0x804, 0x411, kLangEnglish};
	if (starts("zh"))
		return {0x804, 0x1004, 0x404, 0x411, kLangEnglish};
	if (starts("ja"))
		return {0x411, kLangEnglish};
	if (starts("ko"))
		return {0x412, kLangEnglish, 0x411};
	return {kLangEnglish, 0x411};
}

const std::vector<int> &KR2FontPreferredLangsFromEnvironment() {
	static const std::vector<int> langs = [] {
		for (const char *var : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
			const char *v = std::getenv(var);
			if (v && *v && std::strcmp(v, "C") != 0 && std::strcmp(v, "POSIX") != 0)
				return KR2FontPreferredLangs(v);
		}
		return KR2FontPreferredLangs(nullptr);
	}();
	return langs;
}

int KR2FontLangRank(int langId, const std::vector<int> &preferredLangs) {
	for (size_t i = 0; i < preferredLangs.size(); ++i)
		if (preferredLangs[i] == langId) return (int)i;
	return (int)preferredLangs.size() + (langId == kLangEnglish ? 0 : 1);
}

KR2FontFaceNames KR2CollectFontFaceNames(FT_Face face, const std::vector<int> &preferredLangs) {
	KR2FontFaceNames r;
	r.regular = !(face->style_flags & (FT_STYLE_FLAG_BOLD | FT_STYLE_FLAG_ITALIC));

	int bestDisplayRank = 1 << 30;
	int bestDisplayId = 0;
	const FT_UInt count = FT_Get_Sfnt_Name_Count(face);
	for (FT_UInt i = 0; i < count; ++i) {
		FT_SfntName name;
		if (FT_Get_Sfnt_Name(face, i, &name)) continue;
		if (name.name_id != TT_NAME_ID_FONT_FAMILY && name.name_id != TT_NAME_ID_FULL_NAME &&
		    name.name_id != TT_NAME_ID_TYPOGRAPHIC_FAMILY)
			continue;
		std::u16string text;
		int lang;
		if (!decodeName(name, text, lang)) continue;
		if (name.name_id == TT_NAME_ID_FULL_NAME) {
			addUnique(r.fullNames, text);
			continue;
		}
		addUnique(r.familyNames, text);
		if (name.platform_id == TT_PLATFORM_MICROSOFT && isCJKLang(lang) && !r.nativeLang)
			r.nativeLang = lang;
		// Windows lists the name-ID-1 family; the typographic family only as a fallback.
		const int rank = KR2FontLangRank(lang, preferredLangs) * 2 + (name.name_id == TT_NAME_ID_FONT_FAMILY ? 0 : 1);
		if (rank < bestDisplayRank) {
			bestDisplayRank = rank;
			bestDisplayId = name.name_id;
			r.display = text;
		}
	}
	(void)bestDisplayId;

	const std::u16string family = fromUtf8(face->family_name);
	const std::u16string style = fromUtf8(face->style_name);
	addUnique(r.familyNames, family);
	if (!family.empty() && !style.empty() && style != u"Regular" && style != u"Normal" && style != u"Book")
		addUnique(r.fullNames, family + u" " + style);
	if (r.display.empty()) r.display = family;
	return r;
}

std::u16string KR2NormalizeFontName(const std::u16string &name) {
	std::u16string out;
	out.reserve(name.size());
	for (char16_t c : name) {
		if (c == u' ' || c == 0x3000 || c == u'\t') continue;
		if (c >= 0xFF01 && c <= 0xFF5E) c = static_cast<char16_t>(c - 0xFEE0); // full-width ASCII
		if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
		out.push_back(c);
	}
	return out;
}
