// SPDX-License-Identifier: AGPL-3.0-only
// GTK rendering of Win32 dialog templates (see LinuxWin32Dialog.h).
#include "LinuxWin32Dialog.h"

#ifdef KR2_LINUX_HAVE_GTK3
#include <gtk/gtk.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>

namespace {

// ---- Win32 values used below ------------------------------------------------
enum : uint32_t {
	WS_VISIBLE = 0x10000000, WS_DISABLED = 0x08000000, WS_GROUP = 0x00020000,
	BS_TYPEMASK = 0xF, BS_PUSHBUTTON = 0, BS_DEFPUSHBUTTON = 1, BS_CHECKBOX = 2, BS_AUTOCHECKBOX = 3,
	BS_RADIOBUTTON = 4, BS_3STATE = 5, BS_AUTO3STATE = 6, BS_GROUPBOX = 7, BS_AUTORADIOBUTTON = 9,
	SS_TYPEMASK = 0x1F, SS_LEFT = 0, SS_CENTER = 1, SS_RIGHT = 2, SS_ICON = 3, SS_SIMPLE = 0xB,
	SS_LEFTNOWORDWRAP = 0xC, SS_BITMAP = 0xE, SS_ETCHEDHORZ = 0x10, SS_ETCHEDVERT = 0x11, SS_ETCHEDFRAME = 0x12,
	SS_CENTERIMAGE = 0x200,
	ES_CENTER = 1, ES_RIGHT = 2, ES_MULTILINE = 4, ES_PASSWORD = 0x20, ES_READONLY = 0x800,
	LBS_NOTIFY = 1, LBS_MULTIPLESEL = 8, LBS_EXTENDEDSEL = 0x800, LBS_NOSEL = 0x4000,
	CBS_TYPEMASK = 3, CBS_DROPDOWNLIST = 3,
	TBS_VERT = 2, SBS_VERT = 1,
};
enum : int {
	BN_CLICKED = 0, LBN_SELCHANGE = 1, LBN_DBLCLK = 2, CBN_SELCHANGE = 1, CBN_EDITCHANGE = 5, EN_CHANGE = 0x300,
	SB_THUMBTRACK = 5, SB_THUMBPOSITION = 4, SB_ENDSCROLL = 8,
	LB_ERR = -1, IDOK = 1, IDCANCEL = 2,
};
enum : uint32_t {
	WM_SETTEXT = 0xC, WM_GETTEXT = 0xD, WM_GETTEXTLENGTH = 0xE,
	BM_GETCHECK = 0xF0, BM_SETCHECK = 0xF1, BM_GETSTATE = 0xF2, BM_CLICK = 0xF5,
	EM_LIMITTEXT = 0xC5, EM_SETREADONLY = 0xCF,
	LB_ADDSTRING = 0x180, LB_INSERTSTRING = 0x181, LB_DELETESTRING = 0x182, LB_RESETCONTENT = 0x184,
	LB_SETSEL = 0x185, LB_SETCURSEL = 0x186, LB_GETSEL = 0x187, LB_GETCURSEL = 0x188, LB_GETTEXT = 0x189,
	LB_GETTEXTLEN = 0x18A, LB_GETCOUNT = 0x18B, LB_SELECTSTRING = 0x18C, LB_GETTOPINDEX = 0x18E,
	LB_FINDSTRING = 0x18F, LB_GETSELCOUNT = 0x190, LB_SETTOPINDEX = 0x197, LB_GETITEMDATA = 0x199,
	LB_SETITEMDATA = 0x19A, LB_SETITEMHEIGHT = 0x1A0, LB_GETITEMHEIGHT = 0x1A1, LB_FINDSTRINGEXACT = 0x1A2,
	CB_LIMITTEXT = 0x141, CB_ADDSTRING = 0x143, CB_DELETESTRING = 0x144, CB_GETCOUNT = 0x146, CB_GETCURSEL = 0x147,
	CB_GETLBTEXT = 0x148, CB_GETLBTEXTLEN = 0x149, CB_INSERTSTRING = 0x14A, CB_RESETCONTENT = 0x14B,
	CB_FINDSTRING = 0x14C, CB_SELECTSTRING = 0x14D, CB_SETCURSEL = 0x14E, CB_SHOWDROPDOWN = 0x14F,
	CB_GETITEMDATA = 0x150, CB_SETITEMDATA = 0x151, CB_FINDSTRINGEXACT = 0x158,
	SBM_SETPOS = 0xE0, SBM_GETPOS = 0xE1, SBM_SETRANGE = 0xE2, SBM_GETRANGE = 0xE3, SBM_SETRANGEREDRAW = 0xE6,
	TBM_GETPOS = 0x400, TBM_GETRANGEMIN = 0x401, TBM_GETRANGEMAX = 0x402, TBM_SETPOS = 0x405,
	TBM_SETRANGE = 0x406, TBM_SETRANGEMIN = 0x407, TBM_SETRANGEMAX = 0x408,
	PBM_SETRANGE = 0x401, PBM_SETPOS = 0x402, PBM_SETRANGE32 = 0x406, PBM_GETPOS = 0x408,
};

enum class Kind { None, Push, Check, Radio, Group, Label, Image, Separator, Entry, MultiEdit, List, Combo, Scale, ScrollBar, Progress };

struct Control {
	int id = -1;
	Kind kind = Kind::None;
	uint32_t style = 0;
	GtkWidget *widget = nullptr;   // the widget that is placed (may be a scroller)
	GtkWidget *inner = nullptr;    // the widget that holds the state
	GtkListStore *store = nullptr; // list box rows: 0 = text, 1 = item data
	std::vector<int64_t> comboData;
	bool autoToggle = true;
	bool notify = true;
	int progressMin = 0, progressMax = 100;
	int px = 0, py = 0, pw = 0, ph = 0; // template rectangle in pixels
};

struct Dialog {
	KR2DlgHost *host = nullptr;
	GtkWidget *window = nullptr;
	std::vector<std::unique_ptr<Control>> controls;
	bool ended = false;
	int result = IDCANCEL;
	int suppress = 0;     // >0 while the script changes state (no notifications)
	bool initDone = false;
};

std::vector<Dialog *> g_dialogs;

Dialog *active() { return g_dialogs.empty() ? nullptr : g_dialogs.back(); }

Control *find(Dialog *d, int id) {
	if (!d) return nullptr;
	for (auto &c : d->controls)
		if (c->id == id) return c.get();
	return nullptr;
}

struct Suppress {
	Dialog *d;
	explicit Suppress(Dialog *dd) : d(dd) { if (d) ++d->suppress; }
	~Suppress() { if (d) --d->suppress; }
};

bool canNotify(Dialog *d) { return d && d->initDone && !d->suppress && !d->ended; }

// "&OK" -> "_OK", "&&" -> "&", "_" -> "__"
std::string mnemonic(const std::string &s) {
	std::string out;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '&') {
			if (i + 1 < s.size() && s[i + 1] == '&') { out += '&'; ++i; }
			else out += '_';
		} else if (s[i] == '_') out += "__";
		else out += s[i];
	}
	return out;
}

