// SPDX-License-Identifier: AGPL-3.0-only
// GTK font picker (see LinuxFontPicker.h).
#include "LinuxFontPicker.h"

#ifdef KR2_LINUX_HAVE_GTK3
#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

bool KR2LinuxGtkInit(); // LinuxDialogs.cpp (shared GTK start-up: UI font, "C" numeric locale)

namespace {

bool chineseUI() {
	for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"}) {
		const char *v = std::getenv(var);
		if (v && *v) return std::strncmp(v, "zh", 2) == 0;
	}
	return false;
}

struct Text {
	const char *title, *search, *name, *addFile, *ok, *cancel, *fileTitle, *fontFiles, *allFiles,
		*addFailed, *noFace, *sampleDefault, *hint;
};

const Text &texts() {
	static const Text zh = {
		"选择字体", "搜索字体…", "字体名：", "从文件添加…", "确定", "取消", "选择字体文件",
		"字体文件 (*.ttf, *.otf, *.ttc, *.otc)", "所有文件", "无法加载这个字体文件：", "文件里没有可用的字体。",
		"ABCDEFG abcdefg 0123 あいうえお 永字八法 字体预览",
		"从列表选择、从文件添加，或在下面直接输入字体名。",
	};
	static const Text en = {
		"Select Font", "Search fonts…", "Font name:", "Add from file…", "OK", "Cancel", "Select a font file",
		"Font files (*.ttf, *.otf, *.ttc, *.otc)", "All files", "Could not load this font file: ", "The file contains no usable font.",
		"ABCDEFG abcdefg 0123 あいうえお 永字八法",
		"Pick from the list, add a font file, or type a font name below.",
	};
	return chineseUI() ? zh : en;
}

enum { COL_NAME = 0, COL_LOWER, N_COLS };

struct Picker {
	const KR2FontPickRequest *req = nullptr;
	GtkWidget *dialog = nullptr;
	GtkWidget *search = nullptr;
	GtkWidget *view = nullptr;
	GtkWidget *nameEntry = nullptr;
	GtkWidget *preview = nullptr;
	GtkListStore *store = nullptr;
	GtkTreeModel *filter = nullptr;
	std::string sample;
	std::string file;           // last file added
	std::vector<std::string> fileFaces;
	int syncing = 0;
};

std::string lower(const char *s) {
	gchar *l = g_utf8_casefold(s ? s : "", -1);
	std::string r = l ? l : "";
	g_free(l);
	return r;
}

void updatePreview(Picker *p) {
	const char *face = gtk_entry_get_text(GTK_ENTRY(p->nameEntry));
	gchar *escaped = g_markup_escape_text(p->sample.c_str(), -1);
	gchar *family = g_markup_escape_text(face && *face ? face : "Sans", -1);
	gchar *markup = g_strdup_printf("<span font_family=\"%s\" size=\"x-large\">%s</span>", family, escaped);
	gtk_label_set_markup(GTK_LABEL(p->preview), markup);
	g_free(markup);
	g_free(family);
	g_free(escaped);
}

gboolean filterVisible(GtkTreeModel *model, GtkTreeIter *it, gpointer data) {
	Picker *p = static_cast<Picker *>(data);
	const std::string needle = lower(gtk_entry_get_text(GTK_ENTRY(p->search)));
	if (needle.empty()) return TRUE;
	gchar *l = nullptr;
	gtk_tree_model_get(model, it, COL_LOWER, &l, -1);
	const bool hit = l && std::strstr(l, needle.c_str()) != nullptr;
	g_free(l);
	return hit;
}

// Selects the visible row whose name is `face` (exact, then case-insensitive).
bool selectFace(Picker *p, const std::string &face) {
	if (face.empty()) return false;
	const std::string want = lower(face.c_str());
	GtkTreeIter it;
	bool ok = gtk_tree_model_get_iter_first(p->filter, &it);
	GtkTreeIter found;
	bool have = false;
	while (ok) {
		gchar *name = nullptr, *l = nullptr;
		gtk_tree_model_get(p->filter, &it, COL_NAME, &name, COL_LOWER, &l, -1);
		const bool exact = name && face == name;
		const bool loose = l && want == l;
		g_free(name);
		g_free(l);
		if (exact || (loose && !have)) {
			found = it;
			have = true;
			if (exact) break;
		}
		ok = gtk_tree_model_iter_next(p->filter, &it);
	}
	if (!have) return false;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(p->view));
	++p->syncing;
	gtk_tree_selection_select_iter(sel, &found);
	GtkTreePath *path = gtk_tree_model_get_path(p->filter, &found);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(p->view), path, nullptr, TRUE, 0.3f, 0.0f);
	gtk_tree_path_free(path);
	--p->syncing;
	return true;
}

