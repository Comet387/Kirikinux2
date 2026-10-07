// SPDX-License-Identifier: AGPL-3.0-only
// GUI check of the native font picker reached through win32dialog.dll's
// font-selector interception (needs a display, e.g. xvfb-run).
//   font_picker_gui win32dialog.tjs font_picker_gui_test.tjs FONTFILE
#include <gtk/gtk.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "tjs.h"
#include "tjsError.h"
#include "ncbind.hpp"
#include "tjsByteCodeLoader.h"
#include "tjsScriptBlock.h"
#include "../../platform/linux/LinuxFontPicker.h"
using namespace TJS;

static tTJS *gEngine;
iTJSDispatch2 *TVPGetScriptDispatch() { return gEngine->GetGlobal(); }
void TVPThrowExceptionMessage(const tjs_char *msg) { TJS_eTJSError(msg); }
void TVPThrowExceptionMessage(const tjs_char *msg, const ttstr &p1) { TJS_eTJSError(ttstr(msg) + TJS_W(" ") + p1); }
void TVPExecuteExpression(const ttstr &content, tTJSVariant *result) { gEngine->EvalExpression(content, result); }
void TVPExecuteScript(const ttstr &content, tTJSVariant *result) { gEngine->ExecScript(content, result); }
void TVPAddLog(const ttstr &s) { std::fprintf(stderr, "%s\n", s.AsStdString().c_str()); }
bool KR2LinuxGtkInit() { return true; }

static const char *gFontFile = nullptr;
static int gStep = 0;
static bool gWaitingFile = false;
static GtkWidget *gAddButton = nullptr;

// Clicked from its own idle source: the add button runs a nested file
// chooser, which the autopilot answers from its (separate) timeout source.
static gboolean clickAdd(gpointer) { gtk_button_clicked(GTK_BUTTON(gAddButton)); return G_SOURCE_REMOVE; }
static std::string gInitials;

// Engine side stand-in (src/core/visual/FontPicker.cpp).
int TVPShowFontPicker(const ttstr &caption, const ttstr &prompt, const ttstr &sample,
	const ttstr &initial, bool, ttstr &face, ttstr *file) {
	gInitials += initial.AsStdString() + "|";
	tTJSVariant v = ttstr(gInitials.c_str());
	iTJSDispatch2 *g = gEngine->GetGlobal();
	g->PropSet(TJS_MEMBERENSURE, TJS_W("pickerInitials"), nullptr, &v, g);
	g->Release();
	KR2FontPickRequest req;
	req.title = caption.AsStdString();
	req.prompt = prompt.AsStdString();
	req.sample = sample.AsStdString();
	req.initial = initial.AsStdString();
	req.faces = { "Noto Sans CJK JP", "Source Han Serif SC", "思源黑体 CN Bold", "DejaVu Sans" };
	req.addFile = [](const std::string &path, std::string &) {
		return std::vector<std::string>{ path == gFontFile ? "Picked From File" : "?" };
	};
	KR2FontPickResult res;
	const int r = KR2LinuxPickFont(req, res);
	if (r == 1) { face = ttstr(res.face); if (file) *file = ttstr(res.file); }
	return r == -2 ? -1 : r;
}

static GtkWidget *findWidget(GtkWidget *w, GType type, const char *label) {
	if (G_TYPE_CHECK_INSTANCE_TYPE(w, type)) {
		if (!label) return w;
		const gchar *l = GTK_IS_BUTTON(w) ? gtk_button_get_label(GTK_BUTTON(w)) : nullptr;
		if (l && std::string(l).find(label) != std::string::npos) return w;
	}
	if (GTK_IS_CONTAINER(w)) {
		GList *children = gtk_container_get_children(GTK_CONTAINER(w));
		for (GList *c = children; c; c = c->next) {
			GtkWidget *r = findWidget(GTK_WIDGET(c->data), type, label);
			if (r) { g_list_free(children); return r; }
		}
		g_list_free(children);
	}
	return nullptr;
}

static GtkWidget *topModal(GType type) {
	GList *tops = gtk_window_list_toplevels();
	GtkWidget *dlg = nullptr;
	for (GList *l = tops; l; l = l->next)
		if (gtk_widget_get_visible(GTK_WIDGET(l->data)) && G_TYPE_CHECK_INSTANCE_TYPE(l->data, type)) dlg = GTK_WIDGET(l->data);
	g_list_free(tops);
	return dlg;
}

static void settle() { while (gtk_events_pending()) gtk_main_iteration(); }

// Entries in the picker: [0] search, [1] font name.
static std::vector<GtkWidget *> entries(GtkWidget *w, std::vector<GtkWidget *> acc = {}) {
	if (GTK_IS_ENTRY(w)) acc.push_back(w);
	if (GTK_IS_CONTAINER(w)) {
		GList *children = gtk_container_get_children(GTK_CONTAINER(w));
		for (GList *c = children; c; c = c->next) acc = entries(GTK_WIDGET(c->data), acc);
		g_list_free(children);
	}
	return acc;
}

