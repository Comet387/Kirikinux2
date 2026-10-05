// SPDX-License-Identifier: AGPL-3.0-only
// Real GTK regression: scrolling, complete clipboard text, response indexes,
// cancellation and a local file selection (including Unicode/space paths).
#include <gtk/gtk.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int KR2LinuxMessageBox(const std::string &, const std::string &, const std::vector<std::string> &);
std::string KR2LinuxSelectGame(const std::string &, const std::vector<std::string> &);
void KR2LinuxSetUIFont(const std::string &);

static void require(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static GtkWidget *findWidget(GtkWidget *root, GType type) {
    if (G_TYPE_CHECK_INSTANCE_TYPE(root, type)) return root;
    if (!GTK_IS_CONTAINER(root)) return nullptr;
    GList *children = gtk_container_get_children(GTK_CONTAINER(root));
    GtkWidget *result = nullptr;
    for (GList *c = children; c && !result; c = c->next) result = findWidget(GTK_WIDGET(c->data), type);
    g_list_free(children);
    return result;
}

static GtkWidget *dialog() {
    GList *windows = gtk_window_list_toplevels();
    GtkWidget *result = nullptr;
    for (GList *w = windows; w; w = w->next)
        if (GTK_IS_DIALOG(w->data) && gtk_widget_get_visible(GTK_WIDGET(w->data))) { result = GTK_WIDGET(w->data); break; }
    g_list_free(windows);
    return result;
}

struct MessageTest { std::string text; int ticks = 0; int stage = 0; };
static gboolean checkMessage(gpointer pointer) {
    auto &test = *static_cast<MessageTest*>(pointer);
    require(++test.ticks < 150, "dialog timed out");
    GtkWidget *dlg = dialog();
    if (!dlg || test.ticks < 3) return G_SOURCE_CONTINUE;
    auto *view = findWidget(dlg, GTK_TYPE_TEXT_VIEW);
    auto *scroll = findWidget(dlg, GTK_TYPE_SCROLLED_WINDOW);
    require(view && scroll, "text view / scroll container missing");
    if (test.stage == 0) {
        int width, height; gtk_window_get_size(GTK_WINDOW(dlg), &width, &height);
        require(height <= 560, "long device info expanded the window vertically");
        GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
        require(gtk_adjustment_get_upper(adjustment) > gtk_adjustment_get_page_size(adjustment), "long text cannot scroll");
        gtk_adjustment_set_value(adjustment, gtk_adjustment_get_upper(adjustment));
        gtk_dialog_response(GTK_DIALOG(dlg), 10000);
        test.stage = 1;
        return G_SOURCE_CONTINUE;
    }
    gchar *copy = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
    require(copy && test.text == copy, "clipboard did not contain the complete original text");
    g_free(copy);
    gtk_dialog_response(GTK_DIALOG(dlg), 2);
    return G_SOURCE_REMOVE;
}

static gboolean checkShort(gpointer) {
    GtkWidget *dlg=dialog();if(!dlg)return G_SOURCE_CONTINUE;
    int width,height;gtk_window_get_size(GTK_WINDOW(dlg),&width,&height);
    if(height<40)return G_SOURCE_CONTINUE;
    require(height<240 && width<600,"short confirmation is oversized");
    require(!findWidget(dlg,GTK_TYPE_SCROLLED_WINDOW),"short confirmation has a scroll container");
    require(!gtk_dialog_get_widget_for_response(GTK_DIALOG(dlg),10000),"short confirmation has an unnecessary copy button");
    gtk_dialog_response(GTK_DIALOG(dlg),1);return G_SOURCE_REMOVE;
}

struct PickerTest { std::string path; bool cancel; int ticks = 0; int stage = 0; };
static gboolean checkPicker(gpointer pointer) {
    auto &test = *static_cast<PickerTest*>(pointer);
    require(++test.ticks < 150, "file chooser timed out");
    GtkWidget *dlg = dialog();
    if (!dlg) return G_SOURCE_CONTINUE;
    if (test.cancel) { gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL); return G_SOURCE_REMOVE; }
    if (!GTK_IS_FILE_CHOOSER(dlg)) {
        gtk_dialog_response(GTK_DIALOG(dlg), 2);
        return G_SOURCE_CONTINUE;
    }
    if (test.stage == 0) {
        require(gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), test.path.c_str()), "cannot select test file");
        test.stage = test.ticks;
        return G_SOURCE_CONTINUE;
    }
    if (test.ticks - test.stage < 8) return G_SOURCE_CONTINUE;
    gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    require(argc == 3, "usage: desktop_dialogs <packaged-font> <existing-test.xp3>");
    KR2LinuxSetUIFont(argv[1]);
    g_timeout_add(50,checkShort,nullptr);
    require(KR2LinuxMessageBox("确定退出吗？","Kirikinux2",{"是","否"})==1,"short confirmation response index changed");
    MessageTest message;
    for (int i = 0; i < 3000; ++i) message.text += "设备信息 / OpenGL extension " + std::to_string(i) + "\n";
    message.text += "完整文本末尾 / END";
    g_timeout_add(50, checkMessage, &message);
    require(KR2LinuxMessageBox(message.text, "设备信息", {"确定", "补丁", "关闭"}) == 2, "caller response index changed");
    require(message.stage == 1, "copy action did not run");
    std::vector<std::string> labels = {"打开游戏", "选择 XP3 或游戏目录", "取消", "选择目录", "选择文件", "打开"};
    PickerTest cancel{"", true};
    g_timeout_add(50, checkPicker, &cancel);
    require(KR2LinuxSelectGame("/", labels).empty(), "cancel unexpectedly returned a path");
    PickerTest picker{argv[2], false};
    g_timeout_add(50, checkPicker, &picker);
    gchar *folder = g_path_get_dirname(argv[2]);
    require(KR2LinuxSelectGame(folder, labels) == argv[2], "selected Unicode/space path changed");
    g_free(folder);
    std::puts("PASS: long dialog scroll, full clipboard, response indexes, cancel and game file chooser");
    return 0;
}
