// SPDX-License-Identifier: AGPL-3.0-only
// Kirikinux2: games (e.g. Yuzusoft titles) link "motionplayer_nod3d.dll" when
// started with -nod3dm.  Kirikiroid2 has a single software Motion player, so
// this module name is an alias that registers motionplayer.dll.
#include "ncbind.hpp"

#define NCB_MODULE_NAME TJS_W("motionplayer_nod3d.dll")

static void MotionPlayerNoD3DPreRegist() {
    ncbAutoRegister::LoadModule(TJS_W("motionplayer.dll"));
}

NCB_PRE_REGIST_CALLBACK(MotionPlayerNoD3DPreRegist);
