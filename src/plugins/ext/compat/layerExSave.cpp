// SPDX-License-Identifier: AGPL-3.0-only
// layerExSave.dll for Kirikinux2.
//
// The Windows plugin adds format-specific savers to Layer.  The engine core
// already encodes PNG / TLG5 / TLG6 / JPEG / BMP through Layer.saveLayerImage
// (src/core/visual/GraphicsLoaderIntf.cpp), so each method forwards there:
//   saveLayerImagePng(file [, compressLevel, ...])
//   saveLayerImageTlg5(file [, ...]) / saveLayerImageTlg6(file [, ...])
//   saveLayerImageJpg(file [, quality, ...])  saveLayerImageBmp(file [, ...])
// Extra arguments (compression level, quality, tag dictionaries) are accepted
// and ignored; the engine's encoders use their defaults.
#include "ncbind.hpp"

#define NCB_MODULE_NAME TJS_W("layerExSave.dll")

namespace {
tjs_error kr2SaveAs(const tjs_char *type, tTJSVariant *result, tjs_int numparams,
                    tTJSVariant **param, iTJSDispatch2 *objthis) {
    if(numparams < 1) return TJS_E_BADPARAMCOUNT;
    if(!objthis) return TJS_E_NATIVECLASSCRASH;
    tTJSVariant name = *param[0];
    tTJSVariant mode(type);
    tTJSVariant *args[] = { &name, &mode };
    tjs_error hr = objthis->FuncCall(0, TJS_W("saveLayerImage"), nullptr, nullptr, 2, args, objthis);
    if(result) *result = TJS_SUCCEEDED(hr) ? tjs_int(1) : tjs_int(0);
    return hr;
}
} // namespace

struct LayerExSaveCompat {
#define KR2_SAVE_METHOD(fn, type)                                                        \
    static tjs_error TJS_INTF_METHOD fn(tTJSVariant *r, tjs_int n, tTJSVariant **p,     \
                                        iTJSDispatch2 *o) {                              \
        return kr2SaveAs(TJS_W(type), r, n, p, o);                                       \
    }
    KR2_SAVE_METHOD(savePng, "png")
    KR2_SAVE_METHOD(saveTlg5, "tlg5")
    KR2_SAVE_METHOD(saveTlg6, "tlg6")
    KR2_SAVE_METHOD(saveJpg, "jpg")
    KR2_SAVE_METHOD(saveBmp, "bmp")
#undef KR2_SAVE_METHOD
};

NCB_ATTACH_CLASS(LayerExSaveCompat, Layer) {
    RawCallback("saveLayerImagePng", &LayerExSaveCompat::savePng, 0);
    RawCallback("saveLayerImageTlg5", &LayerExSaveCompat::saveTlg5, 0);
    RawCallback("saveLayerImageTlg6", &LayerExSaveCompat::saveTlg6, 0);
    RawCallback("saveLayerImageJpg", &LayerExSaveCompat::saveJpg, 0);
    RawCallback("saveLayerImageBmp", &LayerExSaveCompat::saveBmp, 0);
}