std::string plainText(const std::string &s) {
	std::string out;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '&') {
			if (i + 1 < s.size() && s[i + 1] == '&') { out += '&'; ++i; }
		} else out += s[i];
	}
	return out;
}

int lowWord(int64_t v) { return (int)(int16_t)(v & 0xFFFF); }
int highWord(int64_t v) { return (int)(int16_t)((v >> 16) & 0xFFFF); }

// ---- list helpers ---------------------------------------------------------------
int listCount(Control *c) {
	return c->store ? gtk_tree_model_iter_n_children(GTK_TREE_MODEL(c->store), nullptr) : 0;
}

bool listIter(Control *c, int index, GtkTreeIter *it) {
	return c->store && index >= 0 && gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(c->store), it, nullptr, index);
}

std::string listText(Control *c, int index) {
	GtkTreeIter it;
	if (!listIter(c, index, &it)) return std::string();
	gchar *s = nullptr;
	gtk_tree_model_get(GTK_TREE_MODEL(c->store), &it, 0, &s, -1);
	std::string r = s ? s : "";
	g_free(s);
	return r;
}

GtkTreeSelection *listSelection(Control *c) {
	return gtk_tree_view_get_selection(GTK_TREE_VIEW(c->inner));
}

int listCurSel(Control *c) {
	GtkTreeModel *model = nullptr;
	GList *rows = gtk_tree_selection_get_selected_rows(listSelection(c), &model);
	int r = LB_ERR;
	if (rows) r = gtk_tree_path_get_indices((GtkTreePath *)rows->data)[0];
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	return r;
}

void listScrollTo(Control *c, int index) {
	if (index < 0 || index >= listCount(c) || !gtk_widget_get_realized(c->inner)) return;
	GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(c->inner), path, nullptr, FALSE, 0, 0);
	gtk_tree_path_free(path);
}

int listFind(Control *c, int start, const std::string &text, bool exact) {
	const int n = listCount(c);
	if (n == 0) return LB_ERR;
	std::string want = text;
	gchar *wf = g_utf8_casefold(want.c_str(), -1);
	int r = LB_ERR;
	for (int k = 0; k < n; ++k) {
		const int i = (start + 1 + k) % n;
		gchar *f = g_utf8_casefold(listText(c, i).c_str(), -1);
		const bool hit = exact ? std::strcmp(f, wf) == 0 : std::strncmp(f, wf, std::strlen(wf)) == 0;
		g_free(f);
		if (hit) { r = i; break; }
	}
	g_free(wf);
	return r;
}

// ---- combo helpers --------------------------------------------------------------
int comboCount(Control *c) {
	GtkTreeModel *m = gtk_combo_box_get_model(GTK_COMBO_BOX(c->inner));
	return m ? gtk_tree_model_iter_n_children(m, nullptr) : 0;
}

std::string comboText(Control *c, int index) {
	GtkTreeModel *m = gtk_combo_box_get_model(GTK_COMBO_BOX(c->inner));
	GtkTreeIter it;
	if (!m || index < 0 || !gtk_tree_model_iter_nth_child(m, &it, nullptr, index)) return std::string();
	gchar *s = nullptr;
	gtk_tree_model_get(m, &it, 0, &s, -1);
	std::string r = s ? s : "";
	g_free(s);
	return r;
}

int comboFind(Control *c, int start, const std::string &text, bool exact) {
	const int n = comboCount(c);
	if (n == 0) return LB_ERR;
	gchar *wf = g_utf8_casefold(text.c_str(), -1);
	int r = LB_ERR;
	for (int k = 0; k < n; ++k) {
		const int i = (start + 1 + k) % n;
		gchar *f = g_utf8_casefold(comboText(c, i).c_str(), -1);
		const bool hit = exact ? std::strcmp(f, wf) == 0 : std::strncmp(f, wf, std::strlen(wf)) == 0;
		g_free(f);
		if (hit) { r = i; break; }
	}
	g_free(wf);
	return r;
}

