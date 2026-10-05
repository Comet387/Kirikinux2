// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "tjs.h"
#include "tjsArray.h"
#include "tjsDictionary.h"
#include "tjsNative.h"

#if defined(LINUX) || defined(__linux__)
// The Android build ships motionplayer/emoteplayer as private native plugins.
// Linux intentionally does not claim to implement their renderer, but a large
// set of Android game patches probes Motion before deciding whether to disable
// the D3D path.  A missing top-level object is an exception in TJS2, so expose
// a small, explicit no-op compatibility surface.  This keeps the probe and
// fallback assignments safe without pretending that animation/D3D is present.
inline tjs_error TJS_INTF_METHOD TVPLinuxPluginCompatNoop(
    TJS::tTJSVariant *result, tjs_int, TJS::tTJSVariant **,
    TJS::iTJSDispatch2 *) {
    if (result) result->Clear();
    return TJS_S_OK;
}

// motion.tjs constructs this object with `new Motion.ResourceManager(size)`.
// A dictionary is enough for property probes, but TJS requires a native class
// object for `new`; otherwise the VM reports "Not a function or invalid
// method/property type" before the script can select its no-D3D path.
inline TJS::iTJSNativeInstance *TJS_INTF_METHOD
TVPLinuxPluginCompatCreateNativeInstance() {
    return new TJS::tTJSNativeInstance();
}

inline TJS::iTJSDispatch2 *TVPCreateLinuxResourceManagerClass() {
    using namespace TJS;
    tTJSNativeClassForPlugin *klass = TJSCreateNativeClassForPlugin(
        TJS_W("ResourceManager"), TVPLinuxPluginCompatCreateNativeInstance);

    // The constructor and lifecycle methods intentionally do no work on Linux:
    // the Android Motion/E-mote renderer is not present, but the script still
    // needs an object whose lifetime and cache calls are well-defined.
    TJSNativeClassRegisterNCM(
        klass, TJS_W("ResourceManager"),
        TJSCreateNativeClassConstructor(TVPLinuxPluginCompatNoop),
        TJS_W("ResourceManager"), nitMethod);
    const tjs_char *methods[] = {
        TJS_W("addRef"), TJS_W("release"), TJS_W("finalize"),
        TJS_W("load"), TJS_W("unload"), TJS_W("clearCache"),
        TJS_W("setEmotePSBDecryptFunc"), TJS_W("setEmotePSBDecryptSeed"),
    };
    for (const tjs_char *name : methods) {
        TJSNativeClassRegisterNCM(
            klass, name, TJSCreateNativeClassMethod(TVPLinuxPluginCompatNoop),
            TJS_W("ResourceManager"), nitMethod);
    }
    return klass;
}

inline TJS::iTJSDispatch2 *TVPEnsureLinuxCompatObject(
    TJS::iTJSDispatch2 *parent, const tjs_char *name) {
    using namespace TJS;
    tTJSVariant current;
    const tjs_error status = parent->PropGet(
        TJS_MEMBERMUSTEXIST | TJS_IGNOREPROP, name, nullptr, &current, parent);
    if (status == TJS_S_OK && current.Type() == tvtObject) {
        iTJSDispatch2 *object = current.AsObjectNoAddRef();
        if (object) object->AddRef();
        return object;
    }
    iTJSDispatch2 *object = TJSCreateDictionaryObject();
    tTJSVariant value(object, object);
    const tjs_error set_status = parent->PropSet(
        TJS_MEMBERENSURE | TJS_IGNOREPROP, name, nullptr, &value, parent);
    if (TJS_FAILED(set_status)) {
        object->Release();
        TJS_eTJSError(ttstr(TJS_W("cannot register Linux plugin compatibility object")));
    }
    return object;
}

inline void TVPInstallLinuxCompatMethod(
    TJS::iTJSDispatch2 *object, const tjs_char *name) {
    using namespace TJS;
    tTJSVariant current;
    if (object->PropGet(TJS_MEMBERMUSTEXIST | TJS_IGNOREPROP,
                        name, nullptr, &current, object) == TJS_S_OK) return;
    iTJSDispatch2 *method = TJSCreateNativeClassMethod(TVPLinuxPluginCompatNoop);
    tTJSVariant value(method, method);
    method->Release();
    const tjs_error status = object->PropSet(
        TJS_MEMBERENSURE | TJS_IGNOREPROP, name, nullptr, &value, object);
    if (TJS_FAILED(status))
        TJS_eTJSError(ttstr(TJS_W("cannot register Linux plugin compatibility method")));
}

