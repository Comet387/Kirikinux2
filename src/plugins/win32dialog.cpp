// win32dialog.dll for Linux.
//
// KiriKiri's win32dialog.tjs (WIN32DialogEX / WIN32GenericDialogEX) builds
// Win32 dialog templates on top of the native WIN32Dialog class.  Games use it
// for small tool dialogs; Senren*Banka's font selector
// (k2compat_fontselect.tjs) is one.  The old stub only had messageBox(), so
// building such a dialog failed ('Member "allBitmaps" does not exist').
//
// The class below provides the scripting surface of the native plugin
// (template holders, item accessors, Win32 constants on instances and on the
// class object).  Dialog templates are shown with GTK by
// platform/linux/LinuxWin32Dialog.cpp, which calls back onInit/onCommand/
// onHScroll/onVScroll like Windows' dialog procedure does.
#include "ncbind/ncbind.hpp"
#include "tjsError.h"
#include "tjsDictionary.h"
#include "tjsArray.h"
#include "../../platform/linux/LinuxWin32Dialog.h"
#include <exception>

#define NCB_MODULE_NAME TJS_W("win32dialog.dll")

namespace {

tTJSVariant prop(iTJSDispatch2 *obj, const tjs_char *name) {
	tTJSVariant v;
	if (obj) obj->PropGet(0, name, nullptr, &v, obj);
	return v;
}

tjs_int64 intOf(const tTJSVariant &v, tjs_int64 def = 0) {
	switch (v.Type()) {
	case tvtInteger: case tvtReal: return v.AsInteger();
	case tvtString: { ttstr s = v; return s.IsEmpty() ? def : v.AsInteger(); }
	default: return def;
	}
}

std::string textOf(const tTJSVariant &v) {
	if (v.Type() == tvtVoid) return std::string();
	return ttstr(v).AsStdString();
}

class ScriptHost : public KR2DlgHost {
public:
	explicit ScriptHost(iTJSDispatch2 *dlg) : dlg_(dlg) {}
	int pixelWidth = 0, pixelHeight = 0;
	bool failed = false;
	ttstr error;

	void onInit() override {
		tTJSVariant w((tjs_int)pixelWidth), h((tjs_int)pixelHeight);
		dlg_->PropSet(TJS_MEMBERENSURE, TJS_W("width"), nullptr, &w, dlg_);
		dlg_->PropSet(TJS_MEMBERENSURE, TJS_W("height"), nullptr, &h, dlg_);
		call(TJS_W("onInit"), 0x110 /*WM_INITDIALOG*/, 0, 0);
	}
	void onCommand(int id, int code) override {
		call(TJS_W("onCommand"), 0x111 /*WM_COMMAND*/, (tjs_int)((id & 0xFFFF) | ((code & 0xFFFF) << 16)), 0x10000 + id);
	}
	void onScroll(bool vertical, int id, int code, int pos) override {
		call(vertical ? TJS_W("onVScroll") : TJS_W("onHScroll"), vertical ? 0x115 : 0x114,
		     (tjs_int)((code & 0xFFFF) | ((pos & 0xFFFF) << 16)), 0x10000 + id);
	}

private:
	iTJSDispatch2 *dlg_;
	void call(const tjs_char *name, tjs_int msg, tjs_int wp, tjs_int lp) {
		if (failed) return;
		tTJSVariant a(msg), b(wp), c(lp), r;
		tTJSVariant *args[] = {&a, &b, &c};
		try {
			dlg_->FuncCall(0, name, nullptr, &r, 3, args, dlg_);
			return;
		} catch (const eTJS &e) {
			error = e.GetMessage();
		} catch (const std::exception &e) {
			error = ttstr(e.what());
		} catch (...) {
			error = TJS_W("unknown exception in dialog event");
		}
		// C++ exceptions must not unwind through GTK: end the dialog and
		// rethrow from run() instead.
		failed = true;
		KR2Win32DialogEnd(2 /*IDCANCEL*/);
	}
};

// KR2Win32DialogNative.run(dialog, template) -> result (-2: no GUI)
tjs_error TJS_INTF_METHOD nativeRun(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 2 || p[0]->Type() != tvtObject || p[1]->Type() != tvtObject) return TJS_E_BADPARAMCOUNT;
	iTJSDispatch2 *dlg = p[0]->AsObjectNoAddRef();
	iTJSDispatch2 *tmplObj = p[1]->AsObjectNoAddRef();
	KR2DlgTemplate t;
	t.title = textOf(prop(tmplObj, TJS_W("title")));
	t.style = (uint32_t)intOf(prop(tmplObj, TJS_W("style")));
	t.cx = (int)intOf(prop(tmplObj, TJS_W("cx")));
	t.cy = (int)intOf(prop(tmplObj, TJS_W("cy")));
	t.pointSize = (int)intOf(prop(tmplObj, TJS_W("pointSize")), 9);
	tTJSVariant itemsVar = prop(tmplObj, TJS_W("items"));
	if (itemsVar.Type() == tvtObject && itemsVar.AsObjectNoAddRef()) {
		iTJSDispatch2 *items = itemsVar.AsObjectNoAddRef();
		const tjs_int count = (tjs_int)intOf(prop(items, TJS_W("count")));
		for (tjs_int i = 0; i < count; ++i) {
			tTJSVariant iv;
			items->PropGetByNum(0, i, &iv, items);
			if (iv.Type() != tvtObject || !iv.AsObjectNoAddRef()) continue;
			iTJSDispatch2 *o = iv.AsObjectNoAddRef();
			KR2DlgItem item;
			item.id = (int)intOf(prop(o, TJS_W("id")), -1);
			tTJSVariant cls = prop(o, TJS_W("windowClass"));
			if (cls.Type() == tvtString) item.className = textOf(cls);
			else item.classAtom = (int)intOf(cls);
			item.style = (uint32_t)intOf(prop(o, TJS_W("style")));
			item.exStyle = (uint32_t)intOf(prop(o, TJS_W("exStyle")));
			item.x = (int)intOf(prop(o, TJS_W("x")));
			item.y = (int)intOf(prop(o, TJS_W("y")));
			item.cx = (int)intOf(prop(o, TJS_W("cx")));
			item.cy = (int)intOf(prop(o, TJS_W("cy")));
			item.title = textOf(prop(o, TJS_W("title")));
			t.items.push_back(item);
		}
	}
	ScriptHost host(dlg);
	const int r = KR2Win32DialogRun(t, host, &host.pixelWidth, &host.pixelHeight);
	if (host.failed) TVPThrowExceptionMessage(host.error.c_str());
	if (result) *result = (tjs_int)r;
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeActive(tTJSVariant *result, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
	if (result) *result = KR2Win32DialogActive();
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeEnd(tTJSVariant *, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	KR2Win32DialogEnd(n >= 1 ? (int)intOf(*p[0], 2) : 2);
	return TJS_S_OK;
}

// send(id, msg, wparam, lparam): string lparam for LB_ADDSTRING etc.;
// LB_GETTEXT / CB_GETLBTEXT / WM_GETTEXT return the string.
tjs_error TJS_INTF_METHOD nativeSend(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 2) return TJS_E_BADPARAMCOUNT;
	const int id = (int)intOf(*p[0], -1);
	const uint32_t msg = (uint32_t)intOf(*p[1]);
	const int64_t wp = n > 2 ? intOf(*p[2]) : 0;
	int64_t lp = 0;
	std::string lpText;
	bool hasText = false;
	if (n > 3) {
		if (p[3]->Type() == tvtString) { lpText = textOf(*p[3]); hasText = true; }
		else lp = intOf(*p[3]);
	}
	std::string outText;
	bool outIsText = false;
	const int64_t r = KR2Win32DialogSend(id, msg, wp, lp, hasText ? &lpText : nullptr, &outText, &outIsText);
	if (result) {
		if (outIsText) *result = ttstr(outText);
		else *result = (tTVInteger)r;
	}
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeGetText(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 1) return TJS_E_BADPARAMCOUNT;
	std::string s;
	const bool ok = KR2Win32DialogGetText((int)intOf(*p[0], -1), s);
	if (result) { if (ok) *result = ttstr(s); else result->Clear(); }
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeSetText(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 2) return TJS_E_BADPARAMCOUNT;
	const bool ok = KR2Win32DialogSetText((int)intOf(*p[0], -1), textOf(*p[1]));
	if (result) *result = ok;
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeSetEnabled(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 2) return TJS_E_BADPARAMCOUNT;
	const bool ok = KR2Win32DialogSetEnabled((int)intOf(*p[0], -1), p[1]->operator bool());
	if (result) *result = ok;
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeGetEnabled(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 1) return TJS_E_BADPARAMCOUNT;
	bool enabled = true;
	const bool ok = KR2Win32DialogGetEnabled((int)intOf(*p[0], -1), enabled);
	if (result) { if (ok) *result = enabled; else result->Clear(); }
	return TJS_S_OK;
}

tjs_error TJS_INTF_METHOD nativeSetFocus(tTJSVariant *, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 1) return TJS_E_BADPARAMCOUNT;
	KR2Win32DialogSetFocus((int)intOf(*p[0], -1));
	return TJS_S_OK;
}