// ---- signal handlers ------------------------------------------------------------
struct Binding { Dialog *d; Control *c; };

void onClicked(GtkButton *, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (canNotify(b->d)) b->d->host->onCommand(b->c->id, BN_CLICKED);
}

void onToggled(GtkToggleButton *button, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (!canNotify(b->d)) return;
	if (b->c->kind == Kind::Radio && !gtk_toggle_button_get_active(button)) return; // the deselected one
	if (!b->c->autoToggle) {
		// BS_CHECKBOX / BS_RADIOBUTTON: the script decides the new state.
		Suppress s(b->d);
		gtk_toggle_button_set_active(button, !gtk_toggle_button_get_active(button));
	}
	b->d->host->onCommand(b->c->id, BN_CLICKED);
}

void onListChanged(GtkTreeSelection *, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (canNotify(b->d) && b->c->notify) b->d->host->onCommand(b->c->id, LBN_SELCHANGE);
}

void onListActivated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (canNotify(b->d) && b->c->notify) b->d->host->onCommand(b->c->id, LBN_DBLCLK);
}

void onComboChanged(GtkComboBox *combo, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (!canNotify(b->d)) return;
	b->d->host->onCommand(b->c->id, gtk_combo_box_get_active(combo) >= 0 ? CBN_SELCHANGE : CBN_EDITCHANGE);
}

void onEditChanged(gpointer, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (canNotify(b->d)) b->d->host->onCommand(b->c->id, EN_CHANGE);
}

void onRangeChanged(GtkRange *range, gpointer p) {
	Binding *b = static_cast<Binding *>(p);
	if (!canNotify(b->d)) return;
	const bool vertical = gtk_orientable_get_orientation(GTK_ORIENTABLE(range)) == GTK_ORIENTATION_VERTICAL;
	const int pos = (int)std::lround(gtk_range_get_value(range));
	b->d->host->onScroll(vertical, b->c->id, SB_THUMBTRACK, pos);
	b->d->host->onScroll(vertical, b->c->id, SB_THUMBPOSITION, pos);
	b->d->host->onScroll(vertical, b->c->id, SB_ENDSCROLL, pos);
}

gboolean onDelete(GtkWidget *, GdkEvent *, gpointer p) {
	Dialog *d = static_cast<Dialog *>(p);
	// Like Windows: closing the window is an IDCANCEL command.
	if (d->initDone && !d->ended) d->host->onCommand(IDCANCEL, BN_CLICKED);
	return TRUE; // the script closes the dialog (or not)
}

gboolean onKey(GtkWidget *, GdkEventKey *ev, gpointer p) {
	Dialog *d = static_cast<Dialog *>(p);
	if (ev->keyval == GDK_KEY_Escape && d->initDone && !d->ended) {
		d->host->onCommand(IDCANCEL, BN_CLICKED);
		return TRUE;
	}
	return FALSE;
}

void freeBinding(gpointer p, GClosure *) { delete static_cast<Binding *>(p); }

template <typename F>
void connect(gpointer w, const char *signal, F handler, Dialog *d, Control *c) {
	g_signal_connect_data(w, signal, G_CALLBACK(handler), new Binding{d, c}, freeBinding, GConnectFlags(0));
}

std::string classNameLower(const KR2DlgItem &item) {
	std::string s = item.className;
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
	return s;
}

int classAtomOf(const KR2DlgItem &item) {
	if (item.classAtom) return item.classAtom;
	const std::string s = classNameLower(item);
	if (s == "button") return 0x80;
	if (s == "edit") return 0x81;
	if (s == "static") return 0x82;
	if (s == "listbox") return 0x83;
	if (s == "scrollbar") return 0x84;
	if (s == "combobox") return 0x85;
	return 0;
}

GtkWidget *makeScrolled(GtkWidget *child) {
	GtkWidget *sw = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(sw), child);
	return sw;
}

