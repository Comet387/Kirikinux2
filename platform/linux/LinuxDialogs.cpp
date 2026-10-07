// SPDX-License-Identifier: AGPL-3.0-only
//---------------------------------------------------------------------------
// kirikinux2 : native message/input boxes for TVPShowSimpleMessageBox /
// TVPShowSimpleInputBox (Android uses AlertDialogs through JNI).
// Isolated from the krkr headers on purpose.  Returns -2 if no dialog could be
// shown (no GTK3 at build time or no display).
//---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>

#ifdef KR2_LINUX_HAVE_GTK3
#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>

static std::string _UIFont;
void KR2LinuxSetUIFont(const std::string &path) { _UIFont = path; }

static bool _GtkReady() {
	static int state = -1;
	if (state < 0) {
		if (!_UIFont.empty()) FcConfigAppFontAddFile(nullptr, reinterpret_cast<const FcChar8*>(_UIFont.c_str()));
		if (gdk_display_get_default()) {
			state = 1; // already started elsewhere (win32dialog.dll)
		} else {
			gtk_disable_setlocale(); // TJS2 and the engine rely on the "C" numeric locale
			state = gtk_init_check(nullptr, nullptr) ? 1 : 0;
		}
	}
	return state == 1;
}

// Shared GTK start-up for the other native dialogs (font picker).
bool KR2LinuxGtkInit() { return _GtkReady(); }

static void _GtkFlush() {
	while (gtk_events_pending()) gtk_main_iteration();
}

static void _AddButtons(GtkDialog *dlg, const std::vector<std::string> &buttons) {
	if (buttons.empty()) gtk_dialog_add_button(dlg, "OK", 0);
	for (size_t i = 0; i < buttons.size(); ++i)
		gtk_dialog_add_button(dlg, buttons[i].c_str(), (gint)i);
	gtk_dialog_set_default_response(dlg, 0);
}

// Characters in the longest line (UTF-8 aware); used to give wrapped labels a
// sensible width.
static int _LongestLineChars(const std::string &text) {
	int longest = 0;
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos) end = text.size();
		const std::string line = text.substr(start, end - start);
		longest = std::max(longest, (int)g_utf8_strlen(line.c_str(), -1));
		start = end + 1;
	}
	return longest;
}

static std::string _TrimTrailingSpace(std::string text) {
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
		text.pop_back();
	return text;
}

int KR2LinuxMessageBox(const std::string &rawText, const std::string &caption, const std::vector<std::string> &buttons) {
	if (!_GtkReady()) return -2;
	const std::string text = _TrimTrailingSpace(rawText);
	GtkWidget *dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), caption.c_str());
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
	GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(dlg));
	GdkRectangle workarea = {0, 0, 800, 600};
	gdk_screen_get_monitor_workarea(screen, gdk_screen_get_primary_monitor(screen), &workarea);
    const bool longText = g_utf8_strlen(text.c_str(), -1) > 360 ||
        std::count(text.begin(), text.end(), '\n') > 8;
    if (longText) gtk_window_set_default_size(GTK_WINDOW(dlg), std::min(720, std::max(280, workarea.width - 80)),
		std::min(520, std::max(180, workarea.height - 120)));
	GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	gtk_container_set_border_width(GTK_CONTAINER(area), 12);
    if (!longText) {
        GtkWidget *label = gtk_label_new(text.c_str());
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        // Japanese/Chinese text has no spaces, so a wrapping label's minimum
        // width is a single character.  GTK sizes the dialog from that minimum,
        // which produced a one-character-wide, very tall window for short
        // messages.  Give the label a real width and allow breaks anywhere.
        gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        const int chars = std::min(48, std::max(16, _LongestLineChars(text)));
        gtk_label_set_width_chars(GTK_LABEL(label), chars);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 48);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_selectable(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(GTK_BOX(area), label, FALSE, FALSE, 8);
    } else {
	GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	GtkWidget *view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 8);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 8);
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), text.c_str(), -1);
	gtk_container_add(GTK_CONTAINER(scroll), view);
	gtk_box_pack_start(GTK_BOX(area), scroll, TRUE, TRUE, 0);
    }
	// Copy never changes the caller's existing response indexes or closes the dialog.
	const gint copyResponse = 10000;
	const bool chinese = caption.find("设备") != std::string::npos || caption.find("裝置") != std::string::npos || caption.find("信息") != std::string::npos || caption.find("关于") != std::string::npos;
	if (longText) gtk_dialog_add_button(GTK_DIALOG(dlg), chinese ? "复制全部" : "Copy all", copyResponse);
	_AddButtons(GTK_DIALOG(dlg), buttons);
	gtk_widget_show_all(dlg);
	gint r;
	do {
		r = gtk_dialog_run(GTK_DIALOG(dlg));
		if (r == copyResponse) {
			GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
			gtk_clipboard_set_text(clipboard, text.c_str(), static_cast<gint>(text.size()));
			gtk_clipboard_store(clipboard);
		}
	} while (r == copyResponse);
	gtk_widget_destroy(dlg);
	_GtkFlush();
	return r >= 0 ? (int)r : -1; // closed by the window manager: like "cancel" on Android
}

