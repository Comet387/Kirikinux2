// SPDX-License-Identifier: AGPL-3.0-only
// Regression for psbfile.dll: `var f = new PSBFile(); f.load(storage)` used to
// dereference an unassigned module-local PSBMedia pointer (SIGSEGV as soon as
// psdlayer.tjs loaded the first .pimg).  Runs the real ncbind registration,
// PSBFile.load and the psb:// media lookups against a real PSB file.
//   psbmedia_harness FILE.psb|.pimg|.mtn
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include "tjs.h"
#include "ncbind.hpp"
#include "StorageIntf.h"
#include "UtilStreams.h"
using namespace TJS;

static tTJS *gEngine;
static iTVPStorageMedia *gMedia; // the media psbfile.dll registered

iTJSDispatch2 *TVPGetScriptDispatch() { return gEngine->GetGlobal(); }
void TVPThrowExceptionMessage(const tjs_char *msg) { TJS_eTJSError(msg); }
void TVPThrowExceptionMessage(const tjs_char *msg, const ttstr &p1) {
    TJS_eTJSError(ttstr(msg) + TJS_W(" ") + p1);
}
void TVPExecuteExpression(const ttstr &content, tTJSVariant *result) {
    gEngine->EvalExpression(content, result);
}
void TVPAddLog(const ttstr &s) { std::cerr << s.AsStdString() << "\n"; }
void TVPRegisterStorageMedia(iTVPStorageMedia *m) { m->AddRef(); gMedia = m; }
void TVPUnregisterStorageMedia(iTVPStorageMedia *m) { if(m == gMedia) gMedia = nullptr; m->Release(); }
ttstr TVPExtractStorageName(const ttstr &name) {
    const tjs_char *s = name.c_str(), *p = s + name.length();
    while(p > s && p[-1] != TJS_W('/') && p[-1] != TJS_W('\\') && p[-1] != TJS_W('>')) --p;
    return ttstr(p);
}
tTJSBinaryStream *TVPCreateStream(const ttstr &name, tjs_uint32) {
    FILE *fp = std::fopen(name.AsStdString().c_str(), "rb");
    if(!fp) TJS_eTJSError(ttstr(TJS_W("cannot open ")) + name);
    std::vector<char> buf;
    char tmp[65536];
    size_t n;
    while((n = std::fread(tmp, 1, sizeof tmp, fp)) > 0) buf.insert(buf.end(), tmp, tmp + n);
    std::fclose(fp);
    auto *ms = new tTVPMemoryStream(); // owns a copy (the block ctor only references)
    ms->WriteBuffer(buf.data(), static_cast<tjs_uint>(buf.size()));
    ms->Seek(0, TJS_BS_SEEK_SET);
    return ms;
}

int main(int argc, char **argv) {
    if(argc < 2) { std::cerr << "usage: psbmedia_harness FILE\n"; return 2; }
    gEngine = new tTJS();
    ncbAutoRegister::AllRegist(); // builds the internal plugin table
    if(!ncbAutoRegister::LoadModule(TJS_W("psbfile.dll"))) { std::cout << "FAIL register\n"; return 1; }
    if(!gMedia) { std::cout << "FAIL psb media not registered\n"; return 1; }
    ttstr path(argv[1]);
    tTJSVariant ok;
    try {
        gEngine->EvalExpression(ttstr(TJS_W("(global.__psb = new PSBFile()).load(\"")) + path + TJS_W("\")"), &ok);
    } catch(const eTJS &e) {
        std::cout << "FAIL load threw " << e.GetMessage().AsStdString() << "\n";
        return 1;
    }
    std::cout << "load() result type=" << (int)ok.Type() << "\n";
    if(ok.Type() != tvtInteger || ok.AsInteger() == 0) { std::cout << "FAIL load returned false\n"; return 1; }
    std::cout << "PASS PSBFile.load()\n";
    // Old-style constructor must keep working too.
    try {
        gEngine->EvalExpression(ttstr(TJS_W("global.__psb2 = new PSBFile(\"")) + path + TJS_W("\")"), &ok);
        std::cout << "PASS new PSBFile(path)\n";
    } catch(const eTJS &e) {
        std::cout << "FAIL ctor threw " << e.GetMessage().AsStdString() << "\n";
        return 1;
    }
    // A missing name must report "not existent" and throw on open, not crash.
    if(gMedia->CheckExistentStorage(TJS_W("nothing.pimg/0.tlg"))) { std::cout << "FAIL phantom resource\n"; return 1; }
    try { delete gMedia->Open(TJS_W("nothing.pimg/0.tlg"), 0); std::cout << "FAIL open of missing resource\n"; return 1; }
    catch(const eTJS &) { std::cout << "PASS missing resource throws\n"; }
    ttstr none(TJS_W("psb://x/y"));
    gMedia->GetLocallyAccessibleName(none);
    if(!none.IsEmpty()) { std::cout << "FAIL locally accessible psb name\n"; return 1; }
    std::cout << "PASS not locally accessible\n";
    if(argc >= 3) { // optional: a resource name that must exist, e.g. title.pimg/123.tlg
        ttstr res(argv[2]);
        if(!gMedia->CheckExistentStorage(res)) { std::cout << "FAIL resource " << argv[2] << "\n"; return 1; }
        tTJSBinaryStream *s = gMedia->Open(res, 0);
        std::cout << "PASS resource " << argv[2] << " bytes=" << s->GetSize() << "\n";
        delete s;
    }
    return 0;
}