void buildControl(Dialog *d, const KR2DlgItem &item, Control *c, GSList **radioGroup) {
	c->id = item.id;
	c->style = item.style;
	const std::string title = item.title;
	const int atom = classAtomOf(item);
	const std::string cls = classNameLower(item);
	if (atom == 0x80) {
		const uint32_t type = item.style & BS_TYPEMASK;
		if (type == BS_GROUPBOX) {
			c->kind = Kind::Group;
			c->widget = c->inner = gtk_frame_new(title.empty() ? nullptr : plainText(title).c_str());
		} else if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE) {
			c->kind = Kind::Check;
			c->autoToggle = type == BS_AUTOCHECKBOX || type == BS_AUTO3STATE;
			c->widget = c->inner = gtk_check_button_new_with_mnemonic(mnemonic(title).c_str());
			connect(c->inner, "toggled", onToggled, d, c);
		} else if (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON) {
			c->kind = Kind::Radio;
			c->autoToggle = type == BS_AUTORADIOBUTTON;
			if (item.style & WS_GROUP) *radioGroup = nullptr;
			c->widget = c->inner = gtk_radio_button_new_with_mnemonic(*radioGroup, mnemonic(title).c_str());
			*radioGroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(c->inner));
			connect(c->inner, "toggled", onToggled, d, c);
		} else {
			c->kind = Kind::Push;
			c->widget = c->inner = gtk_button_new_with_mnemonic(mnemonic(title).c_str());
			if (type == BS_DEFPUSHBUTTON) {
				gtk_widget_set_can_default(c->inner, TRUE);
				gtk_window_set_default(GTK_WINDOW(d->window), c->inner);
			}
			connect(c->inner, "clicked", onClicked, d, c);
		}
	} else if (atom == 0x81) {
		if (item.style & ES_MULTILINE) {
			c->kind = Kind::MultiEdit;
			c->inner = gtk_text_view_new();
			gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(c->inner), GTK_WRAP_WORD_CHAR);
			gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(c->inner)), title.c_str(), -1);
			gtk_text_view_set_editable(GTK_TEXT_VIEW(c->inner), !(item.style & ES_READONLY));
			c->widget = makeScrolled(c->inner);
			connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(c->inner)), "changed", onEditChanged, d, c);
		} else {
			c->kind = Kind::Entry;
			c->widget = c->inner = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(c->inner), title.c_str());
			gtk_entry_set_activates_default(GTK_ENTRY(c->inner), TRUE);
			if (item.style & ES_PASSWORD) gtk_entry_set_visibility(GTK_ENTRY(c->inner), FALSE);
			if (item.style & ES_READONLY) gtk_editable_set_editable(GTK_EDITABLE(c->inner), FALSE);
			gtk_entry_set_alignment(GTK_ENTRY(c->inner), (item.style & 3) == ES_CENTER ? 0.5f : (item.style & 3) == ES_RIGHT ? 1.f : 0.f);
			gtk_entry_set_width_chars(GTK_ENTRY(c->inner), 1);
			connect(c->inner, "changed", onEditChanged, d, c);
		}
	} else if (atom == 0x82) {
		const uint32_t type = item.style & SS_TYPEMASK;
		if (type == SS_ICON || type == SS_BITMAP) {
			c->kind = Kind::Image;
			c->widget = c->inner = gtk_image_new();
		} else if (type == SS_ETCHEDHORZ || type == SS_ETCHEDVERT) {
			c->kind = Kind::Separator;
			c->widget = c->inner = gtk_separator_new(type == SS_ETCHEDVERT ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
		} else if ((type >= 4 && type <= 9) || type == SS_ETCHEDFRAME) {
			c->kind = Kind::Group; // frames and rectangles
			c->widget = c->inner = gtk_frame_new(nullptr);
		} else {
			c->kind = Kind::Label;
			c->widget = c->inner = gtk_label_new(plainText(title).c_str());
			const bool wrap = type != SS_SIMPLE && type != SS_LEFTNOWORDWRAP;
			gtk_label_set_line_wrap(GTK_LABEL(c->inner), wrap);
			gtk_label_set_line_wrap_mode(GTK_LABEL(c->inner), PANGO_WRAP_WORD_CHAR);
			gtk_label_set_xalign(GTK_LABEL(c->inner), type == SS_CENTER ? 0.5f : type == SS_RIGHT ? 1.f : 0.f);
			gtk_label_set_yalign(GTK_LABEL(c->inner), (item.style & SS_CENTERIMAGE) ? 0.5f : 0.f);
		}
	} else if (atom == 0x83) {
		c->kind = Kind::List;
		c->notify = (item.style & LBS_NOTIFY) != 0;
		c->store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT64);
		c->inner = gtk_tree_view_new_with_model(GTK_TREE_MODEL(c->store));
		g_object_unref(c->store); // owned by the view
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(c->inner), FALSE);
		gtk_tree_view_set_enable_search(GTK_TREE_VIEW(c->inner), TRUE);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(c->inner), -1, "", gtk_cell_renderer_text_new(), "text", 0, nullptr);
		GtkSelectionMode mode = GTK_SELECTION_SINGLE;
		if (item.style & LBS_NOSEL) mode = GTK_SELECTION_NONE;
		else if (item.style & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) mode = GTK_SELECTION_MULTIPLE;
		gtk_tree_selection_set_mode(listSelection(c), mode);
		c->widget = makeScrolled(c->inner);
		connect(listSelection(c), "changed", onListChanged, d, c);
		connect(c->inner, "row-activated", onListActivated, d, c);
	} else if (atom == 0x85) {
		c->kind = Kind::Combo;
		const bool dropList = (item.style & CBS_TYPEMASK) == CBS_DROPDOWNLIST;
		c->widget = c->inner = dropList ? gtk_combo_box_text_new() : gtk_combo_box_text_new_with_entry();
		if (!dropList) gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(c->inner))), title.c_str());
		connect(c->inner, "changed", onComboChanged, d, c);
	} else if (atom == 0x84 || cls == "msctls_trackbar32") {
		const bool vertical = atom == 0x84 ? (item.style & SBS_VERT) != 0 : (item.style & TBS_VERT) != 0;
		const GtkOrientation o = vertical ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
		GtkAdjustment *adj = gtk_adjustment_new(0, 0, 100, 1, 10, 0);
		if (atom == 0x84) {
			c->kind = Kind::ScrollBar;
			c->widget = c->inner = gtk_scrollbar_new(o, adj);
		} else {
			c->kind = Kind::Scale;
			c->widget = c->inner = gtk_scale_new(o, adj);
			gtk_scale_set_draw_value(GTK_SCALE(c->inner), FALSE);
			gtk_scale_set_digits(GTK_SCALE(c->inner), 0);
		}
		connect(c->inner, "value-changed", onRangeChanged, d, c);
	} else if (cls == "msctls_progress32") {
		c->kind = Kind::Progress;
		c->widget = c->inner = gtk_progress_bar_new();
	} else {
		// SysListView32, up-down buttons, ...: not emulated.
		c->kind = Kind::None;
		c->widget = c->inner = gtk_label_new("");
	}
	if (item.style & WS_DISABLED) gtk_widget_set_sensitive(c->widget, FALSE);
}