void onSelectionChanged(GtkTreeSelection *sel, gpointer data) {
	Picker *p = static_cast<Picker *>(data);
	if (p->syncing) return;
	GtkTreeModel *model = nullptr;
	GtkTreeIter it;
	if (!gtk_tree_selection_get_selected(sel, &model, &it)) return;
	gchar *name = nullptr;
	gtk_tree_model_get(model, &it, COL_NAME, &name, -1);
	++p->syncing;
	gtk_entry_set_text(GTK_ENTRY(p->nameEntry), name ? name : "");
	--p->syncing;
	g_free(name);
	updatePreview(p);
}

void onNameChanged(GtkEditable *, gpointer data) {
	Picker *p = static_cast<Picker *>(data);
	updatePreview(p);
	if (p->syncing) return;
	// Typing a name: deselect the list unless the text names a listed face.
	if (!selectFace(p, gtk_entry_get_text(GTK_ENTRY(p->nameEntry)))) {
		++p->syncing;
		gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->view)));
		--p->syncing;
	}
}

void onSearchChanged(GtkEditable *, gpointer data) {
	Picker *p = static_cast<Picker *>(data);
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(p->filter));
	selectFace(p, gtk_entry_get_text(GTK_ENTRY(p->nameEntry)));
}

void onRowActivated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, gpointer data) {
	Picker *p = static_cast<Picker *>(data);
	gtk_dialog_response(GTK_DIALOG(p->dialog), GTK_RESPONSE_OK);
}

void addFace(Picker *p, const std::string &face, bool front) {
	GtkTreeIter it;
	// already listed?
	bool ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(p->store), &it);
	while (ok) {
		gchar *name = nullptr;
		gtk_tree_model_get(GTK_TREE_MODEL(p->store), &it, COL_NAME, &name, -1);
		const bool same = name && face == name;
		g_free(name);
		if (same) return;
		ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(p->store), &it);
	}
	if (front) gtk_list_store_prepend(p->store, &it);
	else gtk_list_store_append(p->store, &it);
	gtk_list_store_set(p->store, &it, COL_NAME, face.c_str(), COL_LOWER, lower(face.c_str()).c_str(), -1);
}

void showError(GtkWidget *parent, const std::string &msg) {
	GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK, "%s", msg.c_str());
	gtk_dialog_run(GTK_DIALOG(m));
	gtk_widget_destroy(m);
}

void onAddFile(GtkButton *, gpointer data) {
	Picker *p = static_cast<Picker *>(data);
	const Text &t = texts();
	GtkWidget *fc = gtk_file_chooser_dialog_new(t.fileTitle, GTK_WINDOW(p->dialog), GTK_FILE_CHOOSER_ACTION_OPEN,
		t.cancel, GTK_RESPONSE_CANCEL, t.ok, GTK_RESPONSE_ACCEPT, nullptr);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(fc), TRUE);
	GtkFileFilter *fonts = gtk_file_filter_new();
	gtk_file_filter_set_name(fonts, t.fontFiles);
	for (const char *pat : {"*.[tT][tT][fF]", "*.[oO][tT][fF]", "*.[tT][tT][cC]", "*.[oO][tT][cC]"})
		gtk_file_filter_add_pattern(fonts, pat);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), fonts);
	GtkFileFilter *all = gtk_file_filter_new();
	gtk_file_filter_set_name(all, t.allFiles);
	gtk_file_filter_add_pattern(all, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), all);
	std::string path;
	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		gchar *f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		if (f) { path = f; g_free(f); }
	}
	gtk_widget_destroy(fc);
	if (path.empty()) return;

	std::string error;
	std::vector<std::string> faces;
	if (p->req->addFile) faces = p->req->addFile(path, error);
	if (faces.empty()) {
		showError(p->dialog, error.empty() ? std::string(t.noFace) : std::string(t.addFailed) + error);
		return;
	}
	// Let GTK render the preview with the new file too.
	FcConfigAppFontAddFile(nullptr, reinterpret_cast<const FcChar8 *>(path.c_str()));
	p->file = path;
	p->fileFaces = faces;
	for (auto it = faces.rbegin(); it != faces.rend(); ++it) addFace(p, *it, true);
	++p->syncing;
	gtk_entry_set_text(GTK_ENTRY(p->search), "");
	gtk_entry_set_text(GTK_ENTRY(p->nameEntry), faces.front().c_str());
	--p->syncing;
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(p->filter));
	selectFace(p, faces.front());
	updatePreview(p);
}

std::string trim(const std::string &s) {
	size_t b = 0, e = s.size();
	while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
	while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
	return s.substr(b, e - b);
}

} // namespace