static gboolean autopilot(gpointer) {
	if (GtkWidget *fc = topModal(GTK_TYPE_FILE_CHOOSER_DIALOG)) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(fc), gFontFile);
		settle();
		gtk_dialog_response(GTK_DIALOG(fc), GTK_RESPONSE_ACCEPT);
		gWaitingFile = false;
		return G_SOURCE_CONTINUE;
	}
	GtkWidget *dlg = topModal(GTK_TYPE_DIALOG);
	if (!dlg || GTK_IS_MESSAGE_DIALOG(dlg) || gWaitingFile) return G_SOURCE_CONTINUE;
	std::vector<GtkWidget *> e = entries(dlg);
	if (e.size() < 2) { std::printf("FAIL picker entries\n"); std::exit(1); }
	GtkWidget *tree = findWidget(dlg, GTK_TYPE_TREE_VIEW, nullptr);
	switch (gStep++) {
	case 0: {
		if (const char *shot = std::getenv("FONT_PICKER_SHOT")) { // optional screenshot
			settle();
			std::system((std::string("import -window root ") + shot).c_str());
		}
		gtk_entry_set_text(GTK_ENTRY(e[0]), "serif");
		settle();
		GtkTreePath *path = gtk_tree_path_new_from_indices(0, -1);
		gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)), path);
		gtk_tree_path_free(path);
		settle();
		gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
		break;
	}
	case 1:
		gtk_entry_set_text(GTK_ENTRY(e[1]), "My Typed Font");
		settle();
		gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
		break;
	case 2:
		gAddButton = findWidget(dlg, GTK_TYPE_BUTTON, "…");
		if (!gAddButton) { std::printf("FAIL add button\n"); std::exit(1); }
		gWaitingFile = true;
		g_idle_add(clickAdd, nullptr);
		break;
	case 3: {
		const char *name = gtk_entry_get_text(GTK_ENTRY(e[1]));
		if (std::strcmp(name, "Picked From File") != 0) { std::printf("FAIL entry after file: %s\n", name); std::exit(1); }
		gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
		break;
	}
	default:
		gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL);
		break;
	}
	return G_SOURCE_CONTINUE;
}

static std::string readFile(const char *path) {
	std::ifstream f(path, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

int main(int argc, char **argv) {
	if (argc < 4) { std::fprintf(stderr, "usage: %s wrapper.tjs test.tjs fontfile\n", argv[0]); return 2; }
	gFontFile = argv[3];
	if (!gtk_init_check(&argc, &argv)) { std::printf("SKIP: no display\n"); return 77; }
	gEngine = new tTJS();
	gEngine->ExecScript(ttstr(
		"global.System = %[ screenWidth:1280, screenHeight:720, inform:function(t,c){ global.informed=1; return 0; } ];"
		"global.Debug = %[ message:function(*){}, notice:function(*){} ];"
		"global.Plugins = %[ link:function(*){} ];"));
	try {
		ncbAutoRegister::AllRegist();
		if (!ncbAutoRegister::LoadModule(TJS_W("win32dialog.dll"))) { std::printf("FAIL load\n"); return 1; }
	} catch (const eTJS &e) {
		std::printf("FAIL plugin: %s\n", e.GetMessage().AsStdString().c_str());
		return 1;
	}
	g_timeout_add(200, autopilot, nullptr);
	for (int i = 1; i <= 2; ++i) {
		ttstr name(argv[i]);
		try {
			const std::string data = readFile(argv[i]);
			if (data.compare(0, 4, "TJS2") == 0) {
				std::vector<tjs_uint8> bytes(data.begin(), data.end());
				tTJSByteCodeLoader loader;
				tTJSScriptBlock *block = loader.ReadByteCode(gEngine, name.c_str(), bytes.data(), bytes.size());
				if (!block) { std::printf("FAIL bytecode %s\n", argv[i]); return 1; }
				iTJSDispatch2 *global = gEngine->GetGlobal();
				block->ExecuteTopLevel(nullptr, global);
				global->Release();
				block->Release();
				continue;
			}
			gEngine->ExecScript(ttstr(data.c_str()), nullptr, nullptr, &name);
		} catch (const eTJSScriptError &e) {
			std::printf("FAIL %s: %s (line %d)\n", argv[i], e.GetMessage().AsStdString().c_str(), (int)e.GetSourceLine());
			return 1;
		} catch (const eTJS &e) {
			std::printf("FAIL %s: %s\n", argv[i], e.GetMessage().AsStdString().c_str());
			return 1;
		}
	}
	if (gStep != 5) { std::printf("FAIL autopilot steps %d\n", gStep); return 1; }
	std::printf("PASS\n");
	return 0;
}