// Dialog units -> pixels from the dialog font, like MapDialogRect().
void dialogBaseUnits(GtkWidget *window, int pointSize, double &bx, double &by) {
	PangoContext *ctx = gtk_widget_get_pango_context(window);
	PangoFontDescription *desc = pango_font_description_copy(pango_context_get_font_description(ctx));
	if (pointSize > 0) pango_font_description_set_size(desc, pointSize * PANGO_SCALE);
	PangoFontMetrics *m = pango_context_get_metrics(ctx, desc, nullptr);
	bx = std::max(5.0, pango_font_metrics_get_approximate_char_width(m) / (double)PANGO_SCALE);
	by = std::max(10.0, (pango_font_metrics_get_ascent(m) + pango_font_metrics_get_descent(m)) / (double)PANGO_SCALE);
	pango_font_metrics_unref(m);
	pango_font_description_free(desc);
	// Windows' base unit is the average of "a..zA..Z"; Pango's approximate
	// width is close.  Leave room for GTK's larger widget padding.
	bx *= 1.15;
	by *= 1.1;
}

bool gtkReady() {
	static int state = -1;
	if (state < 0) state = gtk_init_check(nullptr, nullptr) ? 1 : 0;
	return state == 1;
}

const char *kCss =
	".kr2-win32dialog button { padding: 1px 6px; min-height: 0; min-width: 0; }"
	".kr2-win32dialog entry { padding: 1px 4px; min-height: 0; }"
	".kr2-win32dialog combobox button { padding: 0 4px; }"
	".kr2-win32dialog treeview { padding: 0; }";

} // namespace

int KR2Win32DialogRun(const KR2DlgTemplate &t, KR2DlgHost &host, int *pixelWidth, int *pixelHeight) {
	if (!gtkReady()) return -2;
	Dialog d;
	d.host = &host;
	d.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWindow *win = GTK_WINDOW(d.window);
	gtk_window_set_title(win, plainText(t.title).c_str());
	gtk_window_set_modal(win, TRUE);
	gtk_window_set_keep_above(win, TRUE);
	gtk_window_set_resizable(win, FALSE);
	gtk_window_set_type_hint(win, GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(win, GTK_WIN_POS_CENTER);
	gtk_style_context_add_class(gtk_widget_get_style_context(d.window), "kr2-win32dialog");
	GtkCssProvider *css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css, kCss, -1, nullptr);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(d.window), GTK_STYLE_PROVIDER(css),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_signal_connect(d.window, "delete-event", G_CALLBACK(onDelete), &d);
	g_signal_connect(d.window, "key-press-event", G_CALLBACK(onKey), &d);

	double bx, by;
	dialogBaseUnits(d.window, t.pointSize, bx, by);
	auto px = [](int dlu, double base, int div) { return (int)std::lround(dlu * base / div); };

	GtkWidget *fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(d.window), fixed);
	GSList *radioGroup = nullptr;
	int needW = px(t.cx, bx, 4), needH = px(t.cy, by, 8);
	for (const KR2DlgItem &item : t.items) {
		std::unique_ptr<Control> c(new Control());
		buildControl(&d, item, c.get(), &radioGroup);
		const int x = px(item.x, bx, 4), y = px(item.y, by, 8);
		int w = std::max(1, px(item.cx, bx, 4)), h = std::max(1, px(item.cy, by, 8));
		c->px = x; c->py = y; c->pw = w; c->ph = h;
		if (c->kind == Kind::Combo) h = -1; // the template height includes the drop-down list
		gtk_widget_set_size_request(c->widget, w, h);
		gtk_fixed_put(GTK_FIXED(fixed), c->widget, x, y);
		if (!(item.style & WS_VISIBLE)) gtk_widget_set_no_show_all(c->widget, TRUE);
		needW = std::max(needW, x + w);
		needH = std::max(needH, y + std::max(h, 24));
		d.controls.push_back(std::move(c));
	}
	gtk_widget_set_size_request(fixed, needW + 4, needH + 4);
	if (pixelWidth) *pixelWidth = needW + 4;
	if (pixelHeight) *pixelHeight = needH + 4;

	g_dialogs.push_back(&d);
	gtk_widget_show_all(d.window);
	host.onInit();
	d.initDone = true;
	while (!d.ended) gtk_main_iteration();
	g_dialogs.pop_back();
	gtk_widget_destroy(d.window);
	g_object_unref(css);
	while (gtk_events_pending()) gtk_main_iteration();
	return d.result;
}

bool KR2Win32DialogActive() { return active() != nullptr; }

void KR2Win32DialogEnd(int result) {
	if (Dialog *d = active()) {
		d->result = result;
		d->ended = true;
	}
}

