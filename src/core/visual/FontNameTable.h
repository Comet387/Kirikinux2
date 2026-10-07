// SPDX-License-Identifier: AGPL-3.0-only
// Font face names as Windows (GDI) games expect them.
//
// KiriKiri games ask for faces by their Windows family name, e.g.
// "Source Han Sans CN Bold" (English name-table entry, name ID 1) or
// "源ノ角ゴシック JP Bold" (Japanese entry).  FreeType's family_name is the
// typographic family ("Source Han Sans CN"), so a lookup by the Windows name
// used to miss and the text silently fell back to the default font.
// This helper reads every family / full name from the OpenType name table.
#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <vector>

struct KR2FontFaceNames {
	// Names that select exactly this face (full names, "family style").
	std::vector<std::u16string> fullNames;
	// Family names (name IDs 1 and 16, FreeType family).  Several styles of a
	// family share these; the registry prefers the regular style for them.
	std::vector<std::u16string> familyNames;
	// One name for font lists (Font.getList), in the preferred language.
	std::u16string display;
	// Windows LANGID of a localized (CJK) family name, 0 if none.  Used to pick
	// e.g. the Japanese or Simplified Chinese face of a font collection.
	int nativeLang = 0;
	// Not bold / italic.
	bool regular = true;
};

// Windows LANGIDs in order of preference for display names, derived from a
// POSIX locale string such as "zh_CN.UTF-8" (null/empty: English first).
std::vector<int> KR2FontPreferredLangs(const char *locale);

// Preferred languages from LANGUAGE / LC_ALL / LC_MESSAGES / LANG.
const std::vector<int> &KR2FontPreferredLangsFromEnvironment();

KR2FontFaceNames KR2CollectFontFaceNames(FT_Face face, const std::vector<int> &preferredLangs);

// Case-, width- and space-insensitive key for tolerant face lookups.
std::u16string KR2NormalizeFontName(const std::u16string &name);

// Rank of a LANGID in a preference list (lower is better).
int KR2FontLangRank(int langId, const std::vector<int> &preferredLangs);