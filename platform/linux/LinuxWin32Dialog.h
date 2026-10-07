// SPDX-License-Identifier: AGPL-3.0-only
// GTK rendering of Win32 dialog templates for win32dialog.dll
// (src/plugins/win32dialog.cpp).  Games build such dialogs with KiriKiri's
// win32dialog.tjs, e.g. the font selector of Senren*Banka
// (k2compat_fontselect.tjs: label, list box, sample image, OK/Cancel).
//
// The interface uses only standard types so the plugin does not need GTK and
// this file does not need the TJS headers.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct KR2DlgItem {
	int id = -1;
	int classAtom = 0;          // 0x80 BUTTON ... 0x85 COMBOBOX, 0 = use className
	std::string className;      // e.g. "msctls_trackbar32"
	uint32_t style = 0;
	uint32_t exStyle = 0;
	int x = 0, y = 0, cx = 0, cy = 0; // dialog units
	std::string title;
};

struct KR2DlgTemplate {
	std::string title;
	uint32_t style = 0;
	int cx = 0, cy = 0;         // dialog units
	int pointSize = 9;
	std::vector<KR2DlgItem> items;
};

// Calls back into the script side.  Implementations must not throw.
class KR2DlgHost {
public:
	virtual ~KR2DlgHost() {}
	virtual void onInit() = 0;
	// WM_COMMAND: notifyCode is BN_CLICKED, LBN_SELCHANGE, ...
	virtual void onCommand(int id, int notifyCode) = 0;
	// WM_HSCROLL / WM_VSCROLL from a scroll bar or track bar
	virtual void onScroll(bool vertical, int id, int code, int pos) = 0;
};

// Shows the dialog modally.  Returns the value passed to KR2Win32DialogEnd, or
// -2 when no GUI is available.
int KR2Win32DialogRun(const KR2DlgTemplate &tmpl, KR2DlgHost &host, int *pixelWidth = nullptr, int *pixelHeight = nullptr);
bool KR2Win32DialogActive();
void KR2Win32DialogEnd(int result);

// Item access on the active (innermost) dialog.  The text overloads carry
// string parameters/results (LB_ADDSTRING, LB_GETTEXT, ...).
int64_t KR2Win32DialogSend(int id, uint32_t msg, int64_t wparam, int64_t lparam,
                           const std::string *lparamText, std::string *resultText, bool *resultIsText);
bool KR2Win32DialogGetText(int id, std::string &out);
bool KR2Win32DialogSetText(int id, const std::string &text);
bool KR2Win32DialogSetEnabled(int id, bool enabled);
bool KR2Win32DialogGetEnabled(int id, bool &enabled);
bool KR2Win32DialogSetFocus(int id);
// Item position and size in pixels (dialog client coordinates).
bool KR2Win32DialogGetRect(int id, int &x, int &y, int &width, int &height);
// 32-bit ARGB pixels (KiriKiri layer format), pitch in bytes.
bool KR2Win32DialogSetImage(int id, int width, int height, const uint8_t *argb, int pitch);