int64_t KR2Win32DialogSend(int id, uint32_t msg, int64_t wp, int64_t lp, const std::string *lpText, std::string *outText, bool *outIsText) {
	if (outIsText) *outIsText = false;
	Dialog *d = active();
	Control *c = find(d, id);
	if (!c) return 0;
	Suppress quiet(d);
	const std::string text = lpText ? *lpText : std::to_string(lp);
	auto giveText = [&](const std::string &s) -> int64_t {
		if (outText) *outText = s;
		if (outIsText) *outIsText = true;
		return (int64_t)s.size();
	};
	switch (c->kind) {
	case Kind::Check:
	case Kind::Radio:
		if (msg == BM_GETCHECK || msg == BM_GETSTATE)
			return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(c->inner)) ? 1 : 0;
		if (msg == BM_SETCHECK) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c->inner), wp != 0); return 0; }
		if (msg == BM_CLICK) { --d->suppress; gtk_button_clicked(GTK_BUTTON(c->inner)); ++d->suppress; return 0; }
		break;
	case Kind::Push:
		if (msg == BM_CLICK) { --d->suppress; gtk_button_clicked(GTK_BUTTON(c->inner)); ++d->suppress; return 0; }
		break;
	case Kind::List: {
		GtkTreeIter it;
		switch (msg) {
		case LB_ADDSTRING:
			gtk_list_store_append(c->store, &it);
			gtk_list_store_set(c->store, &it, 0, text.c_str(), 1, (gint64)(lpText ? 0 : lp), -1);
			return listCount(c) - 1;
		case LB_INSERTSTRING: {
			const int index = (int)wp < 0 || (int)wp > listCount(c) ? listCount(c) : (int)wp;
			gtk_list_store_insert(c->store, &it, index);
			gtk_list_store_set(c->store, &it, 0, text.c_str(), 1, (gint64)(lpText ? 0 : lp), -1);
			return index;
		}
		case LB_DELETESTRING:
			if (!listIter(c, (int)wp, &it)) return LB_ERR;
			gtk_list_store_remove(c->store, &it);
			return listCount(c);
		case LB_RESETCONTENT: gtk_list_store_clear(c->store); return 0;
		case LB_GETCOUNT: return listCount(c);
		case LB_GETCURSEL: return listCurSel(c);
		case LB_SETCURSEL:
			gtk_tree_selection_unselect_all(listSelection(c));
			if (!listIter(c, (int)wp, &it)) return LB_ERR;
			gtk_tree_selection_select_iter(listSelection(c), &it);
			listScrollTo(c, (int)wp);
			return wp;
		case LB_SETSEL:
			if ((int)lp < 0) {
				if (wp) gtk_tree_selection_select_all(listSelection(c));
				else gtk_tree_selection_unselect_all(listSelection(c));
				return 0;
			}
			if (!listIter(c, (int)lp, &it)) return LB_ERR;
			if (wp) gtk_tree_selection_select_iter(listSelection(c), &it);
			else gtk_tree_selection_unselect_iter(listSelection(c), &it);
			return 0;
		case LB_GETSEL:
			if (!listIter(c, (int)wp, &it)) return LB_ERR;
			return gtk_tree_selection_iter_is_selected(listSelection(c), &it) ? 1 : 0;
		case LB_GETSELCOUNT: return gtk_tree_selection_count_selected_rows(listSelection(c));
		case LB_GETTEXT:
			if ((int)wp < 0 || (int)wp >= listCount(c)) return LB_ERR;
			return giveText(listText(c, (int)wp));
		case LB_GETTEXTLEN:
			if ((int)wp < 0 || (int)wp >= listCount(c)) return LB_ERR;
			return (int64_t)listText(c, (int)wp).size();
		case LB_FINDSTRING: return listFind(c, (int)wp, text, false);
		case LB_FINDSTRINGEXACT: return listFind(c, (int)wp, text, true);
		case LB_SELECTSTRING: {
			const int i = listFind(c, (int)wp, text, false);
			if (i >= 0 && listIter(c, i, &it)) {
				gtk_tree_selection_unselect_all(listSelection(c));
				gtk_tree_selection_select_iter(listSelection(c), &it);
				listScrollTo(c, i);
			}
			return i;
		}
		case LB_GETITEMDATA: {
			if (!listIter(c, (int)wp, &it)) return LB_ERR;
			gint64 v = 0;
			gtk_tree_model_get(GTK_TREE_MODEL(c->store), &it, 1, &v, -1);
			return v;
		}
		case LB_SETITEMDATA:
			if (!listIter(c, (int)wp, &it)) return LB_ERR;
			gtk_list_store_set(c->store, &it, 1, (gint64)lp, -1);
			return 0;
		case LB_SETTOPINDEX: listScrollTo(c, (int)wp); return 0;
		case LB_GETTOPINDEX: return 0;
		case LB_SETITEMHEIGHT: return 0;
		case LB_GETITEMHEIGHT: return 18;
		}
		break;
	}
	case Kind::Combo: {
		GtkComboBoxText *cb = GTK_COMBO_BOX_TEXT(c->inner);
		switch (msg) {
		case CB_ADDSTRING:
			gtk_combo_box_text_append_text(cb, text.c_str());
			c->comboData.push_back(lpText ? 0 : lp);
			return comboCount(c) - 1;
		case CB_INSERTSTRING: {
			const int index = (int)wp < 0 || (int)wp > comboCount(c) ? comboCount(c) : (int)wp;
			gtk_combo_box_text_insert_text(cb, index, text.c_str());
			c->comboData.insert(c->comboData.begin() + std::min<size_t>(index, c->comboData.size()), lpText ? 0 : lp);
			return index;
		}
		case CB_DELETESTRING:
			if ((int)wp < 0 || (int)wp >= comboCount(c)) return LB_ERR;
			gtk_combo_box_text_remove(cb, (int)wp);
			if ((size_t)wp < c->comboData.size()) c->comboData.erase(c->comboData.begin() + (int)wp);
			return comboCount(c);
		case CB_RESETCONTENT: gtk_combo_box_text_remove_all(cb); c->comboData.clear(); return 0;
		case CB_GETCOUNT: return comboCount(c);
		case CB_GETCURSEL: return gtk_combo_box_get_active(GTK_COMBO_BOX(cb));
		case CB_SETCURSEL:
			if ((int)wp >= comboCount(c)) return LB_ERR;
			gtk_combo_box_set_active(GTK_COMBO_BOX(cb), (int)wp);
			return wp;
		case CB_GETLBTEXT:
			if ((int)wp < 0 || (int)wp >= comboCount(c)) return LB_ERR;
			return giveText(comboText(c, (int)wp));
		case CB_GETLBTEXTLEN:
			if ((int)wp < 0 || (int)wp >= comboCount(c)) return LB_ERR;
			return (int64_t)comboText(c, (int)wp).size();
		case CB_FINDSTRING: return comboFind(c, (int)wp, text, false);
		case CB_FINDSTRINGEXACT: return comboFind(c, (int)wp, text, true);
		case CB_SELECTSTRING: {
			const int i = comboFind(c, (int)wp, text, false);
			if (i >= 0) gtk_combo_box_set_active(GTK_COMBO_BOX(cb), i);
			return i;
		}
		case CB_GETITEMDATA: return (size_t)wp < c->comboData.size() ? c->comboData[(size_t)wp] : (int64_t)LB_ERR;
		case CB_SETITEMDATA:
			if ((size_t)wp >= c->comboData.size()) return LB_ERR;
			c->comboData[(size_t)wp] = lp;
			return 0;
		case CB_SHOWDROPDOWN:
			if (wp) gtk_combo_box_popup(GTK_COMBO_BOX(cb)); else gtk_combo_box_popdown(GTK_COMBO_BOX(cb));
			return 1;
		case CB_LIMITTEXT: return 1;
		}
		break;
	}
	case Kind::Scale:
	case Kind::ScrollBar: {
		GtkRange *r = GTK_RANGE(c->inner);
		GtkAdjustment *a = gtk_range_get_adjustment(r);
		switch (msg) {
		case TBM_GETPOS:
			if (c->kind == Kind::Scale) return (int64_t)std::lround(gtk_range_get_value(r));
			break;
		case SBM_GETPOS: return (int64_t)std::lround(gtk_range_get_value(r));
		case TBM_SETPOS: if (c->kind == Kind::Scale) { gtk_range_set_value(r, (double)lp); return 0; } break;
		case SBM_SETPOS: { const int64_t old = (int64_t)std::lround(gtk_range_get_value(r)); gtk_range_set_value(r, (double)wp); return old; }
		case TBM_SETRANGE: if (c->kind == Kind::Scale) { gtk_range_set_range(r, lowWord(lp), highWord(lp)); return 0; } break;
		case SBM_SETRANGE: case SBM_SETRANGEREDRAW: gtk_range_set_range(r, (double)wp, (double)lp); return 1;
		case TBM_SETRANGEMIN: if (c->kind == Kind::Scale) { gtk_range_set_range(r, (double)lp, gtk_adjustment_get_upper(a)); return 0; } break;
		case TBM_SETRANGEMAX: if (c->kind == Kind::Scale) { gtk_range_set_range(r, gtk_adjustment_get_lower(a), (double)lp); return 0; } break;
		case TBM_GETRANGEMIN: return (int64_t)gtk_adjustment_get_lower(a);
		case TBM_GETRANGEMAX: return (int64_t)gtk_adjustment_get_upper(a);
		}
		break;
	}
	case Kind::Progress: {
		auto update = [c](int64_t pos) {
			const double span = std::max(1, c->progressMax - c->progressMin);
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(c->inner), std::min(1.0, std::max(0.0, (pos - c->progressMin) / span)));
		};
		if (msg == PBM_SETRANGE) { c->progressMin = lowWord(lp); c->progressMax = highWord(lp); return 0; }
		if (msg == PBM_SETRANGE32) { c->progressMin = (int)wp; c->progressMax = (int)lp; return 0; }
		if (msg == PBM_SETPOS) { update(wp); return 0; }
		break;
	}
	default:
		break;
	}
	if (msg == WM_GETTEXT) {
		std::string s;
		KR2Win32DialogGetText(id, s);
		return giveText(s);
	}
	if (msg == WM_SETTEXT) return KR2Win32DialogSetText(id, text) ? 1 : 0;
	if (msg == WM_GETTEXTLENGTH) {
		std::string s;
		KR2Win32DialogGetText(id, s);
		return (int64_t)s.size();
	}
	return 0;
}

