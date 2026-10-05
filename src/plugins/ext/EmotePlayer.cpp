#include "ncbind.hpp"

#define NCB_MODULE_NAME TJS_W("emoteplayer.dll")

static void EmotePlayerPreRegist() {
    // Load motionplayer.dll as dependency (matches libkrkr2.so sub_682528)
    ncbAutoRegister::LoadModule(TJS_W("motionplayer.dll"));
}

NCB_PRE_REGIST_CALLBACK(EmotePlayerPreRegist);