int KR2LinuxPickFont(const KR2FontPickRequest &req, KR2FontPickResult &out) {
	if (!KR2LinuxGtkInit()) return -2;
	const Text &t = texts();
	Picker p;
	p.req = &req;
	p.sample = req.sample.empty() ? t.sampleDefault : req.sample;

	p.dialog = gtk_dialog_new_with_buttons(req.title.empty() ? t.title : req.title.c_str(), nullptr,
		GTK_DIALOG_MODAL, t.cancel, GTK_RESPONSE_CANCEL, t.ok, GTK_RESPONSE_OK, nullptr);
	GtkWindow *win = GTK_WINDOW(p.dialog);
	gtk_window_set_keep_above(win, TRUE);
	gtk_window_set_position(win, GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(win, 560, 600);
	gtk_dialog_set_default_response(GTK_DIALOG(p.dialog), GTK_RESPONSE_OK);

	GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(p.dialog));
	gtk_container_set_border_width(GTK_CONTAINER(area), 10);
	gtk_box_set_spacing(GTK_BOX(area), 6);

	GtkWidget *prompt = gtk_label_new(req.prompt.empty() ? t.hint : (req.prompt + "\n" + t.hint).c_str());
	gtk_label_set_xalign(GTK_LABEL(prompt), 0.0f);
	gtk_label_set_line_wrap(GTK_LABEL(prompt), TRUE);
	gtk_box_pack_start(GTK_BOX(area), prompt, FALSE, FALSE, 0);

	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	p.search = gtk_search_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(p.search), t.search);
	gtk_box_pack_start(GTK_BOX(row), p.search, TRUE, TRUE, 0);
	GtkWidget *addBtn = gtk_button_new_with_label(t.addFile);
	gtk_box_pack_start(GTK_BOX(row), addBtn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(area), row, FALSE, FALSE, 0);

	p.store = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING);
	std::vector<std::string> faces = req.faces;
	std::sort(faces.begin(), faces.end(), [](const std::string &a, const std::string &b) {
		return g_utf8_collate(a.c_str(), b.c_str()) < 0;
	});
	faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
	for (const std::string &f : faces) {
		if (f.empty()) continue;
		GtkTreeIter it;
		gtk_list_store_append(p.store, &it);
		gtk_list_store_set(p.store, &it, COL_NAME, f.c_str(), COL_LOWER, lower(f.c_str()).c_str(), -1);
	}
	p.filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(p.store), nullptr);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(p.filter), filterVisible, &p, nullptr);
	p.view = gtk_tree_view_new_with_model(p.filter);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(p.view), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(p.view), FALSE);
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(p.view), -1, "", cell, "text", COL_NAME, nullptr);
	GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(scroll), p.view);
	gtk_box_pack_start(GTK_BOX(area), scroll, TRUE, TRUE, 0);

	GtkWidget *frame = gtk_frame_new(nullptr);
	p.preview = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(p.preview), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(p.preview), PANGO_WRAP_WORD_CHAR);
	gtk_widget_set_size_request(p.preview, -1, 72);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
	gtk_container_add(GTK_CONTAINER(frame), p.preview);
	gtk_box_pack_start(GTK_BOX(area), frame, FALSE, FALSE, 0);

	GtkWidget *nameRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(nameRow), gtk_label_new(t.name), FALSE, FALSE, 0);
	p.nameEntry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(p.nameEntry), TRUE);
	gtk_box_pack_start(GTK_BOX(nameRow), p.nameEntry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(area), nameRow, FALSE, FALSE, 0);

	g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(p.view)), "changed", G_CALLBACK(onSelectionChanged), &p);
	g_signal_connect(p.view, "row-activated", G_CALLBACK(onRowActivated), &p);
	g_signal_connect(p.nameEntry, "changed", G_CALLBACK(onNameChanged), &p);
	g_signal_connect(p.search, "changed", G_CALLBACK(onSearchChanged), &p);
	g_signal_connect(addBtn, "clicked", G_CALLBACK(onAddFile), &p);

	++p.syncing;
	gtk_entry_set_text(GTK_ENTRY(p.nameEntry), req.initial.c_str());
	--p.syncing;
	gtk_widget_show_all(p.dialog);
	selectFace(&p, req.initial);
	updatePreview(&p);
	gtk_widget_grab_focus(p.search);

	int ret = 0;
	for (;;) {
		const gint r = gtk_dialog_run(GTK_DIALOG(p.dialog));
		if (r != GTK_RESPONSE_OK) break;
		const std::string face = trim(gtk_entry_get_text(GTK_ENTRY(p.nameEntry)));
		if (face.empty()) { gtk_widget_grab_focus(p.nameEntry); continue; }
		out.face = face;
		if (!p.file.empty() && std::find(p.fileFaces.begin(), p.fileFaces.end(), face) != p.fileFaces.end())
			out.file = p.file;
		ret = 1;
		break;
	}
	gtk_widget_destroy(p.dialog);
	g_object_unref(p.filter);
	g_object_unref(p.store);
	while (gtk_events_pending()) gtk_main_iteration();
	return ret;
}

#else // !KR2_LINUX_HAVE_GTK3

int KR2LinuxPickFont(const KR2FontPickRequest &, KR2FontPickResult &) { return -2; }

#endif