bool KR2Win32DialogGetText(int id, std::string &out) {
	Control *c = find(active(), id);
	if (!c) return false;
	switch (c->kind) {
	case Kind::Entry: out = gtk_entry_get_text(GTK_ENTRY(c->inner)); return true;
	case Kind::MultiEdit: {
		GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(c->inner));
		GtkTextIter s, e;
		gtk_text_buffer_get_bounds(b, &s, &e);
		gchar *t = gtk_text_buffer_get_text(b, &s, &e, FALSE);
		out = t ? t : "";
		g_free(t);
		return true;
	}
	case Kind::Combo: {
		gchar *t = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(c->inner));
		out = t ? t : "";
		g_free(t);
		return true;
	}
	case Kind::Label: out = gtk_label_get_text(GTK_LABEL(c->inner)); return true;
	case Kind::Push: case Kind::Check: case Kind::Radio: {
		const gchar *t = gtk_button_get_label(GTK_BUTTON(c->inner));
		out = t ? t : "";
		return true;
	}
	case Kind::Group: {
		const gchar *t = gtk_frame_get_label(GTK_FRAME(c->inner));
		out = t ? t : "";
		return true;
	}
	case Kind::List: { const int i = listCurSel(c); out = i >= 0 ? listText(c, i) : ""; return true; }
	default: out.clear(); return true;
	}
}

