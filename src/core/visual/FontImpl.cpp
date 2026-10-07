#include "FontImpl.h"
#include <ft2build.h>
#include FT_TRUETYPE_IDS_H
#include FT_SFNT_NAMES_H
#include FT_FREETYPE_H
#include "StorageIntf.h"
#include "DebugIntf.h"
#include "MsgIntf.h"
#include <algorithm>
#include <map>
#include <math.h>
#include "Application.h"
#include "Platform.h"
#include "ConfigManager/IndividualConfigManager.h"
#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

#ifdef _MSC_VER
#pragma comment(lib,"freetype.lib")
#endif
#include "platform/CCFileUtils.h"
#include "StorageImpl.h"
#include "BinaryStream.h"
#include "FontNameTable.h"
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <cstring>
#ifdef KR2_LINUX_HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

tTJSHashTable<ttstr, TVPFontNamePathInfo, tTVPttstrHash>
    TVPFontNames;
static ttstr TVPDefaultFontName;
const ttstr &TVPGetDefaultFontName() {
	return TVPDefaultFontName;
}

// Tolerant lookup (case/width/space-insensitive) -> registered name.
static std::unordered_map<std::u16string, ttstr> TVPFontNormalizedNames;
// One entry per face for Font.getList(), in registration order.
static std::vector<ttstr> TVPFontDisplayNames;
static std::unordered_set<std::u16string> TVPFontDisplaySet;

static std::u16string TVPFontKey(const ttstr &name) {
	return std::u16string(name.c_str(), name.c_str() + name.GetLen());
}

// familyAlias: a name shared by several styles (e.g. "Noto Sans CJK SC" for
// Regular and Bold). Such a name keeps pointing at the regular style.
static void TVPRegisterFontName(const ttstr &name, const TVPFontNamePathInfo &info, bool familyAlias) {
	if (name.IsEmpty()) return;
	if (familyAlias) {
		TVPFontNamePathInfo *existing = TVPFontNames.Find(name);
		if (existing && existing->Regular && !info.Regular) return;
	}
	TVPFontNames.Add(name, info);
	const std::u16string key = KR2NormalizeFontName(TVPFontKey(name));
	auto existing = TVPFontNormalizedNames.find(key);
	if (existing == TVPFontNormalizedNames.end())
		TVPFontNormalizedNames.emplace(key, name);
	else {
		TVPFontNamePathInfo *old = TVPFontNames.Find(existing->second);
		if (!old || (!old->Regular && info.Regular)) existing->second = name;
	}
}

static void TVPAddFontDisplayName(const ttstr &name) {
	if (name.IsEmpty()) return;
	if (TVPFontDisplaySet.insert(TVPFontKey(name)).second)
		TVPFontDisplayNames.push_back(name);
}

static void TVPRegisterFace(const KR2FontFaceNames &names, const TVPFontNamePathInfo &info) {
	for (const std::u16string &n : names.familyNames) TVPRegisterFontName(ttstr(n.c_str()), info, true);
	for (const std::u16string &n : names.fullNames) TVPRegisterFontName(ttstr(n.c_str()), info, false);
	// A style-linked bold face shares its family name with the regular face,
	// which is listed instead. Full names still select the exact face.
	if (names.regular || !TVPFontNames.Find(ttstr(names.display.c_str())))
		TVPAddFontDisplayName(ttstr(names.display.c_str()));
}

void TVPGetAllFontList(std::vector<ttstr>& list) {
	TVPInitFontNames();
	list.insert(list.end(), TVPFontDisplayNames.begin(), TVPFontDisplayNames.end());
}