int KR2LinuxInputBox(std::string &text, const std::string &caption, const std::string &prompt, const std::vector<std::string> &buttons) {
	if (!_GtkReady()) return -2;
	GtkWidget *dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), caption.c_str());
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
	GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	if (!prompt.empty()) gtk_box_pack_start(GTK_BOX(area), gtk_label_new(prompt.c_str()), FALSE, FALSE, 4);
	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), text.c_str());
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(area), entry, TRUE, TRUE, 4);
	_AddButtons(GTK_DIALOG(dlg), buttons);
	gtk_widget_show_all(dlg);
	gint r = gtk_dialog_run(GTK_DIALOG(dlg));
	if (r >= 0) text = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_widget_destroy(dlg);
	_GtkFlush();
	return r >= 0 ? (int)r : -1;
}

std::string KR2LinuxSelectGame(const std::string &initialPath, const std::vector<std::string> &labels) {
	if (!_GtkReady() || labels.size() < 6) return std::string();
	GtkWidget *mode = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE, "%s", labels[1].c_str());
	gtk_window_set_title(GTK_WINDOW(mode), labels[0].c_str());
	gtk_window_set_keep_above(GTK_WINDOW(mode), TRUE);
	gtk_dialog_add_button(GTK_DIALOG(mode), labels[2].c_str(), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(mode), labels[3].c_str(), 1);
	gtk_dialog_add_button(GTK_DIALOG(mode), labels[4].c_str(), 2);
	const gint choice = gtk_dialog_run(GTK_DIALOG(mode));
	gtk_widget_destroy(mode);
	_GtkFlush();
	if (choice != 1 && choice != 2) return std::string();
	GtkWidget *dlg = gtk_file_chooser_dialog_new(labels[0].c_str(), nullptr,
		choice == 1 ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN,
		labels[2].c_str(), GTK_RESPONSE_CANCEL, labels[5].c_str(), GTK_RESPONSE_ACCEPT, nullptr);
	gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dlg), TRUE);
	GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(dlg));
	GdkRectangle workarea = {0, 0, 800, 600};
	gdk_screen_get_monitor_workarea(screen, gdk_screen_get_primary_monitor(screen), &workarea);
	gtk_window_set_default_size(GTK_WINDOW(dlg), std::min(840, std::max(320, workarea.width - 80)),
		std::min(560, std::max(240, workarea.height - 120)));
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), initialPath.c_str());
	if (choice == 2) {
		GtkFileFilter *games = gtk_file_filter_new();
		gtk_file_filter_set_name(games, "KiriKiri (*.xp3, startup.tjs)");
		gtk_file_filter_add_pattern(games, "*.[xX][pP]3");
		gtk_file_filter_add_pattern(games, "*.[tT][jJ][sS]");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), games);
		GtkFileFilter *all = gtk_file_filter_new();
		gtk_file_filter_set_name(all, "*");
		gtk_file_filter_add_pattern(all, "*");
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), all);
	}
	std::string path;
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		if (filename) { path = filename; g_free(filename); }
	}
	gtk_widget_destroy(dlg);
	_GtkFlush();
	if (!path.empty()) std::fprintf(stderr, "Kirikinux2: selected game path: %s\n", path.c_str());
	return path;
}

#else // !KR2_LINUX_HAVE_GTK3

void KR2LinuxSetUIFont(const std::string &) {}
bool KR2LinuxGtkInit() { return false; }

int KR2LinuxMessageBox(const std::string &, const std::string &, const std::vector<std::string> &) { return -2; }
int KR2LinuxInputBox(std::string &, const std::string &, const std::string &, const std::vector<std::string> &) { return -2; }
std::string KR2LinuxSelectGame(const std::string &, const std::vector<std::string> &) { return std::string(); }

#endif