bool KR2Win32DialogSetText(int id, const std::string &text) {
	Dialog *d = active();
	Control *c = find(d, id);
	if (!c) return false;
	Suppress quiet(d);
	switch (c->kind) {
	case Kind::Entry: gtk_entry_set_text(GTK_ENTRY(c->inner), text.c_str()); return true;
	case Kind::MultiEdit: gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(c->inner)), text.c_str(), -1); return true;
	case Kind::Combo: {
		GtkWidget *entry = gtk_bin_get_child(GTK_BIN(c->inner));
		if (entry && GTK_IS_ENTRY(entry)) gtk_entry_set_text(GTK_ENTRY(entry), text.c_str());
		else {
			const int i = comboFind(c, -1, text, true);
			if (i >= 0) gtk_combo_box_set_active(GTK_COMBO_BOX(c->inner), i);
		}
		return true;
	}
	case Kind::Label: gtk_label_set_text(GTK_LABEL(c->inner), plainText(text).c_str()); return true;
	case Kind::Push: case Kind::Check: case Kind::Radio: gtk_button_set_label(GTK_BUTTON(c->inner), mnemonic(text).c_str()); return true;
	case Kind::Group: gtk_frame_set_label(GTK_FRAME(c->inner), plainText(text).c_str()); return true;
	default: return true;
	}
}

bool KR2Win32DialogSetEnabled(int id, bool enabled) {
	Control *c = find(active(), id);
	if (!c) return false;
	gtk_widget_set_sensitive(c->widget, enabled);
	return true;
}

bool KR2Win32DialogGetEnabled(int id, bool &enabled) {
	Control *c = find(active(), id);
	if (!c) return false;
	enabled = gtk_widget_get_sensitive(c->widget);
	return true;
}

bool KR2Win32DialogSetFocus(int id) {
	Control *c = find(active(), id);
	if (!c) return false;
	gtk_widget_grab_focus(c->inner);
	return true;
}

bool KR2Win32DialogGetRect(int id, int &x, int &y, int &width, int &height) {
	Control *c = find(active(), id);
	if (!c) return false;
	x = c->px; y = c->py; width = c->pw; height = c->ph;
	GtkAllocation a;
	gtk_widget_get_allocation(c->widget, &a);
	if (gtk_widget_get_realized(c->widget) && a.width > 1 && a.height > 1) {
		x = a.x; y = a.y; width = a.width; height = a.height;
	}
	return true;
}

bool KR2Win32DialogSetImage(int id, int width, int height, const uint8_t *argb, int pitch) {
	Control *c = find(active(), id);
	if (!c || c->kind != Kind::Image || width <= 0 || height <= 0 || !argb) return false;
	GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
	guchar *dst = gdk_pixbuf_get_pixels(pb);
	const int dstride = gdk_pixbuf_get_rowstride(pb);
	for (int y = 0; y < height; ++y) {
		const uint8_t *s = argb + (size_t)y * pitch;
		guchar *o = dst + (size_t)y * dstride;
		for (int x = 0; x < width; ++x, s += 4, o += 4) {
			// little-endian 0xAARRGGBB: B, G, R, A in memory
			o[0] = s[2]; o[1] = s[1]; o[2] = s[0]; o[3] = s[3];
		}
	}
	gtk_image_set_from_pixbuf(GTK_IMAGE(c->inner), pb);
	g_object_unref(pb);
	return true;
}

#else // !KR2_LINUX_HAVE_GTK3

int KR2Win32DialogRun(const KR2DlgTemplate &, KR2DlgHost &, int *, int *) { return -2; }
bool KR2Win32DialogActive() { return false; }
void KR2Win32DialogEnd(int) {}
int64_t KR2Win32DialogSend(int, uint32_t, int64_t, int64_t, const std::string *, std::string *, bool *outIsText) {
	if (outIsText) *outIsText = false;
	return 0;
}
bool KR2Win32DialogGetText(int, std::string &out) { out.clear(); return false; }
bool KR2Win32DialogSetText(int, const std::string &) { return false; }
bool KR2Win32DialogSetEnabled(int, bool) { return false; }
bool KR2Win32DialogGetEnabled(int, bool &) { return false; }
bool KR2Win32DialogSetFocus(int) { return false; }
bool KR2Win32DialogGetRect(int, int &, int &, int &, int &) { return false; }
bool KR2Win32DialogSetImage(int, int, int, const uint8_t *, int) { return false; }

#endif