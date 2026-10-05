// SPDX-License-Identifier: AGPL-3.0-only
// Exercise the production bootstrap/entry policy with the original TJS VM.
#include "StartupCompatibility.h"
#include "tjsError.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace TJS;

class MemoryStream : public tTJSBinaryStream {
public:
    std::vector<unsigned char> bytes;
    size_t position = 0;
    tjs_uint64 TJS_INTF_METHOD Seek(tjs_int64 offset, tjs_int whence) override {
        const tjs_int64 base = whence == TJS_BS_SEEK_CUR ? position :
            whence == TJS_BS_SEEK_END ? bytes.size() : 0;
        if (base + offset < 0) throw std::runtime_error("negative seek");
        return position = base + offset;
    }
    tjs_uint TJS_INTF_METHOD Read(void *buffer, tjs_uint length) override {
        length = std::min<size_t>(length, bytes.size() - std::min(position, bytes.size()));
        if (length) std::memcpy(buffer, bytes.data() + position, length);
        position += length;
        return length;
    }
    tjs_uint TJS_INTF_METHOD Write(const void *buffer, tjs_uint length) override {
        if (position + length > bytes.size()) bytes.resize(position + length);
        std::memcpy(bytes.data() + position, buffer, length);
        position += length;
        return length;
    }
    tjs_uint64 TJS_INTF_METHOD GetSize() override { return bytes.size(); }
};
static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
static tjs_int eval(tTJS &engine, const tjs_char *expression) {
    tTJSVariant result;
    engine.EvalExpression(expression, &result);
    return result.AsInteger();
}
int main() {
    try {
        auto release = [](tTJS *engine) { engine->Release(); };
        std::unique_ptr<tTJS, decltype(release)> mainOwner(new tTJS(), release);
        tTJS &engine = *mainOwner;
        TVPInitializeKAGStartupContext(engine, true);
        require(eval(engine, TJS_W("inXP3archivePacked && !convertMode && !debugWindowEnabled && !kirikiriz")), "packed context");
        engine.ExecScript(TJS_W("inXP3archivePacked = false; convertMode = true; debugWindowEnabled = true; kirikiriz = true;"));
        TVPInitializeKAGStartupContext(engine, true);
        require(eval(engine, TJS_W("!inXP3archivePacked && convertMode && debugWindowEnabled && kirikiriz")), "game/patch overrides lost");
        std::unique_ptr<tTJS, decltype(release)> looseOwner(new tTJS(), release);
        tTJS &loose = *looseOwner;
        TVPInitializeKAGStartupContext(loose, false);
        require(!eval(loose, TJS_W("inXP3archivePacked")), "loose context");

        // Android patches probe the optional Motion plugin before disabling its
        // D3D path.  Linux must keep that probe safe while reporting D3D as
        // unavailable; the compatibility methods are deliberately no-ops.
        engine.ExecScript(TJS_W(
            "if (typeof Motion.D3DAdaptor != 'undefined') throw 'unexpected D3D';"
            "with (Motion.Player) { .useD3D = 0; }"
            "Motion.ResourceManager.setEmotePSBDecryptSeed(1);"
            "Motion.EmotePlayer.setCameraCoord(0, 0);"
            "var compatResourceManager = new Motion.ResourceManager(1);"
            "compatResourceManager.addRef();"
            "compatResourceManager.release();"
            "compatResourceManager.load('missing.mtn');"
            "compatResourceManager.unload('missing.mtn');"
            "compatResourceManager.clearCache();"
            "compatResourceManager.finalize();"));

        // The real bytecode loader must run the complete startup, including
        // arbitrary game globals: the text decoder must never probe it first.
        MemoryStream compiled;
        engine.CompileScript(TJS_W("var gameBootSentinel = 123; var archiveBootValue = inXP3archivePacked;"), &compiled);
        compiled.position = 0;
        int fallbackCalls = 0;
        TVPExecuteStartupEntry(ttstr(TJS_W("startup.tjs")), [](const ttstr &) { return true; },
            [&](const ttstr &name) {
                if (name == TJS_W("System/Initialize.tjs")) ++fallbackCalls;
                require(engine.LoadByteCode(&compiled), "compiled startup not recognized");
            });
        require(eval(engine, TJS_W("gameBootSentinel")) == 123 && !fallbackCalls, "compiled bootstrap skipped");

        bool caught = false;
        try {
            TVPExecuteStartupEntry(ttstr(TJS_W("startup.tjs")), [](const ttstr &) { return true; },
                [&](const ttstr &name) {
                    if (name == TJS_W("System/Initialize.tjs")) ++fallbackCalls;
                    engine.ExecScript(TJS_W("throw 'original plugin/bootstrap error';"));
                });
        } catch (const eTJS &) { caught = true; }
        require(caught && !fallbackCalls, "original startup exception swallowed");

        TVPExecuteStartupEntry(ttstr(TJS_W("startup.tjs")),
            [](const ttstr &name) { return name == TJS_W("System/Initialize.tjs"); },
            [&](const ttstr &name) { require(name == TJS_W("System/Initialize.tjs"), "missing-entry fallback"); ++fallbackCalls; });
        require(fallbackCalls == 1, "initializer-only package");
        caught = false;
        try { engine.ExecScript(TJS_W("totallyMissingGameMember;")); }
        catch (const eTJS &) { caught = true; }
        require(caught, "missing-member semantics changed");
        std::cout << "startup context, overrides, bytecode entry and first-error preservation passed\n";
        return 0;
    } catch (const eTJS &error) {
        std::cerr << error.GetMessage().AsStdString() << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
