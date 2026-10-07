#pragma once
#include "tjs.h"
#include "tjsHashSearch.h"
#include <functional>

void TVPInitFontNames();
int TVPEnumFontsProc(const ttstr &FontPath);
const ttstr &TVPGetDefaultFontName();
tTJSBinaryStream* TVPCreateFontStream(const ttstr &fontname);
struct TVPFontNamePathInfo {
    ttstr Path;
	std::function<tTJSBinaryStream*(TVPFontNamePathInfo*)> Getter;
    int Index = 0;      // face index inside a collection (.ttc/.otc)
    bool Regular = true; // not bold/italic: keeps shared family names
};
TVPFontNamePathInfo* TVPFindFont(const ttstr &name);

//---------------------------------------------------------------------------
// font enumeration and existence check
//---------------------------------------------------------------------------
class tTVPttstrHash
{
public:
    static tjs_uint32 Make(const ttstr &val);
};
extern tTJSHashTable<ttstr, TVPFontNamePathInfo, tTVPttstrHash>
    TVPFontNames;
