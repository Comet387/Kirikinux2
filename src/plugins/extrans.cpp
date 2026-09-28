#include "ncbind/ncbind.hpp"
#include "extrans/wave.h"
#include "extrans/mosaic.h"
#include "extrans/turn.h"
#include "extrans/rotatetrans.h"
#include "extrans/ripple.h"

#define NCB_MODULE_NAME TJS_W("extrans.dll")
static void RegisterExTrans() {
    RegisterWaveTransHandlerProvider();
    RegisterMosaicTransHandlerProvider();
    RegisterTurnTransHandlerProvider();
    RegisterRotateTransHandlerProvider();
    RegisterRippleTransHandlerProvider();
}
static void UnregisterExTrans() {
    UnregisterRippleTransHandlerProvider();
    UnregisterRotateTransHandlerProvider();
    UnregisterTurnTransHandlerProvider();
    UnregisterMosaicTransHandlerProvider();
    UnregisterWaveTransHandlerProvider();
}
NCB_PRE_REGIST_CALLBACK(RegisterExTrans);
NCB_POST_UNREGIST_CALLBACK(UnregisterExTrans);
