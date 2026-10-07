// SPDX-License-Identifier: AGPL-3.0-only
// GUI check of the GTK win32dialog renderer (needs a display, e.g. xvfb-run).
//   win32dialog_gui wrapper.tjs test.tjs [screenshot.png]
// wrapper.tjs: KiriKiri's win32dialog.tjs (UTF-8).  test.tjs builds a font
// selection dialog like k2compat_fontselect.tjs.  An automatic "user" selects
// the third list row and presses OK; the script then checks the result.
#include <gtk/gtk.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "tjs.h"
#include "tjsError.h"
#include "ncbind.hpp"
#include "tjsByteCodeLoader.h"
#include "tjsScriptBlock.h"
using namespace TJS;

static tTJS *gEngine;
iTJSDispatch2 *TVPGetScriptDispatch() { return gEngine->GetGlobal(); }
void TVPThrowExceptionMessage(const tjs_char *msg) { TJS_eTJSError(msg); }
void TVPThrowExceptionMessage(const tjs_char *msg, const ttstr &p1) { TJS_eTJSError(ttstr(msg) + TJS_W(" ") + p1); }
void TVPExecuteExpression(const ttstr &content, tTJSVariant *result) { gEngine->EvalExpression(content, result); }
void TVPExecuteScript(const ttstr &content, tTJSVariant *result) { gEngine->ExecScript(content, result); }
void TVPAddLog(const ttstr &s) { std::fprintf(stderr, "%s\n", s.AsStdString().c_str()); }

static const char *gShot = nullptr;
static std::vector<uint32_t> gPixels(64 * 16, 0xFF3366CCu);

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

static gboolean autopilot(gpointer) {
	GList *tops = gtk_window_list_toplevels();
	GtkWidget *dlg = nullptr;
	for (GList *l = tops; l; l = l->next)
		if (gtk_widget_get_visible(GTK_WIDGET(l->data)) && gtk_window_get_modal(GTK_WINDOW(l->data))) dlg = GTK_WIDGET(l->data);
	g_list_free(tops);
	if (!dlg) return G_SOURCE_CONTINUE;
	GtkWidget *tree = findWidget(dlg, GTK_TYPE_TREE_VIEW, nullptr);
	if (tree) {
		GtkTreePath *path = gtk_tree_path_new_from_indices(2, -1);
		gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)), path);
		gtk_tree_path_free(path);
	}
	while (gtk_events_pending()) gtk_main_iteration();
	if (gShot) {
		int w, h;
		gtk_window_get_size(GTK_WINDOW(dlg), &w, &h);
		std::printf("dialog size %dx%d\n", w, h);
		std::string cmd = std::string("import -window root ") + gShot;
		if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "screenshot failed\n");
	}
	GtkWidget *ok = findWidget(dlg, GTK_TYPE_BUTTON, "OK");
	if (ok) gtk_button_clicked(GTK_BUTTON(ok));
	return G_SOURCE_REMOVE;
}

static std::string readFile(const char *path) {
	std::ifstream f(path, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

int main(int argc, char **argv) {
	if (argc < 3) { std::fprintf(stderr, "usage: %s wrapper.tjs test.tjs [shot.png]\n", argv[0]); return 2; }
	gShot = argc > 3 ? argv[3] : nullptr;
	if (!gtk_init_check(&argc, &argv)) { std::printf("SKIP: no display\n"); return 77; }
	gEngine = new tTJS();
	char buf[1024];
	std::snprintf(buf, sizeof buf,
		"global.System = %%[ screenWidth:1280, screenHeight:720, inform:function(t,c){ global.informed=1; return 0; } ];"
		"global.Debug = %%[ message:function(*){}, notice:function(*){} ];"
		"global.Plugins = %%[ link:function(*){} ];"
		"global.TestPixels = %%[ imageWidth:64, imageHeight:16, mainImageBufferPitch:256, mainImageBuffer:%lld ];",
		(long long)(intptr_t)gPixels.data());
	gEngine->ExecScript(ttstr(buf));
	try {
		ncbAutoRegister::AllRegist();
		if (!ncbAutoRegister::LoadModule(TJS_W("win32dialog.dll"))) { std::printf("FAIL load\n"); return 1; }
	} catch (const eTJS &e) {
		std::printf("FAIL plugin: %s\n", e.GetMessage().AsStdString().c_str());
		return 1;
	}
	g_timeout_add(300, autopilot, nullptr);
	for (int i = 1; i <= 2; ++i) {
		ttstr name(argv[i]);
		try {
			const std::string data = readFile(argv[i]);
			if (data.compare(0, 4, "TJS2") == 0) { // compiled script (e.g. a game's system/win32dialog.tjs)
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
	std::printf("PASS\n");
	return 0;
}