// getRect(id) -> [x, y, width, height] in pixels, or void.
tjs_error TJS_INTF_METHOD nativeGetRect(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 1) return TJS_E_BADPARAMCOUNT;
	int x = 0, y = 0, w = 0, h = 0;
	const bool ok = KR2Win32DialogGetRect((int)intOf(*p[0], -1), x, y, w, h);
	if (!result) return TJS_S_OK;
	if (!ok) { result->Clear(); return TJS_S_OK; }
	iTJSDispatch2 *array = TJSCreateArrayObject();
	tTJSVariant values[4] = {(tjs_int)x, (tjs_int)y, (tjs_int)w, (tjs_int)h};
	for (int i = 0; i < 4; ++i) array->PropSetByNum(TJS_MEMBERENSURE, i, &values[i], array);
	*result = tTJSVariant(array, array);
	array->Release();
	return TJS_S_OK;
}

// setImage(id, layer): shows the layer's current image (SS_BITMAP / SS_ICON).
tjs_error TJS_INTF_METHOD nativeSetImage(tTJSVariant *result, tjs_int n, tTJSVariant **p, iTJSDispatch2 *) {
	if (n < 2 || p[1]->Type() != tvtObject) return TJS_E_BADPARAMCOUNT;
	iTJSDispatch2 *layer = p[1]->AsObjectNoAddRef();
	bool ok = false;
	try {
		const int w = (int)intOf(prop(layer, TJS_W("imageWidth")));
		const int h = (int)intOf(prop(layer, TJS_W("imageHeight")));
		const int64_t buffer = intOf(prop(layer, TJS_W("mainImageBuffer")));
		const int pitch = (int)intOf(prop(layer, TJS_W("mainImageBufferPitch")));
		if (w > 0 && h > 0 && buffer && pitch)
			ok = KR2Win32DialogSetImage((int)intOf(*p[0], -1), w, h, reinterpret_cast<const uint8_t *>((intptr_t)buffer), pitch);
	} catch (...) {
		ok = false; // a preview image is optional
	}
	if (result) *result = ok;
	return TJS_S_OK;
}

void addMethod(iTJSDispatch2 *obj, const tjs_char *name, tTJSNativeClassMethodCallback fn) {
	iTJSDispatch2 *m = TJSCreateNativeClassMethod(fn);
	tTJSVariant v(m, m);
	m->Release();
	obj->PropSet(TJS_MEMBERENSURE, name, nullptr, &v, obj);
}

} // namespace