// Faces seen by the current TVPInternalEnumFonts calls (default font choice).
struct TVPEnumeratedFace { ttstr Display; int NativeLang; };
static std::vector<TVPEnumeratedFace> *TVPEnumCollector = nullptr;
static FT_Library TVPFontLibrary;
FT_Library &TVPGetFontLibrary() {
	if (!TVPFontLibrary) {
		FT_Error error = FT_Init_FreeType(&TVPFontLibrary);
		if (error) TVPThrowExceptionMessage(
			(ttstr(TJS_W("Initialize FreeType failed, error = ")) + TJSIntegerToString((tjs_int)error)).c_str());
		TVPInitFontNames();
	}
	return TVPFontLibrary;
}
void TVPReleaseFontLibrary() {
	if (TVPFontLibrary) {
		FT_Done_FreeType(TVPFontLibrary);
	}
}
//---------------------------------------------------------------------------
static int TVPInternalEnumFonts(FT_Byte* pBuf, int buflen, const ttstr &FontPath, const std::function<tTJSBinaryStream*(TVPFontNamePathInfo*)>& getter) {
	unsigned int faceCount = 0;
	FT_Face fontface = nullptr;
	FT_Error error = FT_New_Memory_Face(TVPGetFontLibrary(), pBuf, buflen, 0, &fontface);
	if (error) {
		TVPAddLog(ttstr(TJS_W("Load Font \"" ) + FontPath + TJS_W("\" failed (")) + TJSIntegerToString((int)error) + TJS_W(")"));
		return faceCount;
	}
	const int nFaceNum = fontface->num_faces;
	for (int faceIndex = 0; faceIndex < nFaceNum; ++faceIndex) {
		if (faceIndex > 0) {
			FT_Done_Face(fontface);
			fontface = nullptr;
			if (FT_New_Memory_Face(TVPGetFontLibrary(), pBuf, buflen, faceIndex, &fontface))
				continue;
		}
		if (FT_IS_SCALABLE(fontface)) {
			const KR2FontFaceNames names = KR2CollectFontFaceNames(fontface, KR2FontPreferredLangsFromEnvironment());
			TVPFontNamePathInfo info;
			info.Path = FontPath;
			info.Index = faceIndex;
			info.Getter = getter;
			info.Regular = names.regular;
			TVPRegisterFace(names, info);
			if (TVPEnumCollector)
				TVPEnumCollector->push_back(TVPEnumeratedFace{ttstr(names.display.c_str()), names.nativeLang});
			++faceCount;
		}
		FT_Done_Face(fontface);
		fontface = nullptr;
	}
	return faceCount;
}

int TVPEnumFontsProc(const ttstr &FontPath)
{
    if(!TVPIsExistentStorageNoSearch(FontPath)) {
        return 0;
    }

    tTJSBinaryStream * Stream = TVPCreateStream(FontPath, TJS_BS_READ);
    if(!Stream) {
        return 0;
    }
    int bufflen = Stream->GetSize();
	std::vector<FT_Byte> buf; buf.resize(bufflen);
    Stream->ReadBuffer(&buf.front(), bufflen);
    delete Stream;
	return TVPInternalEnumFonts(&buf.front(), bufflen, FontPath, nullptr);
}

tTJSBinaryStream* TVPCreateFontStream(const ttstr &fontname)
{
	TVPFontNamePathInfo *info = TVPFindFont(fontname);
	if (!info) {
		info = TVPFontNames.Find(TVPDefaultFontName);
		if (!info) return nullptr;
	}
	if (info->Getter) {
		return info->Getter(info);
	}
	return TVPCreateBinaryStreamForRead(info->Path, TJS_W(""));
}

//---------------------------------------------------------------------------
#ifdef __ANDROID__
extern std::vector<ttstr> Android_GetExternalStoragePath();
extern ttstr Android_GetInternalStoragePath();
extern ttstr Android_GetApkStoragePath();
#endif
//---------------------------------------------------------------------------
// Default face: the bundled CJK font is often a collection with JP/KR/SC/TC
// faces. Pick the face for the player's language instead of whichever name was
// registered last.
static ttstr TVPChooseDefaultFace(const std::vector<TVPEnumeratedFace> &faces) {
	if (faces.empty()) return ttstr();
	const std::vector<int> &langs = KR2FontPreferredLangsFromEnvironment();
	const TVPEnumeratedFace *best = &faces.front();
	int bestRank = 1 << 30;
	for (const TVPEnumeratedFace &f : faces) {
		int rank = f.NativeLang ? KR2FontLangRank(f.NativeLang, langs) : 1000;
		// KiriKiri games are Japanese first: without a CJK preference use JP.
		if (f.NativeLang == 0x411) rank = std::min(rank, 500);
		if (rank < bestRank) { bestRank = rank; best = &f; }
	}
	return best->Display;
}

