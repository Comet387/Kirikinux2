// SPDX-License-Identifier: AGPL-3.0-only
// Headless regression harness for the ported psbfile plugin.
//
//   psb_harness FILE.psb [TJS-expression ...]
//
// Loads FILE through PSB::PSBFile (the same code the engine plugin uses),
// converts the root to TJS exactly as `new PSBFile(file).root` does, binds it
// to the global `root`, then evaluates each expression and prints the result.
// Exit status is non-zero if loading or any expression fails.
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "tjs.h"
#include "tjsDictionary.h"
#include "UtilStreams.h"
#include "psbfile/PSBFile.h"

using namespace TJS;

namespace {
class FileStream : public tTJSBinaryStream {
    FILE *fp_;
public:
    explicit FileStream(FILE *fp) : fp_(fp) {}
    ~FileStream() override { std::fclose(fp_); }
    tjs_uint64 TJS_INTF_METHOD Seek(tjs_int64 off, tjs_int whence) override {
        int w = whence == TJS_BS_SEEK_CUR ? SEEK_CUR
            : whence == TJS_BS_SEEK_END   ? SEEK_END
                                          : SEEK_SET;
        fseeko(fp_, off, w);
        return static_cast<tjs_uint64>(ftello(fp_));
    }
    tjs_uint TJS_INTF_METHOD Read(void *b, tjs_uint n) override {
        return static_cast<tjs_uint>(std::fread(b, 1, n, fp_));
    }
    tjs_uint TJS_INTF_METHOD Write(const void *, tjs_uint) override { return 0; }
    tjs_uint64 TJS_INTF_METHOD GetSize() override {
        auto cur = ftello(fp_);
        fseeko(fp_, 0, SEEK_END);
        auto s = ftello(fp_);
        fseeko(fp_, cur, SEEK_SET);
        return static_cast<tjs_uint64>(s);
    }
};
} // namespace

tTJSBinaryStream *TVPCreateStream(const ttstr &name, tjs_uint32) {
    FILE *fp = std::fopen(name.AsStdString().c_str(), "rb");
    if(!fp) TJS_eTJSError(ttstr(TJS_W("cannot open: ")) + name);
    return new FileStream(fp);
}

int main(int argc, char **argv) {
    if(argc < 2) {
        std::cerr << "usage: psb_harness FILE [expr...]\n";
        return 2;
    }
    auto *engine = new tTJS();
    int rc = 0;
    try {
        PSB::PSBFile psb;
        if(!psb.loadPSBFile(ttstr(argv[1]))) {
            std::cerr << "load failed\n";
            return 1;
        }
        std::cout << "version=" << psb.getPSBHeader().version
                  << " type=" << static_cast<int>(psb.getType())
                  << " resources=" << psb.resources.size() << '\n';
        iTJSDispatch2 *dic = TJSCreateDictionaryObject();
        for(const auto &kv : *psb.getObjects()) {
            tTJSVariant v = kv.second->toTJSVal();
            dic->PropSet(TJS_MEMBERENSURE, ttstr(kv.first).c_str(), nullptr, &v, dic);
        }
        tTJSVariant rootVar(dic, dic);
        dic->Release();
        iTJSDispatch2 *global = engine->GetGlobalNoAddRef();
        global->PropSet(TJS_MEMBERENSURE, TJS_W("root"), nullptr, &rootVar, global);
        for(int i = 2; i < argc; ++i) {
            tTJSVariant result;
            try {
                engine->EvalExpression(ttstr(argv[i]), &result);
                std::cout << argv[i] << " => " << ttstr(result).AsStdString() << '\n';
            } catch(const eTJS &e) {
                std::cout << argv[i] << " !! " << e.GetMessage().AsStdString() << '\n';
                rc = 1;
            }
        }
    } catch(const eTJS &e) {
        std::cerr << "TJS error: " << e.GetMessage().AsStdString() << '\n';
        rc = 1;
    } catch(const std::exception &e) {
        std::cerr << "error: " << e.what() << '\n';
        rc = 1;
    }
    engine->Shutdown();
    engine->Release();
    return rc;
}
