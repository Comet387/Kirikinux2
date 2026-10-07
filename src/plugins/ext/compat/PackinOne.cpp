// SPDX-License-Identifier: AGPL-3.0-only
// PackinOne.dll for Kirikinux2.
//
// KAGEX's PackinOne.dll is a bundle of small plugins in one DLL.  Scripts
// test for its pieces (e.g. `typeof Array.save2`) and Initialize.tjs falls
// back to fstat.dll when it is missing, so here the module name simply loads
// the equivalent internal modules.
#include "ncbind.hpp"

#define NCB_MODULE_NAME TJS_W("PackinOne.dll")

static void PackinOnePreRegist() {
    static const tjs_char *modules[] = {
        TJS_W("saveStruct.dll"),  // Array.save2 / saveStruct2, Dictionary.saveStruct2
        TJS_W("varfile.dll"),
        TJS_W("fstat.dll"),       // Storages.fstat, dirlist, file times
        TJS_W("scriptsEx.dll"),
        TJS_W("csvParser.dll"),
        TJS_W("dirlist.dll"),
        TJS_W("layerExSave.dll"), // Layer.saveLayerImagePng etc.
    };
    for(const tjs_char *m : modules) ncbAutoRegister::LoadModule(m);
}

NCB_PRE_REGIST_CALLBACK(PackinOnePreRegist);
