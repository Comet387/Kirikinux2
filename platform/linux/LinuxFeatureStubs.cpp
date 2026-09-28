//---------------------------------------------------------------------------
// kirikinux2 : features whose sources/dependencies are not in the public tree.
// Same set Yuri's port leaves out (XP3 repack, BPG, JPEG XR); here they are
// replaced by stubs instead of editing GraphicsLoaderIntf.cpp / the menus.
//---------------------------------------------------------------------------
#include "tjsCommHead.h"
#include "GraphicsLoaderIntf.h"
#include "MsgIntf.h"
#include "Platform.h"
#include <string>

// environ/XP3ArchiveRepack.cpp + environ/ui/XP3RepackForm.cpp need the 7-Zip C++
// encoder sources that zeas2 kept in base/7zip (not published).
void TVPProcessXP3Repack(const std::string &dir) {
	TVPShowSimpleMessageBox(ttstr("XP3 repack is not available in the Linux build yet."), ttstr("Kirikiroid2"));
}

#ifndef KR2_HAVE_BPG // libbpg is not packaged by distributions
void TVPLoadBPG(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
	tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
	tTJSBinaryStream *src, tjs_int keyidx, tTVPGraphicLoadMode mode) {
	TVPThrowExceptionMessage(TJS_W("BPG images are not supported by this Linux build"));
}
void TVPLoadHeaderBPG(void* formatdata, tTJSBinaryStream *src, iTJSDispatch2** dic) {
	TVPThrowExceptionMessage(TJS_W("BPG images are not supported by this Linux build"));
}
#endif

#ifndef KR2_HAVE_JXR // visual/LoadJXR.cpp needs jxrlib (KR2_WITH_JXR)
void TVPLoadJXR(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
	tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
	tTJSBinaryStream *src, tjs_int keyidx, tTVPGraphicLoadMode mode) {
	TVPThrowExceptionMessage(TJS_W("JPEG XR images are not supported by this Linux build"));
}
void TVPLoadHeaderJXR(void* formatdata, tTJSBinaryStream *src, iTJSDispatch2** dic) {
	TVPThrowExceptionMessage(TJS_W("JPEG XR images are not supported by this Linux build"));
}
void TVPSaveAsJXR(void* formatdata, tTJSBinaryStream* dst, const iTVPBaseBitmap* image, const ttstr & mode, iTJSDispatch2* meta) {
	TVPThrowExceptionMessage(TJS_W("JPEG XR images are not supported by this Linux build"));
}
bool TVPAcceptSaveAsJXR(void* formatdata, const ttstr & type, iTJSDispatch2** dic) {
	return false;
}
#endif
