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
void TVPExecuteScript(const ttstr &content, tTJSVariant *result) {
    gEngine->ExecScript(content, result);
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
        "Layer.saveLayerImage = function(n, t) { global.lastSave = n + ':' + t; };"
        "System.inform = function(t, c) { global.informCount = (typeof global.informCount == 'undefined' ? 0 : global.informCount) + 1; return 0; };"));
    try {
        ncbAutoRegister::AllRegist();
    } catch(const eTJS &e) {
        std::cout << "FAIL pre-register: " << e.GetMessage().AsStdString() << "\n";
        return 1;
    }
    int fails = 0;
    for(const tjs_char *m : { TJS_W("getLangName.dll"), TJS_W("layerExSave.dll"),
                              TJS_W("win32ole.dll"), TJS_W("PackinOne.dll"),
                              TJS_W("win32dialog.dll") }) {
        try {
            if(!ncbAutoRegister::LoadModule(m)) { std::cout << "FAIL load " << ttstr(m).AsStdString() << "\n"; fails++; }
        } catch(const eTJSScriptError &e) {
            std::cout << "FAIL load " << ttstr(m).AsStdString() << ": " << e.GetMessage().AsStdString()
                      << " line " << e.GetSourceLine() << " block " << e.GetBlockName() << "\n";
            fails++;
        } catch(const eTJS &e) {
            std::cout << "FAIL load " << ttstr(m).AsStdString() << ": " << e.GetMessage().AsStdString() << "\n";
            fails++;
        }
    }
    setenv("KR2_UI_LANG", "ja_JP.UTF-8", 1);
    fails += check("System.getCurrentUILangName()", "ja-JP");
    fails += check("Layer.saveLayerImagePng('a.png', 9), lastSave", "a.png:png");
    fails += check("Layer.saveLayerImageTlg5('b.tlg'), lastSave", "b.tlg:tlg5");
    fails += check("Layer.saveLayerImageJpg('c.jpg', 90), lastSave", "c.jpg:jpg");
    fails += check("typeof global.WIN32OLE", "Object");
    fails += check("(function(){ try { new WIN32OLE('SAPI.SpVoice'); return 'no-throw'; } catch(e) { return 'threw'; } })()", "threw");
    // win32dialog.dll: the member initialisers of KiriKiri's win32dialog.tjs
    // (WIN32DialogEX) must succeed, and finalize() must find allBitmaps.
    gEngine->ExecScript(ttstr(
        "class W32Ex extends WIN32Dialog {"
        "  function W32Ex(o) { super.WIN32Dialog(null); }"
        "  function finalize() { removeAllBitmap(); super.finalize(...); }"
        "  var Header = global.WIN32Dialog.Header; var Items = global.WIN32Dialog.Items;"
        "  var DefaultStyles = %[ EditText: ES_LEFT|WS_BORDER|WS_TABSTOP, ListBox: LBS_NOTIFY|WS_BORDER|WS_VSCROLL ];"
        "  function store(elm) { var h = new Header(); h.store(elm); var i = new Items(); i.store(elm.items[0]);"
        "    makeTemplate(h, i); global.storedTitle = h.title; global.storedClass = i.windowClass; invalidate h; invalidate i; }"
        "  function removeAllBitmap() { for (var i = allBitmaps.count-1; i >= 0; i--) invalidate allBitmaps[i]; allBitmaps.clear(); }"
        "  var allBitmaps = [];"
        "}"));
    fails += check("(function(){ var d = new W32Ex(null); d.store(%[ title:'Font', items:[ %[ windowClass:WIN32Dialog.LISTBOX ] ] ]);"
                   " var r = d.open(null); d.open(null); invalidate d; return r; })()", "2");
    fails += check("storedTitle + ':' + storedClass", "Font:131");
    fails += check("informCount", "1");
    fails += check("WIN32Dialog.BM_GETCHECK + ',' + WIN32Dialog.CB_SETCURSEL + ',' + WIN32Dialog.IDOK", "240,334,1");
    fails += check("(new WIN32Dialog(null)).messageBox('m', 'c', 0)", "1");
    std::cout << (fails ? "FAILED " : "ALL PASSED ") << fails << "\n";
    return fails ? 1 : 0;
}
