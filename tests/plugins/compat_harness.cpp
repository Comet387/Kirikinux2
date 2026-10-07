// SPDX-License-Identifier: AGPL-3.0-only
// Headless check of the small compatibility plugins (getLangName, layerExSave,
// PackinOne, win32ole) through the real ncbind registration path.
#include <iostream>
#include <set>
#include "tjs.h"
#include "ncbind.hpp"
using namespace TJS;

static tTJS *gEngine;
iTJSDispatch2 *TVPGetScriptDispatch() {
    iTJSDispatch2 *g = gEngine->GetGlobal();
    return g; // caller releases
}
void TVPThrowExceptionMessage(const tjs_char *msg) { TJS_eTJSError(msg); }
void TVPThrowExceptionMessage(const tjs_char *msg, const ttstr &p1) {
    TJS_eTJSError(ttstr(msg) + TJS_W(" ") + p1);
}
void TVPExecuteExpression(const ttstr &content, tTJSVariant *result) {
    gEngine->EvalExpression(content, result);
}

void TVPAddLog(const ttstr &s) { std::cerr << s.AsStdString() << "\n"; }

static int check(const char *expr, const char *expect) {
    tTJSVariant r;
    try {
        gEngine->EvalExpression(ttstr(expr), &r);
    } catch(const eTJS &e) {
        std::cout << "FAIL " << expr << " !! " << e.GetMessage().AsStdString() << "\n";
        return 1;
    }
    std::string got = ttstr(r).AsStdString();
    bool ok = !expect || got == expect;
    std::cout << (ok ? "ok   " : "FAIL ") << expr << " => " << got << "\n";
    return ok ? 0 : 1;
}

int main() {
    gEngine = new tTJS();
    // minimal stand-ins for the engine's System / Layer classes
    tTJSVariant dummy;
    gEngine->ExecScript(ttstr(
        "global.System = %[]; global.Layer = %[];"
        "Layer.saveLayerImage = function(n, t) { global.lastSave = n + ':' + t; };"));
    ncbAutoRegister::AllRegist();
    int fails = 0;
    for(const tjs_char *m : { TJS_W("getLangName.dll"), TJS_W("layerExSave.dll"),
                              TJS_W("win32ole.dll"), TJS_W("PackinOne.dll") }) {
        if(!ncbAutoRegister::LoadModule(m)) { std::cout << "FAIL load " << ttstr(m).AsStdString() << "\n"; fails++; }
    }
    setenv("KR2_UI_LANG", "ja_JP.UTF-8", 1);
    fails += check("System.getCurrentUILangName()", "ja-JP");
    fails += check("Layer.saveLayerImagePng('a.png', 9), lastSave", "a.png:png");
    fails += check("Layer.saveLayerImageTlg5('b.tlg'), lastSave", "b.tlg:tlg5");
    fails += check("Layer.saveLayerImageJpg('c.jpg', 90), lastSave", "c.jpg:jpg");
    fails += check("typeof global.WIN32OLE", "Object");
    fails += check("(function(){ try { new WIN32OLE('SAPI.SpVoice'); return 'no-throw'; } catch(e) { return 'threw'; } })()", "threw");
    std::cout << (fails ? "FAILED " : "ALL PASSED ") << fails << "\n";
    return fails ? 1 : 0;
}
