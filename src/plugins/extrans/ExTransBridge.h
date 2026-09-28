#pragma once
// Adapt the upstream scanline API to Kirikiroid2's existing texture providers.
// Pixel algorithms remain the upstream extrans implementations.
#include "tp_stub.h"
#include "RenderManager.h"
#include "tvpgl.h"
#include "DetectCPU.h"
#include "cpu_types.h"

class TVPExTransScanLines {
    iTVPScanLineProvider *provider;
public:
    explicit TVPExTransScanLines(iTVPScanLineProvider *p) : provider(p) {}
    tjs_error GetPitchBytes(tjs_int *pitch) {
        if (!pitch) return TJS_E_INVALIDPARAM;
        *pitch = provider->GetTexture()->GetPitch();
        return *pitch ? TJS_S_OK : TJS_E_NOTIMPL;
    }
    tjs_error GetScanLine(tjs_int line, const void **out) {
        if (!out) return TJS_E_INVALIDPARAM;
        auto *texture = provider->GetTexture();
        if (line < 0 || line >= static_cast<tjs_int>(texture->GetHeight())) return TJS_E_INVALIDPARAM;
        *out = texture->GetScanLineForRead(line);
        return *out ? TJS_S_OK : TJS_E_NOTIMPL;
    }
    tjs_error GetScanLineForWrite(tjs_int line, void **out) {
        if (!out) return TJS_E_INVALIDPARAM;
        auto *texture = provider->GetTextureForRender();
        if (line < 0 || line >= static_cast<tjs_int>(texture->GetHeight())) return TJS_E_INVALIDPARAM;
        *out = texture->GetScanLineForWrite(line);
        return *out ? TJS_S_OK : TJS_E_NOTIMPL;
    }
};
