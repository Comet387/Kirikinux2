// SPDX-License-Identifier: AGPL-3.0-only
// win32ole.dll for Kirikinux2.
//
// COM/OLE automation does not exist on Linux.  Games use WIN32OLE for
// optional features (Yuzusoft: SAPI text-to-speech in sysscn/speech_ole.tjs)
// and wrap creation in try/catch, which is exactly how a Windows PC without
// the requested COM server behaves.  So the class exists (typeof checks and
// Plugins.link succeed) but construction throws a clear exception.
#include "ncbind.hpp"
#include "MsgIntf.h"

#define NCB_MODULE_NAME TJS_W("win32ole.dll")

struct WIN32OLE {
    static tjs_error TJS_INTF_METHOD factory(WIN32OLE **, tjs_int numparams, tTJSVariant **param,
                                             iTJSDispatch2 *) {
        ttstr name = numparams > 0 ? ttstr(*param[0]) : ttstr(TJS_W("(none)"));
        TVPThrowExceptionMessage(
            (ttstr(TJS_W("WIN32OLE: COM object \"")) + name +
             TJS_W("\" is not available on this platform")).c_str());
        return TJS_E_FAIL;
    }
};

NCB_REGISTER_CLASS(WIN32OLE) {
    Factory(&WIN32OLE::factory);
}