// Fonts installed on the system (Windows' Font.getList lists them too).
// Only names and paths are recorded; files are opened when a face is used.
static void TVPEnumSystemFonts() {
#ifdef KR2_LINUX_HAVE_FONTCONFIG
	const char *off = std::getenv("KIRIKINUX_NO_SYSTEM_FONTS");
	if (off && *off && *off != '0') return;
	if (!FcInit()) return;
	FcPattern *pattern = FcPatternCreate();
	FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
	FcObjectSet *objects = FcObjectSetBuild(FC_FAMILY, FC_FAMILYLANG, FC_FULLNAME,
		FC_STYLE, FC_FILE, FC_INDEX, FC_WEIGHT, FC_SLANT, FC_FONTFORMAT, nullptr);
	FcFontSet *set = FcFontList(nullptr, pattern, objects);
	FcObjectSetDestroy(objects);
	FcPatternDestroy(pattern);
	if (!set) return;
	const std::vector<int> &langs = KR2FontPreferredLangsFromEnvironment();
	auto langIdOf = [](const char *fcLang) -> int {
		std::string l = fcLang ? fcLang : "";
		if (l == "ja") return 0x411;
		if (l == "zh-cn" || l == "zh-sg") return 0x804;
		if (l == "zh-tw") return 0x404;
		if (l == "zh-hk" || l == "zh-mo") return 0xC04;
		if (l == "ko") return 0x412;
		return 0x409;
	};
	int added = 0;
	for (int i = 0; i < set->nfont; ++i) {
		FcPattern *font = set->fonts[i];
		FcChar8 *file = nullptr, *format = nullptr;
		int index = 0, weight = FC_WEIGHT_REGULAR, slant = FC_SLANT_ROMAN;
		if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch || !file) continue;
		if (FcPatternGetString(font, FC_FONTFORMAT, 0, &format) == FcResultMatch && format &&
			strcmp((const char *)format, "TrueType") != 0 && strcmp((const char *)format, "CFF") != 0)
			continue;
		FcPatternGetInteger(font, FC_INDEX, 0, &index);
		if (index >> 16) continue; // named instance of a variable font
		FcPatternGetInteger(font, FC_WEIGHT, 0, &weight);
		FcPatternGetInteger(font, FC_SLANT, 0, &slant);
		TVPFontNamePathInfo info;
		info.Path = ttstr((const char *)file);
		info.Index = index;
		info.Regular = weight >= FC_WEIGHT_BOOK && weight <= FC_WEIGHT_MEDIUM && slant == FC_SLANT_ROMAN;
		ttstr display;
		int bestRank = 1 << 30;
		FcChar8 *name = nullptr;
		for (int n = 0; FcPatternGetString(font, FC_FAMILY, n, &name) == FcResultMatch; ++n) {
			FcChar8 *lang = nullptr;
			FcPatternGetString(font, FC_FAMILYLANG, n, &lang);
			const ttstr family((const char *)name);
			TVPRegisterFontName(family, info, true);
			const int rank = KR2FontLangRank(langIdOf((const char *)lang), langs);
			if (rank < bestRank) { bestRank = rank; display = family; }
		}
		for (int n = 0; FcPatternGetString(font, FC_FULLNAME, n, &name) == FcResultMatch; ++n)
			TVPRegisterFontName(ttstr((const char *)name), info, false);
		if (info.Regular) TVPAddFontDisplayName(display);
		++added;
	}
	FcFontSetDestroy(set);
	TVPAddLog(ttstr(TJS_W("Kirikinux2: registered ")) + TJSIntegerToString(added) + TJS_W(" system font faces"));
#endif
}