inline void TVPInstallLinuxPluginCompatibility(TJS::tTJS &engine) {
    using namespace TJS;
    iTJSDispatch2 *global = engine.GetGlobalNoAddRef();
    iTJSDispatch2 *motion = TVPEnsureLinuxCompatObject(global, TJS_W("Motion"));
    iTJSDispatch2 *player = TVPEnsureLinuxCompatObject(motion, TJS_W("Player"));
    iTJSDispatch2 *emote = TVPEnsureLinuxCompatObject(motion, TJS_W("EmotePlayer"));
    tTJSVariant existing_resources;
    iTJSDispatch2 *resources = nullptr;
    if (motion->PropGet(TJS_MEMBERMUSTEXIST | TJS_IGNOREPROP,
                        TJS_W("ResourceManager"), nullptr,
                        &existing_resources, motion) == TJS_S_OK &&
        existing_resources.Type() == tvtObject &&
        existing_resources.AsObjectNoAddRef() != nullptr) {
        resources = existing_resources.AsObjectNoAddRef();
        resources->AddRef();
    } else {
        resources = TVPCreateLinuxResourceManagerClass();
        tTJSVariant value(resources, resources);
        const tjs_error status = motion->PropSet(
            TJS_MEMBERENSURE | TJS_IGNOREPROP, TJS_W("ResourceManager"),
            nullptr, &value, motion);
        if (TJS_FAILED(status)) {
            resources->Release();
            TJS_eTJSError(ttstr(TJS_W(
                "cannot register Linux ResourceManager compatibility class")));
        }
    }

    tTJSVariant zero(static_cast<tjs_int>(0));
    player->PropSet(TJS_MEMBERENSURE | TJS_IGNOREPROP, TJS_W("useD3D"), nullptr, &zero, player);
    emote->PropSet(TJS_MEMBERENSURE | TJS_IGNOREPROP, TJS_W("useD3D"), nullptr, &zero, emote);
    tTJSVariant keys;
    iTJSDispatch2 *key_array = TJSCreateArrayObject();
    keys.SetObject(key_array, key_array);
    key_array->Release();
    player->PropSet(TJS_MEMBERENSURE | TJS_IGNOREPROP, TJS_W("varibleKeys"), nullptr, &keys, player);

    TVPInstallLinuxCompatMethod(resources, TJS_W("setEmotePSBDecryptFunc"));
    TVPInstallLinuxCompatMethod(resources, TJS_W("setEmotePSBDecryptSeed"));
    TVPInstallLinuxCompatMethod(emote, TJS_W("setCameraCoord"));
    TVPInstallLinuxCompatMethod(emote, TJS_W("setCameraRotate"));
    TVPInstallLinuxCompatMethod(emote, TJS_W("setCameraScale"));

    resources->Release();
    emote->Release();
    player->Release();
    motion->Release();
}
#endif

// KAGEX bootstrap context used by the upstream Kirikiroid2 patch library.
// These are writable script defaults, not native engine/plugin capabilities.
// Derive PACKED from resolved storage. Preserve game/configuration overrides.
inline void TVPInitializeKAGStartupContext(TJS::tTJS &engine, bool packed)
{
    using namespace TJS;
    struct Default { const tjs_char *name; tjs_int value; };
    const Default defaults[] = {
        { TJS_W("kirikiriz"), 0 },
        { TJS_W("debugWindowEnabled"), 0 },
        { TJS_W("inXP3archivePacked"), packed ? 1 : 0 },
        { TJS_W("convertMode"), 0 },
    };
    iTJSDispatch2 *global = engine.GetGlobalNoAddRef();
    for (const Default &setting : defaults) {
        tTJSVariant existing;
        const tjs_error status = global->PropGet(TJS_MEMBERMUSTEXIST | TJS_IGNOREPROP,
            setting.name, NULL, &existing, global);
        if (status != TJS_E_MEMBERNOTFOUND) continue;
        tTJSVariant value(setting.value);
        global->PropSet(TJS_MEMBERENSURE | TJS_IGNOREPROP,
            setting.name, NULL, &value, global);
    }
#if defined(LINUX) || defined(__linux__)
    TVPInstallLinuxPluginCompatibility(engine);
#endif
}

// Only a missing startup may use an initializer-only package. A present startup
// must finish: decoding/plugin/script exceptions propagate with their original
// trace. ExecuteStorage selects bytecode or text; do not text-probe beforehand.
template <typename Exists, typename Execute>
inline void TVPExecuteStartupEntry(const TJS::ttstr &startup, Exists exists, Execute execute)
{
    const TJS::ttstr initializer(TJS_W("System/Initialize.tjs"));
    if (!exists(startup) && exists(initializer)) execute(initializer);
    else execute(startup);
}
