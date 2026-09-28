//---------------------------------------------------------------------------
// kirikinux2 : native message/input boxes for TVPShowSimpleMessageBox /
// TVPShowSimpleInputBox (Android uses AlertDialogs through JNI).
// Isolated from the krkr headers on purpose.  Returns -2 if no dialog could be
// shown (no GTK3 at build time or no display).
//---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <cstdio>

#ifdef KR2_LINUX_HAVE_GTK3
#include <gtk/gtk.h>

static bool _GtkReady() {
	static int state = -1;
	if (state < 0) {
		gtk_disable_setlocale(); // TJS2 and the engine rely on the "C" numeric locale
		state = gtk_init_check(nullptr, nullptr) ? 1 : 0;
	}
	return state == 1;
}

static void _GtkFlush() {
	while (gtk_events_pending()) gtk_main_iteration();
}

static void _AddButtons(GtkDialog *dlg, const std::vector<std::string> &buttons) {
	if (buttons.empty()) gtk_dialog_add_button(dlg, "OK", 0);
	for (size_t i = 0; i < buttons.size(); ++i)
		gtk_dialog_add_button(dlg, buttons[i].c_str(), (gint)i);
	gtk_dialog_set_default_response(dlg, 0);
}

int KR2LinuxMessageBox(const std::string &text, const std::string &caption, const std::vector<std::string> &buttons) {
	fprintf(stderr, "kirikiroid2: dialog [%s]: %s\n", caption.c_str(), text.c_str());
	if (!_GtkReady()) return -2;
	GtkWidget *dlg = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, "%s", text.c_str());
	gtk_window_set_title(GTK_WINDOW(dlg), caption.c_str());
	gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
	_AddButtons(GTK_DIALOG(dlg), buttons);
	gint r = gtk_dialog_run(GTK_DIALOG(dlg));
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

#else // !KR2_LINUX_HAVE_GTK3

int KR2LinuxMessageBox(const std::string &, const std::string &, const std::vector<std::string> &) { return -2; }
int KR2LinuxInputBox(std::string &, const std::string &, const std::string &, const std::vector<std::string> &) { return -2; }

#endif
