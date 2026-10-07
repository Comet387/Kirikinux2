// KAGParserEx.dll for Kirikinux2.
//
// The parser is the original KAGParserEx plugin by miahmie / wtnbgo
// (krkrz/krkr2: kirikiri2/trunk/kirikiri2/src/plugins/win32/KAGParserEx,
// KiriKiri licence), compiled from KAGParserEx_impl.inc inside a private
// namespace so it can coexist with the engine's built-in KAGParser.
// Linking the module replaces global "KAGParser" exactly as the original
// V2Link did; unlinking restores the built-in class.  (Glue: AGPL-3.0-only.)
//
// Adds: multiLineTagEnabled, "taglist" in getNextTag() results, parameter
// macros (paramMacros, @pmacro / @erasepmacro), [emb escape=false].
#include "tjsCommHead.h"
#include "tjsNative.h"
#include "tjsHashSearch.h"
#include "StorageIntf.h"
#include "tjsDictionary.h"
#include "MsgIntf.h"
#include "DebugIntf.h"
#include "ScriptMgnIntf.h"
#include "TextStream.h"
#include "tjsGlobalStringMap.h"
#include "EventIntf.h"
#include <vector>
#include "ncbind.hpp"

#define TVP_KAGPARSER_EX_PLUGIN
#define TVP_KAGPARSER_EX_CLASSNAME TJS_W("KAGParser")
#define TVP_KAGPARSER_MESSAGEMAP(name) name

// Build the class the way a tp_stub plugin would (classobj, explicit ClassID).
#undef TJS_NCM_REG_THIS
#undef TJS_NATIVE_SET_ClassID
#undef TJS_NATIVE_CLASSID_NAME
#define TJS_NCM_REG_THIS classobj
#define TJS_NATIVE_SET_ClassID TJS_NATIVE_CLASSID_NAME = TJS_NCM_CLASSID;

namespace kr2_kagparserex {
using namespace TJS;
// Message texts (same as the engine's built-in KAGParser.cpp).
const tjs_char* TVPKAGNoLine = TJS_W("Readed scenario file %1 is empty.");
const tjs_char* TVPKAGCannotOmmitFirstLabelName = TJS_W("Can not ommit first label name in scenario.");
//const tjs_char* TVPInternalError = TJS_W("内部エラーが発生しました: at %1 line %2");
const tjs_char* TVPKAGMalformedSaveData = TJS_W("Malformed savedata, data may damaged.");
const tjs_char* TVPKAGLabelNotFound = TJS_W("Label %2 not found in scenario file %1.");
const tjs_char* TVPLabelOrScriptInMacro = TJS_W("Label in macro 'iscript' is illegal.");
const tjs_char* TVPKAGInlineScriptNotEnd = TJS_W("Matched [endscript] or @endscript not found.");
const tjs_char* TVPKAGSyntaxError = TJS_W("Syntax error.'[' match to ']', \" match to \", 'macro' march to 'endmacro'. Notice space and newline.");
const tjs_char* TVPKAGCallStackUnderflow = TJS_W("'return' is not matched to any 'call' ( 'return' is unexpected )");
const tjs_char* TVPKAGReturnLostSync = TJS_W("Lost return position due to the scenario file changed.");
const tjs_char* TVPKAGSpecifyKAGParser = TJS_W("Please specify KAGParser object.");
const tjs_char* TVPUnknownMacroName = TJS_W("Unknown macro \"%1\"");
#include "KAGParserEx_impl.h"
#include "KAGParserEx_impl.inc"
} // namespace kr2_kagparserex

#define NCB_MODULE_NAME TJS_W("KAGParserEx.dll")

static iTJSDispatch2 *origKAGParser = nullptr;
static bool kagParserExLinked = false;

static void KAGParserExPostRegist() {
    using namespace kr2_kagparserex;
    if(kagParserExLinked) return;
    tTJSNI_KAGParser::initMethod();
    iTJSDispatch2 *global = TVPGetScriptDispatch();
    if(!global) return;
    tTJSVariant val;
    if(TJS_SUCCEEDED(global->PropGet(0, TVP_KAGPARSER_EX_CLASSNAME, nullptr, &val, global))) {
        origKAGParser = val.AsObject();
        val.Clear();
    }
    iTJSDispatch2 *tjsclass = tTJSNC_KAGParser::CreateNativeClass();
    val = tTJSVariant(tjsclass);
    tjsclass->Release();
    global->PropSet(TJS_MEMBERENSURE, TVP_KAGPARSER_EX_CLASSNAME, nullptr, &val, global);
    global->Release();
    kagParserExLinked = true;
}

static void KAGParserExPreUnregist() {
    using namespace kr2_kagparserex;
    if(!kagParserExLinked) return;
    iTJSDispatch2 *global = TVPGetScriptDispatch();
    if(global) {
        global->DeleteMember(0, TVP_KAGPARSER_EX_CLASSNAME, nullptr, global);
        if(origKAGParser) {
            tTJSVariant val(origKAGParser);
            origKAGParser->Release();
            origKAGParser = nullptr;
            global->PropSet(TJS_MEMBERENSURE, TVP_KAGPARSER_EX_CLASSNAME, nullptr, &val, global);
        }
        global->Release();
    }
    tTJSNI_KAGParser::doneMethod();
    kagParserExLinked = false;
}

NCB_POST_REGIST_CALLBACK(KAGParserExPostRegist);
NCB_PRE_UNREGIST_CALLBACK(KAGParserExPreUnregist);