static void InitPlugin_WIN32Dialog()
{
	iTJSDispatch2 *global = TVPGetScriptDispatch();
	if (global) {
		iTJSDispatch2 *native = TJSCreateDictionaryObject();
		addMethod(native, TJS_W("run"), nativeRun);
		addMethod(native, TJS_W("active"), nativeActive);
		addMethod(native, TJS_W("end"), nativeEnd);
		addMethod(native, TJS_W("send"), nativeSend);
		addMethod(native, TJS_W("getText"), nativeGetText);
		addMethod(native, TJS_W("setText"), nativeSetText);
		addMethod(native, TJS_W("setEnabled"), nativeSetEnabled);
		addMethod(native, TJS_W("getEnabled"), nativeGetEnabled);
		addMethod(native, TJS_W("setFocus"), nativeSetFocus);
		addMethod(native, TJS_W("setImage"), nativeSetImage);
		addMethod(native, TJS_W("getRect"), nativeGetRect);
		tTJSVariant v(native, native);
		native->Release();
		global->PropSet(TJS_MEMBERENSURE, TJS_W("KR2Win32DialogNative"), nullptr, &v, global);
		global->Release();
	}
	TVPExecuteScript(ttstr(TJS_W(R"TJS(

class WIN32Dialog
{
	// Template holders filled by win32dialog.tjs (WIN32DialogEX.store).
	class Header {
		var dlgItems = 0, _data;
		function Header() { _data = %[]; }
		function store(elm) {
			if (typeof elm != "Object" || !elm) return;
			(global.Dictionary.assign incontextof _data)(elm, false);
			var kv = []; kv.assign(elm);
			for (var i = 0; i + 1 < kv.count; i += 2) this[kv[i]] = kv[i + 1];
		}
	}
	class Items {
		var _data;
		function Items() { _data = %[]; }
		function store(elm) {
			if (typeof elm != "Object" || !elm) return;
			(global.Dictionary.assign incontextof _data)(elm, false);
			var kv = []; kv.assign(elm);
			for (var i = 0; i + 1 < kv.count; i += 2) this[kv[i]] = kv[i + 1];
		}
	}
	// A layer whose current image is shown by setItemBitmap.
	class Bitmap {
		var layer;
		function Bitmap(l) { layer = l; }
	}

	// No owner member: WIN32DialogEX keeps its own; the native class has none.
	var modeless = false, width = 0, height = 0, left = 0, top = 0;
	var _texts = %[], _ints = %[], _enabled = %[], _template;

	function WIN32Dialog(o) {}
	function finalize() {}

	property _native { getter { return typeof global.KR2Win32DialogNative != "undefined" ? global.KR2Win32DialogNative : void; } }
	property _running { getter { var n = _native; return n !== void && n.active(); } }

	// Rendered with GTK by the player (platform/linux/LinuxWin32Dialog.cpp).
	function open(win) {
		var n = _native;
		if (n !== void && _template !== void) {
			var r = n.run(this, _template);
			if (r != -2) return r;
		}
		global.WIN32Dialog.notifyUnsupported();
		return IDCANCEL;
	}
	function close(result) {
		if (_running) _native.end(result);
		return result;
	}
	function show() {}
	function makeTemplate(head, items*) {
		var t = %[ title:"", style:0, cx:0, cy:0, pointSize:9, items:[] ];
		if (typeof head == "Object" && head && typeof head._data == "Object")
			(global.Dictionary.assign incontextof t)(head._data, false);
		t.items = [];
		for (var i = 0; i < items.count; i++) {
			var it = items[i];
			if (typeof it == "Object" && it && typeof it._data == "Object") {
				var d = %[];
				(global.Dictionary.assign incontextof d)(it._data, true);
				t.items.add(d);
			}
		}
		_template = t;
	}
	function loadResource(*) { _template = void; }
	function onInit(*) { return true; }
	function onCommand(*) { return false; }
	function onNotify(*) { return false; }
	function onHScroll(*) { return false; }
	function onVScroll(*) { return false; }
	function onSize(*) { return false; }
	function getItem(id) { return 0x10000 + (int)id; }
	function setItemInt(id, v) { if (_running) _native.setText(id, "" + (int)v); else _ints[id] = v; }
	function getItemInt(id) { if (_running) { var t = _native.getText(id); return t === void ? 0 : +t; } return +_ints[id]; }
	function setItemText(id, v) { if (_running) _native.setText(id, "" + v); else _texts[id] = v; }
	function getItemText(id) {
		if (_running) { var t = _native.getText(id); if (t !== void) return t; }
		return _texts[id] !== void ? _texts[id] : "";
	}
	function setItemEnabled(id, v) { if (_running) _native.setEnabled(id, !!v); else _enabled[id] = v; }
	function getItemEnabled(id) {
		if (_running) { var e = _native.getEnabled(id); if (e !== void) return e; }
		return _enabled[id] !== void ? _enabled[id] : true;
	}
	function setItemFocus(id) { if (_running) _native.setFocus(id); }
	function _rect(id, i) { if (_running) { var r = _native.getRect(id); if (r !== void) return r[i]; } return 0; }
	function getItemLeft(id)   { return _rect(id, 0); }
	function getItemTop(id)    { return _rect(id, 1); }
	function getItemWidth(id)  { return _rect(id, 2); }
	function getItemHeight(id) { return _rect(id, 3); }
	function setItemPos(*) {}
	function setItemSize(*) {}
	function setItemBitmap(id, bmp) {
		if (!_running || typeof bmp != "Object" || !bmp) return;
		var layer = typeof bmp.layer == "Object" ? bmp.layer : bmp;
		if (layer) _native.setImage(id, layer);
	}
	function sendItemMessage(id, msg, wp, lp) {
		if (!_running) return 0;
		return _native.send(id, +msg, wp === void ? 0 : wp, lp === void ? 0 : lp);
	}
	function setPos(x, y) { left = x; top = y; }
	function setSize(w, h) { width = w; height = h; }
	function messageBox(message, caption, type) { return !System.inform(message, caption, 2); }

	// Static helpers of the native plugin (pointer tricks: meaningless here).
	function getOctetAddress(*) { return 0; }
	function getStringAddress(*) { return 0; }
	function getStringFromAddress(*) { return ""; }
	function initCommonControls(*) { return true; }
	function initCommonControlsEx(*) { return true; }

	// Dialogs loaded from a DLL resource, or no display: cannot be shown.
	function notifyUnsupported() {
		if (typeof global.kr2Win32DialogNotified != "undefined") return;
		global.kr2Win32DialogNotified = true;
		var text = "This game tried to open a Windows dialog that Kirikinux2 cannot display here.\n"
			+ "To change the text font, open Kirikinux2 Preferences -> \"Default Font\" and enable \"Force using default font\".\n\n"
			+ "此游戏试图打开 Kirikinux2 在此无法显示的 Windows 对话框。\n"
			+ "如需更换游戏字体，请在 Kirikinux2 设置中选择“默认字体”并勾选“强制使用默认字体”。";
		try { System.inform(text, "Kirikinux2"); } catch (e) { try { Debug.message(text); } catch (e2) {} }
	}
	// Win32 constants the wrapper script and game dialogs refer to by name.
	var IDOK = 1, IDCANCEL = 2, IDABORT = 3, IDRETRY = 4, IDIGNORE = 5, IDYES = 6, IDNO = 7, IDCLOSE = 8, IDHELP = 9;
	var MB_OK = 0x0, MB_OKCANCEL = 0x1, MB_ABORTRETRYIGNORE = 0x2, MB_YESNOCANCEL = 0x3, MB_YESNO = 0x4, MB_RETRYCANCEL = 0x5;
	var MB_ICONHAND = 0x10, MB_ICONERROR = 0x10, MB_ICONSTOP = 0x10, MB_ICONQUESTION = 0x20, MB_ICONEXCLAMATION = 0x30, MB_ICONWARNING = 0x30, MB_ICONASTERISK = 0x40, MB_ICONINFORMATION = 0x40;
	var MB_DEFBUTTON1 = 0x0, MB_DEFBUTTON2 = 0x100, MB_DEFBUTTON3 = 0x200, MB_APPLMODAL = 0x0, MB_SYSTEMMODAL = 0x1000, MB_TASKMODAL = 0x2000, MB_TOPMOST = 0x40000, MB_SETFOREGROUND = 0x10000;

	var WS_OVERLAPPED = 0x00000000, WS_POPUP = 0x80000000, WS_CHILD = 0x40000000, WS_MINIMIZE = 0x20000000, WS_VISIBLE = 0x10000000;
	var WS_DISABLED = 0x08000000, WS_CLIPSIBLINGS = 0x04000000, WS_CLIPCHILDREN = 0x02000000, WS_MAXIMIZE = 0x01000000, WS_CAPTION = 0x00C00000;
	var WS_BORDER = 0x00800000, WS_DLGFRAME = 0x00400000, WS_VSCROLL = 0x00200000, WS_HSCROLL = 0x00100000, WS_SYSMENU = 0x00080000;
	var WS_THICKFRAME = 0x00040000, WS_GROUP = 0x00020000, WS_TABSTOP = 0x00010000, WS_MINIMIZEBOX = 0x00020000, WS_MAXIMIZEBOX = 0x00010000;
	var WS_SIZEBOX = 0x00040000, WS_OVERLAPPEDWINDOW = 0x00CF0000, WS_POPUPWINDOW = 0x80880000;
	var WS_EX_DLGMODALFRAME = 0x1, WS_EX_TOPMOST = 0x8, WS_EX_TOOLWINDOW = 0x80, WS_EX_WINDOWEDGE = 0x100, WS_EX_CLIENTEDGE = 0x200, WS_EX_STATICEDGE = 0x20000, WS_EX_CONTROLPARENT = 0x10000;

	var DS_ABSALIGN = 0x01, DS_SYSMODAL = 0x02, DS_LOCALEDIT = 0x20, DS_SETFONT = 0x40, DS_MODALFRAME = 0x80, DS_NOIDLEMSG = 0x100;
	var DS_SETFOREGROUND = 0x200, DS_3DLOOK = 0x4, DS_FIXEDSYS = 0x8, DS_NOFAILCREATE = 0x10, DS_CONTROL = 0x400, DS_CENTER = 0x800;
	var DS_CENTERMOUSE = 0x1000, DS_CONTEXTHELP = 0x2000, DS_SHELLFONT = 0x48;
	var FW_DONTCARE = 0, FW_THIN = 100, FW_EXTRALIGHT = 200, FW_LIGHT = 300, FW_NORMAL = 400, FW_REGULAR = 400, FW_MEDIUM = 500, FW_SEMIBOLD = 600, FW_BOLD = 700, FW_EXTRABOLD = 800, FW_HEAVY = 900;

	var BUTTON = 0x80, EDIT = 0x81, STATIC = 0x82, LISTBOX = 0x83, SCROLLBAR = 0x84, COMBOBOX = 0x85;
	var LISTVIEW = "SysListView32", TREEVIEW = "SysTreeView32", TRACKBAR = "msctls_trackbar32", UPDOWN = "msctls_updown32", PROGRESS = "msctls_progress32", TABCONTROL = "SysTabControl32";

	var BS_PUSHBUTTON = 0x0, BS_DEFPUSHBUTTON = 0x1, BS_CHECKBOX = 0x2, BS_AUTOCHECKBOX = 0x3, BS_RADIOBUTTON = 0x4, BS_3STATE = 0x5, BS_AUTO3STATE = 0x6;
	var BS_GROUPBOX = 0x7, BS_USERBUTTON = 0x8, BS_AUTORADIOBUTTON = 0x9, BS_OWNERDRAW = 0xB, BS_LEFTTEXT = 0x20, BS_TEXT = 0x0, BS_ICON = 0x40, BS_BITMAP = 0x80;
	var BS_LEFT = 0x100, BS_RIGHT = 0x200, BS_CENTER = 0x300, BS_TOP = 0x400, BS_BOTTOM = 0x800, BS_VCENTER = 0xC00, BS_PUSHLIKE = 0x1000, BS_MULTILINE = 0x2000, BS_NOTIFY = 0x4000, BS_FLAT = 0x8000;
	var BST_UNCHECKED = 0, BST_CHECKED = 1, BST_INDETERMINATE = 2, BST_PUSHED = 4, BST_FOCUS = 8;
	var BM_GETCHECK = 0xF0, BM_SETCHECK = 0xF1, BM_GETSTATE = 0xF2, BM_SETSTATE = 0xF3, BM_SETSTYLE = 0xF4, BM_CLICK = 0xF5, BM_GETIMAGE = 0xF6, BM_SETIMAGE = 0xF7;
	var BN_CLICKED = 0, BN_PAINT = 1, BN_DBLCLK = 5, BN_SETFOCUS = 6, BN_KILLFOCUS = 7;

	var SS_LEFT = 0x0, SS_CENTER = 0x1, SS_RIGHT = 0x2, SS_ICON = 0x3, SS_BLACKRECT = 0x4, SS_GRAYRECT = 0x5, SS_WHITERECT = 0x6, SS_BLACKFRAME = 0x7;
	var SS_GRAYFRAME = 0x8, SS_WHITEFRAME = 0x9, SS_SIMPLE = 0xB, SS_LEFTNOWORDWRAP = 0xC, SS_OWNERDRAW = 0xD, SS_BITMAP = 0xE, SS_ETCHEDHORZ = 0x10, SS_ETCHEDVERT = 0x11, SS_ETCHEDFRAME = 0x12;
	var SS_NOPREFIX = 0x80, SS_NOTIFY = 0x100, SS_CENTERIMAGE = 0x200, SS_RIGHTJUST = 0x400, SS_SUNKEN = 0x1000;

	var ES_LEFT = 0x0, ES_CENTER = 0x1, ES_RIGHT = 0x2, ES_MULTILINE = 0x4, ES_UPPERCASE = 0x8, ES_LOWERCASE = 0x10, ES_PASSWORD = 0x20, ES_AUTOVSCROLL = 0x40;
	var ES_AUTOHSCROLL = 0x80, ES_NOHIDESEL = 0x100, ES_OEMCONVERT = 0x400, ES_READONLY = 0x800, ES_WANTRETURN = 0x1000, ES_NUMBER = 0x2000;
	var EM_GETSEL = 0xB0, EM_SETSEL = 0xB1, EM_GETLINECOUNT = 0xBA, EM_LIMITTEXT = 0xC5, EM_SETLIMITTEXT = 0xC5, EM_SETREADONLY = 0xCF;
	var EN_SETFOCUS = 0x100, EN_KILLFOCUS = 0x200, EN_CHANGE = 0x300, EN_UPDATE = 0x400;

	var LBS_NOTIFY = 0x1, LBS_SORT = 0x2, LBS_NOREDRAW = 0x4, LBS_MULTIPLESEL = 0x8, LBS_OWNERDRAWFIXED = 0x10, LBS_HASSTRINGS = 0x40;
	var LBS_USETABSTOPS = 0x80, LBS_NOINTEGRALHEIGHT = 0x100, LBS_MULTICOLUMN = 0x200, LBS_WANTKEYBOARDINPUT = 0x400, LBS_EXTENDEDSEL = 0x800, LBS_DISABLENOSCROLL = 0x1000, LBS_NOSEL = 0x4000;
	var LBS_STANDARD = 0xA00003;
	var LB_ADDSTRING = 0x180, LB_INSERTSTRING = 0x181, LB_DELETESTRING = 0x182, LB_RESETCONTENT = 0x184, LB_SETSEL = 0x185, LB_SETCURSEL = 0x186;
	var LB_GETSEL = 0x187, LB_GETCURSEL = 0x188, LB_GETTEXT = 0x189, LB_GETTEXTLEN = 0x18A, LB_GETCOUNT = 0x18B, LB_SELECTSTRING = 0x18C;
	var LB_GETTOPINDEX = 0x18E, LB_FINDSTRING = 0x18F, LB_GETSELCOUNT = 0x190, LB_GETSELITEMS = 0x191, LB_SETTOPINDEX = 0x197, LB_FINDSTRINGEXACT = 0x1A2;
	var LBN_ERRSPACE = -2, LBN_SELCHANGE = 1, LBN_DBLCLK = 2, LBN_SELCANCEL = 3, LBN_SETFOCUS = 4, LBN_KILLFOCUS = 5;

	var CBS_SIMPLE = 0x1, CBS_DROPDOWN = 0x2, CBS_DROPDOWNLIST = 0x3, CBS_OWNERDRAWFIXED = 0x10, CBS_AUTOHSCROLL = 0x40, CBS_OEMCONVERT = 0x80;
	var CBS_SORT = 0x100, CBS_HASSTRINGS = 0x200, CBS_NOINTEGRALHEIGHT = 0x400, CBS_DISABLENOSCROLL = 0x800;
	var CB_GETEDITSEL = 0x140, CB_LIMITTEXT = 0x141, CB_SETEDITSEL = 0x142, CB_ADDSTRING = 0x143, CB_DELETESTRING = 0x144, CB_GETCOUNT = 0x146;
	var CB_GETCURSEL = 0x147, CB_GETLBTEXT = 0x148, CB_GETLBTEXTLEN = 0x149, CB_INSERTSTRING = 0x14A, CB_RESETCONTENT = 0x14B, CB_FINDSTRING = 0x14C;
	var CB_SELECTSTRING = 0x14D, CB_SETCURSEL = 0x14E, CB_SHOWDROPDOWN = 0x14F, CB_FINDSTRINGEXACT = 0x158;
	var CBN_ERRSPACE = -1, CBN_SELCHANGE = 1, CBN_DBLCLK = 2, CBN_SETFOCUS = 3, CBN_KILLFOCUS = 4, CBN_EDITCHANGE = 5, CBN_EDITUPDATE = 6, CBN_DROPDOWN = 7, CBN_CLOSEUP = 8, CBN_SELENDOK = 9, CBN_SELENDCANCEL = 10;

	var SBS_HORZ = 0x0, SBS_VERT = 0x1, SB_HORZ = 0, SB_VERT = 1, SB_CTL = 2;
	var SB_LINEUP = 0, SB_LINELEFT = 0, SB_LINEDOWN = 1, SB_LINERIGHT = 1, SB_PAGEUP = 2, SB_PAGELEFT = 2, SB_PAGEDOWN = 3, SB_PAGERIGHT = 3;
	var SB_THUMBPOSITION = 4, SB_THUMBTRACK = 5, SB_TOP = 6, SB_LEFT = 6, SB_BOTTOM = 7, SB_RIGHT = 7, SB_ENDSCROLL = 8;
	var SBM_SETPOS = 0xE0, SBM_GETPOS = 0xE1, SBM_SETRANGE = 0xE2, SBM_GETRANGE = 0xE3, SBM_SETRANGEREDRAW = 0xE6, SBM_SETSCROLLINFO = 0xE9, SBM_GETSCROLLINFO = 0xEA;

	var TBS_AUTOTICKS = 0x1, TBS_VERT = 0x2, TBS_HORZ = 0x0, TBS_TOP = 0x4, TBS_BOTTOM = 0x0, TBS_LEFT = 0x4, TBS_RIGHT = 0x0, TBS_BOTH = 0x8, TBS_NOTICKS = 0x10;
	var TBM_GETPOS = 0x400, TBM_GETRANGEMIN = 0x401, TBM_GETRANGEMAX = 0x402, TBM_SETTIC = 0x404, TBM_SETPOS = 0x405, TBM_SETRANGE = 0x406, TBM_SETRANGEMIN = 0x407, TBM_SETRANGEMAX = 0x408;
	var TBM_SETPAGESIZE = 0x415, TBM_SETLINESIZE = 0x417, TBM_SETTICFREQ = 0x414;
	var UDM_SETRANGE = 0x465, UDM_GETRANGE = 0x466, UDM_SETPOS = 0x467, UDM_GETPOS = 0x468, UDM_SETRANGE32 = 0x46F, UDM_GETRANGE32 = 0x470, UDM_SETPOS32 = 0x471, UDM_GETPOS32 = 0x472;
	var PBM_SETRANGE = 0x401, PBM_SETPOS = 0x402, PBM_DELTAPOS = 0x403, PBM_SETSTEP = 0x404, PBM_STEPIT = 0x405, PBM_SETRANGE32 = 0x406;

	var LVS_ICON = 0x0, LVS_REPORT = 0x1, LVS_SMALLICON = 0x2, LVS_LIST = 0x3, LVS_SINGLESEL = 0x4, LVS_SHOWSELALWAYS = 0x8, LVS_SORTASCENDING = 0x10;
	var LVS_SORTDESCENDING = 0x20, LVS_NOLABELWRAP = 0x80, LVS_AUTOARRANGE = 0x100, LVS_EDITLABELS = 0x200, LVS_NOSCROLL = 0x2000, LVS_NOCOLUMNHEADER = 0x4000, LVS_NOSORTHEADER = 0x8000;
	var LVS_EX_GRIDLINES = 0x1, LVS_EX_CHECKBOXES = 0x4, LVS_EX_FULLROWSELECT = 0x20;
	var LVCF_FMT = 0x1, LVCF_WIDTH = 0x2, LVCF_TEXT = 0x4, LVCF_SUBITEM = 0x8, LVCFMT_LEFT = 0x0, LVCFMT_RIGHT = 0x1, LVCFMT_CENTER = 0x2;
	var LVM_FIRST = 0x1000, LVM_GETITEMCOUNT = 0x1004, LVM_DELETEITEM = 0x1008, LVM_DELETEALLITEMS = 0x1009, LVM_DELETECOLUMN = 0x101C;
	var LVM_SETEXTENDEDLISTVIEWSTYLE = 0x1036, LVM_INSERTITEMW = 0x104D, LVM_SETITEMW = 0x104C, LVM_INSERTCOLUMNW = 0x1061, LVM_SETITEMTEXTW = 0x1074;
	var ICC_LISTVIEW_CLASSES = 0x1, ICC_TREEVIEW_CLASSES = 0x2, ICC_BAR_CLASSES = 0x4, ICC_TAB_CLASSES = 0x8, ICC_UPDOWN_CLASS = 0x10, ICC_PROGRESS_CLASS = 0x20;
	var ICC_HOTKEY_CLASS = 0x40, ICC_ANIMATE_CLASS = 0x80, ICC_WIN95_CLASSES = 0xFF, ICC_DATE_CLASSES = 0x100, ICC_USEREX_CLASSES = 0x200, ICC_COOL_CLASSES = 0x400, ICC_STANDARD_CLASSES = 0x4000;

	var WM_INITDIALOG = 0x110, WM_COMMAND = 0x111, WM_NOTIFY = 0x4E, WM_HSCROLL = 0x114, WM_VSCROLL = 0x115, WM_SIZE = 0x5, WM_CLOSE = 0x10, WM_SETTEXT = 0xC, WM_GETTEXT = 0xD;
	var WM_GETTEXTLENGTH = 0xE, WM_SETFONT = 0x30, WM_USER = 0x400, WM_APP = 0x8000;
}

// The native plugin also exposes the constants on the class object
// (e.g. WIN32Dialog.IDOK), so copy them there as well.
{
	var names = [
		"IDOK", "IDCANCEL", "IDABORT", "IDRETRY", "IDIGNORE", "IDYES", "IDNO", "IDCLOSE", "IDHELP", "MB_OK",
		"MB_OKCANCEL", "MB_ABORTRETRYIGNORE", "MB_YESNOCANCEL", "MB_YESNO", "MB_RETRYCANCEL", "MB_ICONHAND",
		"MB_ICONERROR", "MB_ICONSTOP", "MB_ICONQUESTION", "MB_ICONEXCLAMATION", "MB_ICONWARNING", "MB_ICONASTERISK",
		"MB_ICONINFORMATION", "MB_DEFBUTTON1", "MB_DEFBUTTON2", "MB_DEFBUTTON3", "MB_APPLMODAL", "MB_SYSTEMMODAL",
		"MB_TASKMODAL", "MB_TOPMOST", "MB_SETFOREGROUND", "WS_OVERLAPPED", "WS_POPUP", "WS_CHILD", "WS_MINIMIZE",
		"WS_VISIBLE", "WS_DISABLED", "WS_CLIPSIBLINGS", "WS_CLIPCHILDREN", "WS_MAXIMIZE", "WS_CAPTION", "WS_BORDER",
		"WS_DLGFRAME", "WS_VSCROLL", "WS_HSCROLL", "WS_SYSMENU", "WS_THICKFRAME", "WS_GROUP", "WS_TABSTOP",
		"WS_MINIMIZEBOX", "WS_MAXIMIZEBOX", "WS_SIZEBOX", "WS_OVERLAPPEDWINDOW", "WS_POPUPWINDOW",
		"WS_EX_DLGMODALFRAME", "WS_EX_TOPMOST", "WS_EX_TOOLWINDOW", "WS_EX_WINDOWEDGE", "WS_EX_CLIENTEDGE",
		"WS_EX_STATICEDGE", "WS_EX_CONTROLPARENT", "DS_ABSALIGN", "DS_SYSMODAL", "DS_LOCALEDIT", "DS_SETFONT",
		"DS_MODALFRAME", "DS_NOIDLEMSG", "DS_SETFOREGROUND", "DS_3DLOOK", "DS_FIXEDSYS", "DS_NOFAILCREATE",
		"DS_CONTROL", "DS_CENTER", "DS_CENTERMOUSE", "DS_CONTEXTHELP", "DS_SHELLFONT", "FW_DONTCARE", "FW_THIN",
		"FW_EXTRALIGHT", "FW_LIGHT", "FW_NORMAL", "FW_REGULAR", "FW_MEDIUM", "FW_SEMIBOLD", "FW_BOLD",
		"FW_EXTRABOLD", "FW_HEAVY", "BUTTON", "EDIT", "STATIC", "LISTBOX", "SCROLLBAR", "COMBOBOX", "LISTVIEW",
		"TREEVIEW", "TRACKBAR", "UPDOWN", "PROGRESS", "TABCONTROL", "BS_PUSHBUTTON", "BS_DEFPUSHBUTTON",
		"BS_CHECKBOX", "BS_AUTOCHECKBOX", "BS_RADIOBUTTON", "BS_3STATE", "BS_AUTO3STATE", "BS_GROUPBOX",
		"BS_USERBUTTON", "BS_AUTORADIOBUTTON", "BS_OWNERDRAW", "BS_LEFTTEXT", "BS_TEXT", "BS_ICON", "BS_BITMAP",
		"BS_LEFT", "BS_RIGHT", "BS_CENTER", "BS_TOP", "BS_BOTTOM", "BS_VCENTER", "BS_PUSHLIKE", "BS_MULTILINE",
		"BS_NOTIFY", "BS_FLAT", "BST_UNCHECKED", "BST_CHECKED", "BST_INDETERMINATE", "BST_PUSHED", "BST_FOCUS",
		"BM_GETCHECK", "BM_SETCHECK", "BM_GETSTATE", "BM_SETSTATE", "BM_SETSTYLE", "BM_CLICK", "BM_GETIMAGE",
		"BM_SETIMAGE", "BN_CLICKED", "BN_PAINT", "BN_DBLCLK", "BN_SETFOCUS", "BN_KILLFOCUS", "SS_LEFT", "SS_CENTER",
		"SS_RIGHT", "SS_ICON", "SS_BLACKRECT", "SS_GRAYRECT", "SS_WHITERECT", "SS_BLACKFRAME", "SS_GRAYFRAME",
		"SS_WHITEFRAME", "SS_SIMPLE", "SS_LEFTNOWORDWRAP", "SS_OWNERDRAW", "SS_BITMAP", "SS_ETCHEDHORZ",
		"SS_ETCHEDVERT", "SS_ETCHEDFRAME", "SS_NOPREFIX", "SS_NOTIFY", "SS_CENTERIMAGE", "SS_RIGHTJUST", "SS_SUNKEN",
		"ES_LEFT", "ES_CENTER", "ES_RIGHT", "ES_MULTILINE", "ES_UPPERCASE", "ES_LOWERCASE", "ES_PASSWORD",
		"ES_AUTOVSCROLL", "ES_AUTOHSCROLL", "ES_NOHIDESEL", "ES_OEMCONVERT", "ES_READONLY", "ES_WANTRETURN",
		"ES_NUMBER", "EM_GETSEL", "EM_SETSEL", "EM_GETLINECOUNT", "EM_LIMITTEXT", "EM_SETLIMITTEXT",
		"EM_SETREADONLY", "EN_SETFOCUS", "EN_KILLFOCUS", "EN_CHANGE", "EN_UPDATE", "LBS_NOTIFY", "LBS_SORT",
		"LBS_NOREDRAW", "LBS_MULTIPLESEL", "LBS_OWNERDRAWFIXED", "LBS_HASSTRINGS", "LBS_USETABSTOPS",
		"LBS_NOINTEGRALHEIGHT", "LBS_MULTICOLUMN", "LBS_WANTKEYBOARDINPUT", "LBS_EXTENDEDSEL", "LBS_DISABLENOSCROLL",
		"LBS_NOSEL", "LBS_STANDARD", "LB_ADDSTRING", "LB_INSERTSTRING", "LB_DELETESTRING", "LB_RESETCONTENT",
		"LB_SETSEL", "LB_SETCURSEL", "LB_GETSEL", "LB_GETCURSEL", "LB_GETTEXT", "LB_GETTEXTLEN", "LB_GETCOUNT",
		"LB_SELECTSTRING", "LB_GETTOPINDEX", "LB_FINDSTRING", "LB_GETSELCOUNT", "LB_GETSELITEMS", "LB_SETTOPINDEX",
		"LB_FINDSTRINGEXACT", "LBN_ERRSPACE", "LBN_SELCHANGE", "LBN_DBLCLK", "LBN_SELCANCEL", "LBN_SETFOCUS",
		"LBN_KILLFOCUS", "CBS_SIMPLE", "CBS_DROPDOWN", "CBS_DROPDOWNLIST", "CBS_OWNERDRAWFIXED", "CBS_AUTOHSCROLL",
		"CBS_OEMCONVERT", "CBS_SORT", "CBS_HASSTRINGS", "CBS_NOINTEGRALHEIGHT", "CBS_DISABLENOSCROLL",
		"CB_GETEDITSEL", "CB_LIMITTEXT", "CB_SETEDITSEL", "CB_ADDSTRING", "CB_DELETESTRING", "CB_GETCOUNT",
		"CB_GETCURSEL", "CB_GETLBTEXT", "CB_GETLBTEXTLEN", "CB_INSERTSTRING", "CB_RESETCONTENT", "CB_FINDSTRING",
		"CB_SELECTSTRING", "CB_SETCURSEL", "CB_SHOWDROPDOWN", "CB_FINDSTRINGEXACT", "CBN_ERRSPACE", "CBN_SELCHANGE",
		"CBN_DBLCLK", "CBN_SETFOCUS", "CBN_KILLFOCUS", "CBN_EDITCHANGE", "CBN_EDITUPDATE", "CBN_DROPDOWN",
		"CBN_CLOSEUP", "CBN_SELENDOK", "CBN_SELENDCANCEL", "SBS_HORZ", "SBS_VERT", "SB_HORZ", "SB_VERT", "SB_CTL",
		"SB_LINEUP", "SB_LINELEFT", "SB_LINEDOWN", "SB_LINERIGHT", "SB_PAGEUP", "SB_PAGELEFT", "SB_PAGEDOWN",
		"SB_PAGERIGHT", "SB_THUMBPOSITION", "SB_THUMBTRACK", "SB_TOP", "SB_LEFT", "SB_BOTTOM", "SB_RIGHT",
		"SB_ENDSCROLL", "SBM_SETPOS", "SBM_GETPOS", "SBM_SETRANGE", "SBM_GETRANGE", "SBM_SETRANGEREDRAW",
		"SBM_SETSCROLLINFO", "SBM_GETSCROLLINFO", "TBS_AUTOTICKS", "TBS_VERT", "TBS_HORZ", "TBS_TOP", "TBS_BOTTOM",
		"TBS_LEFT", "TBS_RIGHT", "TBS_BOTH", "TBS_NOTICKS", "TBM_GETPOS", "TBM_GETRANGEMIN", "TBM_GETRANGEMAX",
		"TBM_SETTIC", "TBM_SETPOS", "TBM_SETRANGE", "TBM_SETRANGEMIN", "TBM_SETRANGEMAX", "TBM_SETPAGESIZE",
		"TBM_SETLINESIZE", "TBM_SETTICFREQ", "UDM_SETRANGE", "UDM_GETRANGE", "UDM_SETPOS", "UDM_GETPOS",
		"UDM_SETRANGE32", "UDM_GETRANGE32", "UDM_SETPOS32", "UDM_GETPOS32", "PBM_SETRANGE", "PBM_SETPOS",
		"PBM_DELTAPOS", "PBM_SETSTEP", "PBM_STEPIT", "PBM_SETRANGE32", "LVS_ICON", "LVS_REPORT", "LVS_SMALLICON",
		"LVS_LIST", "LVS_SINGLESEL", "LVS_SHOWSELALWAYS", "LVS_SORTASCENDING", "LVS_SORTDESCENDING",
		"LVS_NOLABELWRAP", "LVS_AUTOARRANGE", "LVS_EDITLABELS", "LVS_NOSCROLL", "LVS_NOCOLUMNHEADER",
		"LVS_NOSORTHEADER", "LVS_EX_GRIDLINES", "LVS_EX_CHECKBOXES", "LVS_EX_FULLROWSELECT", "LVCF_FMT",
		"LVCF_WIDTH", "LVCF_TEXT", "LVCF_SUBITEM", "LVCFMT_LEFT", "LVCFMT_RIGHT", "LVCFMT_CENTER", "LVM_FIRST",
		"LVM_GETITEMCOUNT", "LVM_DELETEITEM", "LVM_DELETEALLITEMS", "LVM_DELETECOLUMN",
		"LVM_SETEXTENDEDLISTVIEWSTYLE", "LVM_INSERTITEMW", "LVM_SETITEMW", "LVM_INSERTCOLUMNW", "LVM_SETITEMTEXTW",
		"ICC_LISTVIEW_CLASSES", "ICC_TREEVIEW_CLASSES", "ICC_BAR_CLASSES", "ICC_TAB_CLASSES", "ICC_UPDOWN_CLASS",
		"ICC_PROGRESS_CLASS", "ICC_HOTKEY_CLASS", "ICC_ANIMATE_CLASS", "ICC_WIN95_CLASSES", "ICC_DATE_CLASSES",
		"ICC_USEREX_CLASSES", "ICC_COOL_CLASSES", "ICC_STANDARD_CLASSES", "WM_INITDIALOG", "WM_COMMAND", "WM_NOTIFY",
		"WM_HSCROLL", "WM_VSCROLL", "WM_SIZE", "WM_CLOSE", "WM_SETTEXT", "WM_GETTEXT", "WM_GETTEXTLENGTH",
		"WM_SETFONT", "WM_USER", "WM_APP"
	];
	var proto = new WIN32Dialog(null);
	for (var i = 0; i < names.count; i++) global.WIN32Dialog[names[i]] = proto[names[i]];
	invalidate proto;
}
)TJS")));
}

NCB_PRE_REGIST_CALLBACK(InitPlugin_WIN32Dialog);