void TVPInitFontNames()
{
    static bool TVPFontNamesInit = false;
    // enumlate all fonts
    if(TVPFontNamesInit) return;
	TVPFontNamesInit = true;
#ifdef __ANDROID__
	std::vector<ttstr> pathlist = Android_GetExternalStoragePath();
#endif
	// Installed fonts first; game-added fonts registered later can still replace
	// a matching alias through TVPRegisterFontName.
	TVPEnumSystemFonts();
	std::vector<TVPEnumeratedFace> defaultFaces;
	TVPEnumCollector = &defaultFaces;
	do {
		ttstr userFont = IndividualConfigManager::GetInstance()->GetValue<std::string>("default_font", "");
		if (!userFont.IsEmpty() && TVPEnumFontsProc(userFont)) break;

		if (TVPEnumFontsProc(TVPGetAppPath() + "default.ttf")) break;
		if (TVPEnumFontsProc(TVPGetAppPath() + "default.ttc")) break;
		if (TVPEnumFontsProc(TVPGetAppPath() + "default.otf")) break;
		if (TVPEnumFontsProc(TVPGetAppPath() + "default.otc")) break;
#if defined(__ANDROID__)
		int fontCount = 0;
		for (const ttstr &path : pathlist) {
			fontCount += TVPEnumFontsProc(path + "/default.ttf");
			if (fontCount) break;
		}
		if (fontCount) break;
		
		if (TVPEnumFontsProc(Android_GetInternalStoragePath() + "/default.ttf")) break;

		{	// from internal storage
			auto data = cocos2d::FileUtils::getInstance()->getDataFromFile("DroidSansFallback.ttf");
			if (TVPInternalEnumFonts(data.getBytes(), data.getSize(), "DroidSansFallback.ttf", [](TVPFontNamePathInfo* info)->tTJSBinaryStream* {
				auto data = cocos2d::FileUtils::getInstance()->getDataFromFile(info->Path.AsStdString());
				tTVPMemoryStream *ret = new tTVPMemoryStream();
				ret->WriteBuffer(data.getBytes(), data.getSize());
				ret->SetPosition(0);
				return ret;
			})) break;
		}
		if (TVPEnumFontsProc(TJS_W("file://./system/fonts/DroidSansFallback.ttf"))) break;
		if (TVPEnumFontsProc(TJS_W("file://./system/fonts/NotoSansHans-Regular.otf"))) break;
		if (TVPEnumFontsProc(TJS_W("file://./system/fonts/DroidSans.ttf"))) break;
#elif defined(WIN32)
		if (TVPEnumFontsProc(TJS_W("file://./c/windows/fonts/msyh.ttf"))) break;
		if (TVPEnumFontsProc(TJS_W("file://./c/windows/fonts/simhei.ttf"))) break;
#endif
        
        std::string fullPath = cocos2d::FileUtils::getInstance()->fullPathForFilename("DroidSansFallback.ttf");
        if (TVPEnumFontsProc(fullPath)) break;
	} while (false);
	TVPEnumCollector = nullptr;
	TVPDefaultFontName = TVPChooseDefaultFace(defaultFaces);

    // check exePath + "/fonts/*.ttf"
	{
		std::vector<ttstr> list;
		auto lister = [&](const ttstr &name, tTVPLocalFileInfo* s) {
			if (s->Mode & (S_IFREG | S_IFDIR)) {
				list.emplace_back(name);
			}
		};
#ifdef __ANDROID__
		TVPGetLocalFileListAt(Android_GetInternalStoragePath() + "/fonts", lister);
		for (const ttstr &path : pathlist) {
			TVPGetLocalFileListAt(path + "/fonts", lister);
		}
#endif
		TVPGetLocalFileListAt(TVPGetAppPath() + "/fonts", lister);
        auto itend = list.end();
        for (auto it = list.begin(); it != itend; ++it) {
            TVPEnumFontsProc(*it);
        }
    }

	if (TVPDefaultFontName.IsEmpty() && !TVPFontDisplayNames.empty())
		TVPDefaultFontName = TVPFontDisplayNames.front();
	if (TVPDefaultFontName.IsEmpty()) {
		TVPShowSimpleMessageBox(("Could not found any font.\nPlease ensure that at least \"default.ttf\" exists"), "Exception Occured");
    }
}
//---------------------------------------------------------------------------
TVPFontNamePathInfo* TVPFindFont(const ttstr &fontname)
{
	// check existence of font
	TVPInitFontNames();
	if (fontname.IsEmpty()) return nullptr;
	ttstr name = fontname;
	if (name[0] == TJS_W('@')) name = ttstr(fontname.c_str() + 1); // vertical version
	TVPFontNamePathInfo *info = TVPFontNames.Find(name);
	if (!info) info = TVPFontNames.Find(fontname);
	if (!info) {
		// Windows matches face names case-insensitively; also tolerate
		// full-width letters and spacing differences.
		auto it = TVPFontNormalizedNames.find(KR2NormalizeFontName(TVPFontKey(name)));
		if (it != TVPFontNormalizedNames.end()) info = TVPFontNames.Find(it->second);
	}
	return info;
}

tjs_uint32 tTVPttstrHash::Make( const ttstr &val )
{
    const tjs_char * ptr = val.c_str();
    if(*ptr == 0) return 0;
    tjs_uint32 v = 0;
    while(*ptr)
    {
        v += *ptr;
        v += (v << 10);
        v ^= (v >> 6);
        ptr++;
    }
    v += (v << 3);
    v ^= (v >> 11);
    v += (v << 15);
    if(!v) v = (tjs_uint32)-1;
    return v;
}
