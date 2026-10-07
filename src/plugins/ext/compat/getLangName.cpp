// SPDX-License-Identifier: AGPL-3.0-only
// getLangName.dll for Kirikinux2.
//
// Windows original: System.getCurrentUILangName() returns the UI language as a
// locale name (LCIDToLocaleName style, e.g. "ja-JP", "zh-CN", "en-US").
// Linux: derived from LC_ALL / LC_MESSAGES / LANGUAGE / LANG ("ja_JP.UTF-8"
// -> "ja-JP"); KR2_UI_LANG overrides everything (e.g. KR2_UI_LANG=zh-TW).
// Used by Yuzusoft multilang.tjs to pick the default text language.
#include "ncbind.hpp"
#include <cstdlib>
#include <string>

#define NCB_MODULE_NAME TJS_W("getLangName.dll")

namespace {
std::string kr2LocaleToBcp47(std::string v) {
    // LANGUAGE may be a list "ja:en"; take the first entry
    auto colon = v.find(':');
    if(colon != std::string::npos) v = v.substr(0, colon);
    auto cut = v.find_first_of(".@");
    if(cut != std::string::npos) v = v.substr(0, cut);
    if(v.empty() || v == "C" || v == "POSIX") return "en-US";
    for(auto &c : v) if(c == '_') c = '-';
    // zh-Hans/zh-Hant style hints without region
    if(v == "zh") return "zh-CN";
    return v;
}

std::string kr2CurrentUILang() {
    const char *keys[] = { "KR2_UI_LANG", "LC_ALL", "LC_MESSAGES", "LANGUAGE", "LANG" };
    for(const char *k : keys) {
        const char *e = std::getenv(k);
        if(e && *e) return kr2LocaleToBcp47(e);
    }
    return "en-US";
}
} // namespace

struct GetLangNameCompat {
    static tjs_error TJS_INTF_METHOD getCurrentUILangName(tTJSVariant *result, tjs_int,
                                                          tTJSVariant **, iTJSDispatch2 *) {
        if(result) *result = ttstr(kr2CurrentUILang().c_str());
        return TJS_S_OK;
    }
};

NCB_ATTACH_CLASS(GetLangNameCompat, System) {
    RawCallback("getCurrentUILangName", &GetLangNameCompat::getCurrentUILangName, TJS_STATICMEMBER);
}
