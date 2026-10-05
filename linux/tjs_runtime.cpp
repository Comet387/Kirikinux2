#include "tjs_runtime.h"

#include "storage_root.h"

#include "tjs.h"
#include "tjsArray.h"
#include "tjsDictionary.h"
#include "tjsError.h"
#include "tjsNative.h"
#include "StartupCompatibility.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace krkr2 {
namespace {

using namespace TJS;

std::string narrow(const ttstr &text) {
  return text.AsStdString();
}

void append_utf8(std::string &out, std::uint32_t value) {
  if (value <= 0x7f) {
    out.push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    out.push_back(static_cast<char>(0xc0 | (value >> 6)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else if (value <= 0xffff) {
    out.push_back(static_cast<char>(0xe0 | (value >> 12)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    out.push_back(static_cast<char>(0xf0 | (value >> 18)));
    out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
}

bool decode_utf16(const std::vector<std::uint8_t> &bytes, bool big_endian,
                  std::string &text) {
  if ((bytes.size() - 2) % 2 != 0) return false;
  text.clear();
  for (std::size_t offset = 2; offset < bytes.size(); offset += 2) {
    std::uint32_t value = big_endian
        ? (static_cast<std::uint32_t>(bytes[offset]) << 8) | bytes[offset + 1]
        : static_cast<std::uint32_t>(bytes[offset]) |
              (static_cast<std::uint32_t>(bytes[offset + 1]) << 8);
    if (value >= 0xd800 && value <= 0xdbff) {
      offset += 2;
      if (offset >= bytes.size()) return false;
      const std::uint32_t low = big_endian
          ? (static_cast<std::uint32_t>(bytes[offset]) << 8) | bytes[offset + 1]
          : static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8);
      if (low < 0xdc00 || low > 0xdfff) return false;
      value = 0x10000 + ((value - 0xd800) << 10) + (low - 0xdc00);
    } else if (value >= 0xdc00 && value <= 0xdfff) {
      return false;
    }
    append_utf8(text, value);
  }
  return true;
}

bool decode_source(const std::vector<std::uint8_t> &bytes, std::string &text) {
  if (bytes.empty()) {
    text.clear();
    return true;
  }
  if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe)
    return decode_utf16(bytes, false, text);
  if (bytes.size() >= 2 && bytes[0] == 0xfe && bytes[1] == 0xff)
    return decode_utf16(bytes, true, text);
  std::size_t offset = bytes.size() >= 3 && bytes[0] == 0xef &&
      bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
  text.assign(reinterpret_cast<const char *>(bytes.data() + offset),
              bytes.size() - offset);
  return text.find('\0') == std::string::npos;
}

struct ActiveStorageRuntime {
  tTJS *engine = nullptr;
  StorageRoot *storage = nullptr;
  std::vector<std::string> auto_paths;
};

thread_local ActiveStorageRuntime *active_storage_runtime = nullptr;

class ActiveStorageGuard {
 public:
  explicit ActiveStorageGuard(ActiveStorageRuntime *runtime)
      : previous_(active_storage_runtime) {
    active_storage_runtime = runtime;
  }
  ~ActiveStorageGuard() { active_storage_runtime = previous_; }

 private:
  ActiveStorageRuntime *previous_;
};

void throw_storage_error(const std::string &message) {
  TJS_eTJSError(ttstr(message));
}

bool resolve_storage_entry(const std::string &requested, std::string &resolved) {
  if (!active_storage_runtime || !active_storage_runtime->storage) return false;
  if (active_storage_runtime->storage->exists(requested)) {
    resolved = requested;
    return true;
  }
  for (auto path = active_storage_runtime->auto_paths.rbegin();
       path != active_storage_runtime->auto_paths.rend(); ++path) {
    // A full "archive.xp3>" auto path refers to another storage root.  The
    // current host does not open sibling archives yet, but internal prefixes
    // (the normal KAG system/, scenario/, image/ entries) are supported.
    if (path->find('>') != std::string::npos) continue;
    std::string candidate = *path;
    if (!candidate.empty() && candidate.back() != '/' &&
        candidate.back() != '\\') candidate.push_back('/');
    candidate += requested;
    if (active_storage_runtime->storage->exists(candidate)) {
      resolved = std::move(candidate);
      return true;
    }
  }
  return false;
}

std::string variant_string(tTJSVariant *value) {
  return ttstr(*value).AsStdString();
}

// ScriptsEx exposes this as a static Scripts.foreach(obj, callback, args*).
// Senren*Banka's KAG bytecode uses it during environment construction.  Keep
// the implementation in the Linux host instead of requiring the optional
// Windows DLL, and preserve the plugin's callback contract: (key, value,
// args...), with a non-void callback result stopping the traversal.
class ScriptsForeachCaller : public tTJSDispatch {
 public:
  ScriptsForeachCaller(iTJSDispatch2 *function, iTJSDispatch2 *function_this,
                       tTJSVariant **arguments, tjs_int argument_count)
      : function_(function), function_this_(function_this),
        arguments_(arguments), argument_count_(argument_count) {}

  tjs_error TJS_INTF_METHOD FuncCall(
      tjs_uint32, const tjs_char *, tjs_uint32 *, tTJSVariant *result,
      tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *) override {
    break_result_.Clear();
    // EnumMembers passes name, member flags and value.  Hidden members are
    // intentionally skipped, matching the ScriptsEx plugin.
    if (numparams > 1 && static_cast<tjs_int>(*param[1]) != TJS_HIDDENMEMBER) {
      arguments_[0] = param[0];
      arguments_[1] = param[2];
      function_->FuncCall(0, nullptr, nullptr, &break_result_, argument_count_,
                          arguments_, function_this_);
    }
    if (result) *result = break_result_.Type() == tvtVoid;
    return TJS_S_OK;
  }

  const tTJSVariant &break_result() const { return break_result_; }

 private:
  iTJSDispatch2 *function_;
  iTJSDispatch2 *function_this_;
  tTJSVariant **arguments_;
  tjs_int argument_count_;
  tTJSVariant break_result_;
};

tjs_error TJS_INTF_METHOD scripts_foreach(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *objthis) {
  if (numparams < 2 || param[0]->Type() != tvtObject ||
      param[1]->Type() != tvtObject) {
    return TJS_E_BADPARAMCOUNT;
  }

  const tTJSVariantClosure object = param[0]->AsObjectClosureNoAddRef();
  const tTJSVariantClosure callback = param[1]->AsObjectClosureNoAddRef();
  if (!object.Object || !callback.Object) return TJS_E_INVALIDPARAM;
  iTJSDispatch2 *callback_this = callback.ObjThis ? callback.ObjThis : objthis;

  std::vector<tTJSVariant *> arguments(static_cast<std::size_t>(numparams));
  tTJSVariant key;
  tTJSVariant value;
  arguments[0] = &key;
  arguments[1] = &value;
  for (tjs_int index = 2; index < numparams; ++index)
    arguments[static_cast<std::size_t>(index)] = param[index];

  tTJSVariant break_result;
  if (object.IsInstanceOf(0, nullptr, nullptr, TJS_W("Array"), nullptr) ==
      TJS_S_TRUE) {
    tTJSVariant count;
    object.PropGet(0, TJS_W("count"), nullptr, &count, nullptr);
    const tjs_int length = static_cast<tjs_int>(count);
    for (tjs_int index = 0; index < length; ++index) {
      key = index;
      value.Clear();
      object.PropGetByNum(TJS_IGNOREPROP, index, &value, nullptr);
      break_result.Clear();
      callback.Object->FuncCall(0, nullptr, nullptr, &break_result, numparams,
                                arguments.data(), callback_this);
      if (break_result.Type() != tvtVoid) break;
    }
  } else {
    ScriptsForeachCaller caller(callback.Object, callback_this, arguments.data(),
                                numparams);
    tTJSVariantClosure closure(&caller);
    object.EnumMembers(TJS_IGNOREPROP, &closure, nullptr);
    break_result = caller.break_result();
  }
  if (result) *result = break_result;
  return TJS_S_OK;
}

void execute_storage_entry(const std::string &entry, bool expression,
                           tTJSVariant *result, iTJSDispatch2 *context) {
  if (!active_storage_runtime || !active_storage_runtime->engine ||
      !active_storage_runtime->storage) {
    throw_storage_error("no active Linux storage root");
    return;
  }
  std::vector<std::uint8_t> source;
  std::string error;
  std::string resolved;
  if (!resolve_storage_entry(entry, resolved)) {
    throw_storage_error("storage not found: " + entry);
    return;
  }
  if (!active_storage_runtime->storage->read(resolved, source, error)) {
    throw_storage_error(error);
    return;
  }
  // KAG distributions normally store compiled TJS bytecode.  Probe the
  // bytecode header before decoding as text so the diagnostic host follows
  // the production storage runner and preserves the original script name in
  // any exception trace.
  if (source.size() >= 8 && std::equal(source.begin(), source.begin() + 7,
                                       std::begin("TJS2100"))) {
    const ttstr source_name(active_storage_runtime->storage->placed_path(resolved));
    active_storage_runtime->engine->LoadByteCode(
        source.data(), source.size(), result, context, source_name.c_str());
    return;
  }
  std::string decoded;
  if (!decode_source(source, decoded)) {
    throw_storage_error("invalid UTF-16 source or embedded NUL byte: " + entry);
    return;
  }
  std::string placed = active_storage_runtime->storage->placed_path(resolved);
  if (placed.empty()) placed = resolved;
  const ttstr source_name(placed);
  if (expression) {
    active_storage_runtime->engine->EvalExpression(
        ttstr(decoded), result, context, &source_name);
  } else {
    active_storage_runtime->engine->ExecScript(
        ttstr(decoded), result, context, &source_name);
  }
}

tjs_error TJS_INTF_METHOD scripts_exec_storage(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  iTJSDispatch2 *context = numparams >= 3 && param[2]->Type() != tvtVoid
      ? param[2]->AsObjectNoAddRef() : nullptr;
  execute_storage_entry(variant_string(param[0]), false, result, context);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD scripts_eval_storage(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  iTJSDispatch2 *context = numparams >= 3 && param[2]->Type() != tvtVoid
      ? param[2]->AsObjectNoAddRef() : nullptr;
  execute_storage_entry(variant_string(param[0]), true, result, context);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD scripts_exec(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  if (!active_storage_runtime || !active_storage_runtime->engine)
    return TJS_E_NATIVECLASSCRASH;
  const ttstr content = *param[0];
  const ttstr name = numparams >= 2 && param[1]->Type() != tvtVoid
      ? ttstr(*param[1]) : ttstr(TJS_W("script string"));
  const tjs_int line_offset = numparams >= 3 && param[2]->Type() != tvtVoid
      ? static_cast<tjs_int>(*param[2]) : 0;
  iTJSDispatch2 *context = numparams >= 4 && param[3]->Type() != tvtVoid
      ? param[3]->AsObjectNoAddRef() : nullptr;
  active_storage_runtime->engine->ExecScript(
      content, result, context, &name, line_offset);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD scripts_eval(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  if (!active_storage_runtime || !active_storage_runtime->engine)
    return TJS_E_NATIVECLASSCRASH;
  const ttstr content = *param[0];
  const ttstr name = numparams >= 2 && param[1]->Type() != tvtVoid
      ? ttstr(*param[1]) : ttstr(TJS_W("script expression"));
  const tjs_int line_offset = numparams >= 3 && param[2]->Type() != tvtVoid
      ? static_cast<tjs_int>(*param[2]) : 0;
  iTJSDispatch2 *context = numparams >= 4 && param[3]->Type() != tvtVoid
      ? param[3]->AsObjectNoAddRef() : nullptr;
  active_storage_runtime->engine->EvalExpression(
      content, result, context, &name, line_offset);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_exists(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  if (!active_storage_runtime || !active_storage_runtime->storage)
    return TJS_E_NATIVECLASSCRASH;
  if (result) {
    std::string resolved;
    *result = static_cast<tjs_int>(resolve_storage_entry(
        variant_string(param[0]), resolved));
  }
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_placed_path(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  if (!active_storage_runtime || !active_storage_runtime->storage)
    return TJS_E_NATIVECLASSCRASH;
  if (result) {
    std::string resolved;
    if (resolve_storage_entry(variant_string(param[0]), resolved))
      *result = ttstr(active_storage_runtime->storage->placed_path(resolved));
    else
      *result = ttstr(TJS_W(""));
  }
  return TJS_S_OK;
}

std::size_t storage_delimiter(const std::string &path) {
  return path.find_last_of("/\\>");
}

tjs_error TJS_INTF_METHOD storages_extract_name(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  const std::string path = variant_string(param[0]);
  const std::size_t split = storage_delimiter(path);
  if (result) *result = ttstr(split == std::string::npos ? path : path.substr(split + 1));
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_extract_path(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  const std::string path = variant_string(param[0]);
  const std::size_t split = storage_delimiter(path);
  if (result) *result = ttstr(split == std::string::npos ? std::string() :
                              path.substr(0, split + 1));
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_extract_ext(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  const std::string path = variant_string(param[0]);
  const std::size_t split = storage_delimiter(path);
  const std::size_t dot = path.find_last_of('.');
  const std::string extension = dot == std::string::npos ||
      (split != std::string::npos && dot < split) ? std::string() : path.substr(dot + 1);
  if (result) *result = ttstr(extension);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_chop_ext(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  std::string path = variant_string(param[0]);
  const std::size_t split = storage_delimiter(path);
  const std::size_t dot = path.find_last_of('.');
  if (dot != std::string::npos && (split == std::string::npos || dot > split))
    path.erase(dot);
  if (result) *result = ttstr(path);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storage_noop(
    tTJSVariant *result, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
  if (result) result->Clear();
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_add_auto_path(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  if (!active_storage_runtime) return TJS_E_NATIVECLASSCRASH;
  const std::string path = variant_string(param[0]);
  auto &paths = active_storage_runtime->auto_paths;
  paths.erase(std::remove(paths.begin(), paths.end(), path), paths.end());
  paths.push_back(path);
  if (result) result->Clear();
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD storages_remove_auto_path(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  if (!active_storage_runtime) return TJS_E_NATIVECLASSCRASH;
  const std::string path = variant_string(param[0]);
  auto &paths = active_storage_runtime->auto_paths;
  paths.erase(std::remove(paths.begin(), paths.end(), path), paths.end());
  if (result) result->Clear();
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD debug_message(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  for (tjs_int index = 0; index < numparams; ++index) {
    if (index) std::clog << ' ';
    std::clog << variant_string(param[index]);
  }
  std::clog << '\n';
  if (result) result->Clear();
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD debug_get_tick_count(
    tTJSVariant *result, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  if (result) *result = static_cast<tTVInteger>(milliseconds);
  return TJS_S_OK;
}

std::string ascii_lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

tjs_error TJS_INTF_METHOD plugins_link(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  const std::string name = ascii_lower_copy(variant_string(param[0]));
  // motionplayer.dll is an optional Windows plugin.  The Linux startup
  // compatibility namespace supplies its Motion objects, so linking the
  // absent DLL must be a successful no-op rather than aborting startup.
  if (name != "layereximage.dll" && name != "motionplayer.dll" &&
      name != "motionplayer_nod3d.dll") {
    throw_storage_error("unsupported Windows plugin in Linux build: " + name);
    return TJS_E_FAIL;
  }
  if (result) *result = static_cast<tjs_int>(1);
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD plugins_unlink(
    tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
    iTJSDispatch2 *) {
  if (numparams < 1) return TJS_E_BADPARAMCOUNT;
  const std::string name = ascii_lower_copy(variant_string(param[0]));
  if (result) {
    *result = static_cast<tjs_int>(
        name == "layereximage.dll" || name == "motionplayer.dll" ||
        name == "motionplayer_nod3d.dll");
  }
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD plugins_get_list(
    tTJSVariant *result, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
  if (!result) return TJS_S_OK;
  iTJSDispatch2 *array = TJSCreateArrayObject();
  const tjs_char *names[] = {
      TJS_W("layerExImage.dll"), TJS_W("motionplayer.dll")};
  for (tjs_int index = 0; index < 2; ++index) {
    tTJSVariant item(names[index]);
    const tjs_error status = array->PropSetByNum(
        TJS_MEMBERENSURE | TJS_IGNOREPROP, index, &item, array);
    if (TJS_FAILED(status)) {
      array->Release();
      return status;
    }
  }
  result->SetObject(array, array);
  array->Release();
  return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD system_create_app_lock(
    tTJSVariant *result, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
  // Yuri's mobile host treats the single-instance lock as already acquired.
  if (result) *result = static_cast<tjs_int>(1);
  return TJS_S_OK;
}

void set_method(iTJSDispatch2 *object, const tjs_char *name,
                tTJSNativeClassMethodCallback callback) {
  iTJSDispatch2 *method = TJSCreateNativeClassMethod(callback);
  tTJSVariant value(method, method);
  method->Release();
  const tjs_error status = object->PropSet(
      TJS_MEMBERENSURE | TJS_IGNOREPROP, name, nullptr, &value, object);
  if (TJS_FAILED(status)) throw_storage_error("cannot register Linux TJS method");
}

void set_global_object(tTJS *engine, const tjs_char *name,
                       iTJSDispatch2 *object) {
  tTJSVariant value(object, object);
  const tjs_error status = engine->GetGlobalNoAddRef()->PropSet(
      TJS_MEMBERENSURE | TJS_IGNOREPROP, name, nullptr, &value,
      engine->GetGlobalNoAddRef());
  if (TJS_FAILED(status)) throw_storage_error("cannot register Linux TJS object");
}

void set_global_property(tTJS *engine, const tjs_char *name,
                         const tTJSVariant &value) {
  const tjs_error status = engine->GetGlobalNoAddRef()->PropSet(
      TJS_MEMBERENSURE | TJS_IGNOREPROP, name, nullptr, &value,
      engine->GetGlobalNoAddRef());
  if (TJS_FAILED(status)) throw_storage_error("cannot register Linux TJS constant");
}

void set_property(iTJSDispatch2 *object, const tjs_char *name,
                  const tTJSVariant &value) {
  const tjs_error status = object->PropSet(
      TJS_MEMBERENSURE | TJS_IGNOREPROP, name, nullptr, &value, object);
  if (TJS_FAILED(status)) throw_storage_error("cannot register Linux TJS property");
}

void install_tvp_script_constants(tTJS *engine) {
  // The full player installs TVPInitTJSScript before startup.  The small
  // Linux host does not link the GUI platform, but KAG bytecode still reads
  // these layer/blend/cursor constants while creating its environment.  Keep
  // the values identical to ScriptMgnIntf.cpp so a game cannot fail merely
  // because it is running through the platform-neutral bootstrap.
  struct Constant {
    const tjs_char *name;
    tjs_int value;
  };
  static const Constant constants[] = {
      {TJS_W("bsNone"), 0}, {TJS_W("bsSingle"), 1},
      {TJS_W("bsSizeable"), 2}, {TJS_W("bsDialog"), 3},
      {TJS_W("utNormal"), 0}, {TJS_W("utEntire"), 1},
      {TJS_W("mbLeft"), 0}, {TJS_W("mbRight"), 1},
      {TJS_W("mbMiddle"), 2}, {TJS_W("mbX1"), 3}, {TJS_W("mbX2"), 4},
      {TJS_W("mcsVisible"), 0}, {TJS_W("mcsTempHidden"), 1},
      {TJS_W("mcsHidden"), 2}, {TJS_W("imDisable"), 0},
      {TJS_W("imClose"), 1}, {TJS_W("imOpen"), 2},
      {TJS_W("ssShift"), 1}, {TJS_W("ssAlt"), 2},
      {TJS_W("ssCtrl"), 4}, {TJS_W("ssLeft"), 8},
      {TJS_W("ssRight"), 16}, {TJS_W("ssMiddle"), 32},
      {TJS_W("ssDouble"), 64}, {TJS_W("ssRepeat"), 128},
      {TJS_W("ltBinder"), 0}, {TJS_W("ltCoverRect"), 1},
      {TJS_W("ltOpaque"), 1}, {TJS_W("ltTransparent"), 2},
      {TJS_W("ltAlpha"), 2}, {TJS_W("ltAdditive"), 3},
      {TJS_W("ltSubtractive"), 4}, {TJS_W("ltMultiplicative"), 5},
      {TJS_W("ltEffect"), 6}, {TJS_W("ltFilter"), 7},
      {TJS_W("ltDodge"), 8}, {TJS_W("ltDarken"), 9},
      {TJS_W("ltLighten"), 10}, {TJS_W("ltScreen"), 11},
      {TJS_W("ltAddAlpha"), 12}, {TJS_W("ltPsNormal"), 13},
      {TJS_W("ltPsAdditive"), 14}, {TJS_W("ltPsSubtractive"), 15},
      {TJS_W("ltPsMultiplicative"), 16}, {TJS_W("ltPsScreen"), 17},
      {TJS_W("ltPsOverlay"), 18}, {TJS_W("ltPsHardLight"), 19},
      {TJS_W("ltPsSoftLight"), 20}, {TJS_W("ltPsColorDodge"), 21},
      {TJS_W("ltPsColorDodge5"), 22}, {TJS_W("ltPsColorBurn"), 23},
      {TJS_W("ltPsLighten"), 24}, {TJS_W("ltPsDarken"), 25},
      {TJS_W("ltPsDifference"), 26}, {TJS_W("ltPsDifference5"), 27},
      {TJS_W("ltPsExclusion"), 28}, {TJS_W("omPsNormal"), 13},
      {TJS_W("omPsAdditive"), 14}, {TJS_W("omPsSubtractive"), 15},
      {TJS_W("omPsMultiplicative"), 16}, {TJS_W("omPsScreen"), 17},
      {TJS_W("omPsOverlay"), 18}, {TJS_W("omPsHardLight"), 19},
      {TJS_W("omPsSoftLight"), 20}, {TJS_W("omPsColorDodge"), 21},
      {TJS_W("omPsColorDodge5"), 22}, {TJS_W("omPsColorBurn"), 23},
      {TJS_W("omPsLighten"), 24}, {TJS_W("omPsDarken"), 25},
      {TJS_W("omPsDifference"), 26}, {TJS_W("omPsDifference5"), 27},
      {TJS_W("omPsExclusion"), 28}, {TJS_W("omAdditive"), 3},
      {TJS_W("omSubtractive"), 4}, {TJS_W("omMultiplicative"), 5},
      {TJS_W("omDodge"), 8}, {TJS_W("omDarken"), 9},
      {TJS_W("omLighten"), 10}, {TJS_W("omScreen"), 11},
      {TJS_W("omAddAlpha"), 12}, {TJS_W("omOpaque"), 1},
      {TJS_W("omAlpha"), 2}, {TJS_W("omAuto"), 128},
      {TJS_W("dfBoth"), 0}, {TJS_W("dfAlpha"), 0},
      {TJS_W("dfAddAlpha"), 4}, {TJS_W("dfMain"), 1},
      {TJS_W("dfOpaque"), 1}, {TJS_W("dfMask"), 2},
      {TJS_W("dfProvince"), 3}, {TJS_W("dfAuto"), 128},
      {TJS_W("htMask"), 0}, {TJS_W("htProvince"), 1},
      {TJS_W("sttLeft"), 0}, {TJS_W("sttTop"), 1},
      {TJS_W("sttRight"), 2}, {TJS_W("sttBottom"), 3},
      {TJS_W("ststNoStay"), 0}, {TJS_W("ststStayDest"), 1},
      {TJS_W("ststStaySrc"), 2}, {TJS_W("tkdlNone"), 0},
      {TJS_W("tkdlSimple"), 1}, {TJS_W("tkdlVerbose"), 2},
      {TJS_W("atmNormal"), 0}, {TJS_W("atmExclusive"), 1},
      {TJS_W("atmAtIdle"), 2}, {TJS_W("stNearest"), 0},
      {TJS_W("stFastLinear"), 1}, {TJS_W("stLinear"), 2},
      {TJS_W("stCubic"), 3}, {TJS_W("stSemiFastLinear"), 4},
      {TJS_W("stFastCubic"), 5}, {TJS_W("stLanczos2"), 6},
      {TJS_W("stFastLanczos2"), 7}, {TJS_W("stLanczos3"), 8},
      {TJS_W("stFastLanczos3"), 9}, {TJS_W("stSpline16"), 10},
      {TJS_W("stFastSpline16"), 11}, {TJS_W("stSpline36"), 12},
      {TJS_W("stFastSpline36"), 13}, {TJS_W("stAreaAvg"), 14},
      {TJS_W("stFastAreaAvg"), 15}, {TJS_W("stGaussian"), 16},
      {TJS_W("stFastGaussian"), 17}, {TJS_W("stBlackmanSinc"), 18},
      {TJS_W("stFastBlackmanSinc"), 19}, {TJS_W("stRefNoClip"), 0x10000},
      {TJS_W("cbfText"), 1}, {TJS_W("clIdle"), 5},
      {TJS_W("clDeactivate"), 10}, {TJS_W("clMinimize"), 15},
      {TJS_W("clAll"), 100}, {TJS_W("vomOverlay"), 0},
      {TJS_W("vomLayer"), 1}, {TJS_W("vomMixer"), 2},
      {TJS_W("vomMFEVR"), 3}, {TJS_W("perLoop"), 0},
      {TJS_W("perPeriod"), 1}, {TJS_W("perPrepare"), 2},
      {TJS_W("perSegLoop"), 3}, {TJS_W("sgfmNeverMute"), 0},
      {TJS_W("sgfmMuteOnMinimize"), 1},
      {TJS_W("sgfmMuteOnDeactivate"), 2}, {TJS_W("oriUnknown"), 0},
      {TJS_W("oriPortrait"), 1}, {TJS_W("oriLandscape"), 2},
      {TJS_W("faReadOnly"), 0x01}, {TJS_W("faHidden"), 0x02},
      {TJS_W("faSysFile"), 0x04}, {TJS_W("faVolumeID"), 0x08},
      {TJS_W("faDirectory"), 0x10}, {TJS_W("faArchive"), 0x20},
      {TJS_W("faAnyFile"), 0x3f},
  };
  for (const Constant &constant : constants)
    set_global_property(engine, constant.name, tTJSVariant(constant.value));
}

void install_storage_compatibility(tTJS *engine) {
  iTJSDispatch2 *debug = TJSCreateDictionaryObject();
  try {
    set_method(debug, TJS_W("message"), debug_message);
    set_method(debug, TJS_W("notice"), debug_message);
    set_method(debug, TJS_W("warning"), debug_message);
    set_method(debug, TJS_W("getTickCount"), debug_get_tick_count);
    set_method(debug, TJS_W("logAsError"), storage_noop);
    set_global_object(engine, TJS_W("Debug"), debug);
  } catch (...) {
    debug->Release();
    throw;
  }
  debug->Release();

  iTJSDispatch2 *system = TJSCreateDictionaryObject();
  try {
    set_property(system, TJS_W("exePath"), tTJSVariant(TJS_W("")));
    set_property(system, TJS_W("personalPath"), tTJSVariant(TJS_W("")));
    set_property(system, TJS_W("osName"), tTJSVariant(TJS_W("Linux")));
    set_property(system, TJS_W("platformName"), tTJSVariant(TJS_W("Linux")));
    set_property(system, TJS_W("versionString"), tTJSVariant(TJS_W("Kirikinux2 Linux")));
    set_property(system, TJS_W("eventDisabled"), tTJSVariant(static_cast<tjs_int>(0)));
    set_method(system, TJS_W("createAppLock"), system_create_app_lock);
    set_method(system, TJS_W("getTickCount"), debug_get_tick_count);
    set_method(system, TJS_W("inform"), debug_message);
    set_global_object(engine, TJS_W("System"), system);
  } catch (...) {
    system->Release();
    throw;
  }
  system->Release();

  iTJSDispatch2 *plugins = TJSCreateDictionaryObject();
  try {
    set_method(plugins, TJS_W("link"), plugins_link);
    set_method(plugins, TJS_W("unlink"), plugins_unlink);
    set_method(plugins, TJS_W("getList"), plugins_get_list);
    set_global_object(engine, TJS_W("Plugins"), plugins);
  } catch (...) {
    plugins->Release();
    throw;
  }
  plugins->Release();

  set_global_property(engine, TJS_W("gcsAuto"),
                      tTJSVariant(static_cast<tjs_int>(-1)));
  set_global_property(engine, TJS_W("crArrow"),
                      tTJSVariant(static_cast<tjs_int>(-2)));
  set_global_property(engine, TJS_W("crHandPoint"),
                      tTJSVariant(static_cast<tjs_int>(-21)));
  set_global_property(engine, TJS_W("crSizeAll"),
                      tTJSVariant(static_cast<tjs_int>(-22)));

  iTJSDispatch2 *scripts = TJSCreateDictionaryObject();
  try {
    set_method(scripts, TJS_W("execStorage"), scripts_exec_storage);
    set_method(scripts, TJS_W("evalStorage"), scripts_eval_storage);
    set_method(scripts, TJS_W("exec"), scripts_exec);
    set_method(scripts, TJS_W("eval"), scripts_eval);
    set_method(scripts, TJS_W("foreach"), scripts_foreach);
    set_global_object(engine, TJS_W("Scripts"), scripts);
  } catch (...) {
    scripts->Release();
    throw;
  }
  scripts->Release();

  iTJSDispatch2 *storages = TJSCreateDictionaryObject();
  try {
    set_method(storages, TJS_W("isExistentStorage"), storages_exists);
    set_method(storages, TJS_W("getPlacedPath"), storages_placed_path);
    set_method(storages, TJS_W("getFullPath"), storages_placed_path);
    set_method(storages, TJS_W("extractStorageExt"), storages_extract_ext);
    set_method(storages, TJS_W("extractStorageName"), storages_extract_name);
    set_method(storages, TJS_W("extractStoragePath"), storages_extract_path);
    set_method(storages, TJS_W("chopStorageExt"), storages_chop_ext);
    set_method(storages, TJS_W("addAutoPath"), storages_add_auto_path);
    set_method(storages, TJS_W("removeAutoPath"), storages_remove_auto_path);
    set_method(storages, TJS_W("clearArchiveCache"), storage_noop);
    set_global_object(engine, TJS_W("Storages"), storages);
  } catch (...) {
    storages->Release();
    throw;
  }
  storages->Release();

  install_tvp_script_constants(engine);

  // Keep the diagnostic host's storage runner consistent with the full Linux
  // player: Android patches may probe Motion before disabling their D3D path.
  TVPInstallLinuxPluginCompatibility(*engine);
}

TjsRunResult make_value(tTJSVariant &value) {
  TjsRunResult result;
  result.ok = true;
  result.has_value = value.Type() != tvtVoid;
  if (result.has_value) {
    value.ToString();
    result.value = ttstr(value.GetString()).AsStdString();
  }
  return result;
}

template <typename Callback>
TjsRunResult invoke_tjs(tTJS *engine, Callback callback) {
  TjsRunResult result;
  try {
    tTJSVariant value;
    callback(engine, value);
    result = make_value(value);
  } catch (const eTJSScriptError &error) {
    result.error = narrow(error.GetMessage());
    if (error.GetBlockName()) {
      result.error += " at ";
      result.error += narrow(ttstr(error.GetBlockName()));
      const tjs_int line = error.GetSourceLine();
      if (line >= 0) result.error += ":" + std::to_string(line);
    }
  } catch (const eTJS &error) {
    result.error = narrow(error.GetMessage());
  } catch (const std::exception &error) {
    result.error = error.what();
  } catch (...) {
    result.error = "unknown TJS2 error";
  }
  return result;
}

template <typename Callback>
TjsRunResult run_tjs(Callback callback) {
  tTJS *engine = nullptr;
  try {
    engine = new tTJS();
  } catch (const eTJS &error) {
    TjsRunResult result;
    result.error = narrow(error.GetMessage());
    return result;
  } catch (const std::exception &error) {
    TjsRunResult result;
    result.error = error.what();
    return result;
  }

  // Script exceptions retain their source block. invoke_tjs destroys the
  // caught exception before this function releases the owning interpreter.
  TjsRunResult result = invoke_tjs(engine, callback);
  engine->Release();
  return result;
}

} // namespace

TjsRunResult evaluate_tjs(const std::string &expression) {
  return run_tjs([&](tTJS *engine, tTJSVariant &result) {
    const ttstr name(TJS_W("command line"));
    engine->EvalExpression(ttstr(expression), &result, nullptr, &name);
  });
}

TjsRunResult execute_tjs_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    TjsRunResult result;
    result.error = "cannot open script: " + path;
    return result;
  }

  std::vector<std::uint8_t> contents{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  if (!input.good() && !input.eof()) {
    TjsRunResult result;
    result.error = "cannot read script: " + path;
    return result;
  }

  return execute_tjs_bytes(contents, path);
}

TjsRunResult execute_tjs_bytes(const std::vector<std::uint8_t> &source,
                               const std::string &source_name) {
  if (source.size() >= 7 && std::equal(source.begin(), source.begin() + 7,
                                       std::begin("TJS2100"))) {
    return run_tjs([&](tTJS *engine, tTJSVariant &result) {
      install_storage_compatibility(engine);
      const ttstr name(source_name);
      engine->LoadByteCode(source.data(), source.size(), &result, nullptr,
                           name.c_str());
    });
  }
  std::string decoded;
  if (!decode_source(source, decoded)) {
    TjsRunResult result;
    result.error = "invalid UTF-16 source or embedded NUL byte: " + source_name;
    return result;
  }
  return run_tjs([&](tTJS *engine, tTJSVariant &result) {
    install_storage_compatibility(engine);
    const ttstr name(source_name);
    engine->ExecScript(ttstr(decoded), &result, nullptr, &name);
  });
}

TjsRunResult execute_tjs_storage(const std::string &root,
                                 const std::string &entry) {
  StorageRoot storage;
  std::string error;
  if (!storage.open(root, error)) {
    TjsRunResult result;
    result.error = error;
    return result;
  }
  return run_tjs([&](tTJS *engine, tTJSVariant &result) {
    ActiveStorageRuntime runtime{engine, &storage};
    ActiveStorageGuard guard(&runtime);
    install_storage_compatibility(engine);
    execute_storage_entry(entry, false, &result, nullptr);
  });
}

TjsRunResult execute_tjs_startup(const std::string &root) {
  StorageRoot storage;
  std::string error;
  if (!storage.open(root, error)) {
    TjsRunResult result;
    result.error = error;
    return result;
  }
  std::string startup;
  if (storage.exists("startup.tjs")) {
    startup = "startup.tjs";
  } else if (storage.exists("System/Initialize.tjs")) {
    startup = "System/Initialize.tjs";
  } else {
    TjsRunResult result;
    result.error = "neither startup.tjs nor System/Initialize.tjs exists in " + root;
    return result;
  }
  return run_tjs([&](tTJS *engine, tTJSVariant &result) {
    ActiveStorageRuntime runtime{engine, &storage};
    ActiveStorageGuard guard(&runtime);
    install_storage_compatibility(engine);
    execute_storage_entry(startup, false, &result, nullptr);
  });
}

} // namespace krkr2
