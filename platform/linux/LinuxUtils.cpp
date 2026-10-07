//---------------------------------------------------------------------------
// kirikinux2 : Linux desktop host for the Kirikiroid2 core.
//
// Linux counterpart of src/core/environ/android/AndroidUtils.cpp.  It defines
// the same set of symbols AndroidUtils.cpp exports on Android, so src/core and
// src/plugins are compiled unchanged.  Desktop behaviour follows the original
// win32 host (src/core/environ/win32/Platform.cpp); POSIX helpers that Android
// already shares stay in src/core/environ/linux/Platform.cpp
// (TVPGetMemoryInfo, TVPRelinquishCPU, TVP_utime).
//
// Do NOT move this file into src/core/environ/linux/: project/android globs that
// directory and would then link AndroidUtils.cpp and this file together.
//---------------------------------------------------------------------------
#include "Platform.h"
#include "tjsCommHead.h"
#include "StorageImpl.h"
#include "SysInitIntf.h"
#include "EventIntf.h"
#include "RenderManager.h"
#include "Application.h"
#include "TickCount.h"
#include "cocos2d/MainScene.h"
#include "ConfigManager/LocaleConfigManager.h"
#include "WindowIntf.h"
#include "cocos2d.h"

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// LinuxDialogs.cpp (GTK3, kept in its own translation unit so glib/gtk macros never
// meet the krkr headers).  Both return -2 when no dialog could be shown.
int KR2LinuxMessageBox(const std::string &text, const std::string &caption, const std::vector<std::string> &buttons);
std::string KR2LinuxSelectGame(const std::string &initialPath, const std::vector<std::string> &labels);
int KR2LinuxInputBox(std::string &text, const std::string &caption, const std::string &prompt, const std::vector<std::string> &buttons);

extern std::thread::id TVPMainThreadID; // Application.cpp, set by TVPAppDelegate
int TVPCheckArchive(const ttstr &localname);  // UtilStreams.cpp (same declaration as the win32 host)

// Program arguments without argv[0] and without the host options handled by LinuxMain.cpp.
std::vector<std::string> TVPLinuxStartupArgs;

//---------------------------------------------------------------------------
// small POSIX helpers
//---------------------------------------------------------------------------
static bool _IsDir(const std::string &p) {
	struct stat st;
	return !p.empty() && ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string _HomeDir() {
	const char *h = getenv("HOME");
	return (h && *h) ? std::string(h) : std::string("/");
}

static void _PushUniqueDir(std::vector<std::string> &v, std::string p) {
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	if (!_IsDir(p)) return;
	if (std::find(v.begin(), v.end(), p) == v.end()) v.emplace_back(p);
}

static bool _MkdirP(const std::string &dir) {
	if (dir.empty()) return false;
	std::string cur = dir[0] == '/' ? "/" : "";
	size_t pos = 0;
	while (pos <= dir.size()) {
		size_t next = dir.find('/', pos);
		if (next == std::string::npos) next = dir.size();
		std::string part = dir.substr(pos, next - pos);
		if (!part.empty()) {
			if (!cur.empty() && cur.back() != '/') cur += '/';
			cur += part;
			if (::mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) return false;
		}
		pos = next + 1;
	}
	return _IsDir(dir);
}

static std::string _ToLower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
	return s;
}

// /proc/mounts escapes blanks as octal (\040)
static std::string _UnescapeMount(const char *s) {
	std::string r;
	for (; *s; ++s) {
		if (s[0] == '\\' && s[1] >= '0' && s[1] <= '7' && s[2] >= '0' && s[2] <= '7' && s[3] >= '0' && s[3] <= '7') {
			r += (char)(((s[1] - '0') << 6) | ((s[2] - '0') << 3) | (s[3] - '0'));
			s += 3;
		} else {
			r += *s;
		}
	}
	return r;
}

static std::string _AppDataDir() {
	const char *x = getenv("XDG_DATA_HOME");
	std::string dir = (x && *x) ? std::string(x) : _HomeDir() + "/.local/share";
	const std::string legacy = dir + "/kirikiroid2";
	dir += "/kirikinux";
	_MkdirP(dir);
	// Preserve prior global settings and recent games without deleting originals.
	for (const char *name : {"GlobalPreference.xml", "recentpath.xml"}) {
		const std::string destination = dir + "/" + name;
		if (access(destination.c_str(), F_OK) == 0) continue;
		FILE *input = fopen((legacy + "/" + name).c_str(), "rb");
		if (!input) continue;
		FILE *output = fopen(destination.c_str(), "wb");
		bool copied = output != nullptr;
		if (output) {
			char buffer[4096];
			size_t n;
			while ((n = fread(buffer, 1, sizeof(buffer), input)) != 0) {
				if (fwrite(buffer, 1, n, output) != n) { copied = false; break; }
			}
			if (ferror(input)) copied = false;
			if (fclose(output)) copied = false;
		}
		fclose(input);
		if (!copied) unlink(destination.c_str());
	}
	return dir;
}

// Dialogs need the cocos2d-x (main) thread, like the Java UI thread on Android.
static int _RunOnMainThread(const std::function<int()> &fn) {
	if (TVPMainThreadID == std::thread::id() || std::this_thread::get_id() == TVPMainThreadID)
		return fn();
	std::mutex m;
	std::condition_variable cv;
	bool done = false;
	int ret = -1;
	cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([&]() {
		int r = fn();
		std::lock_guard<std::mutex> lk(m);
		ret = r;
		done = true;
		cv.notify_all();
	});
	std::unique_lock<std::mutex> lk(m);
	cv.wait(lk, [&] { return done; });
	return ret;
}

//---------------------------------------------------------------------------
// logging / memory / time
//---------------------------------------------------------------------------
void TVPPrintLog(const char *str) {
	fputs(str, stdout); // same as the win32 host (printf("%s"))
	fflush(stdout);
}

static tjs_uint32 _lastMemoryInfoQuery = 0;
static tjs_int _availMemory = 0, _usedMemory = 0;
static void _updateMemoryInfo() {
	tjs_uint32 now = TVPGetRoughTickCount32();
	if (_lastMemoryInfoQuery && now - _lastMemoryInfoQuery < 3000) return; // freq in 3s, like Android
	_lastMemoryInfoQuery = now ? now : 1;
	long long availKB = -1, freeKB = -1;
	if (FILE *fp = fopen("/proc/meminfo", "r")) {
		char line[256];
		while (fgets(line, sizeof(line), fp)) {
			long long v;
			if (sscanf(line, "MemAvailable: %lld kB", &v) == 1) availKB = v;
			else if (sscanf(line, "MemFree: %lld kB", &v) == 1) freeKB = v;
		}
		fclose(fp);
	}
	if (availKB < 0) availKB = freeKB;
	if (availKB >= 0) _availMemory = (tjs_int)(availKB / 1024);
	if (FILE *fp = fopen("/proc/self/statm", "r")) {
		long long pages = 0, rss = 0;
		if (fscanf(fp, "%lld %lld", &pages, &rss) == 2)
			_usedMemory = (tjs_int)(rss * (long long)sysconf(_SC_PAGESIZE) / (1024 * 1024));
		fclose(fp);
	}
}

tjs_int TVPGetSystemFreeMemory() { _updateMemoryInfo(); return _availMemory; } // MB
tjs_int TVPGetSelfUsedMemory() { _updateMemoryInfo(); return _usedMemory; }    // MB

tjs_uint32 TVPGetRoughTickCount32() {
	tjs_uint32 uptime = 0;
	struct timespec on;
	if (clock_gettime(CLOCK_MONOTONIC, &on) == 0)
		uptime = (tjs_uint32)(on.tv_sec * 1000 + on.tv_nsec / 1000000);
	return uptime;
}

// Director::drawScene() already polls GLFW and swaps on desktop (win32 host: same no-ops).
void TVPForceSwapBuffer() {}
void TVPProcessInputEvents() {}
// Desktop input goes through TVPMainScene::attachWithIME() (non-Android branch).
void TVPShowIME(int x, int y, int w, int h) {}
void TVPHideIME() {}
void TVPControlAdDialog(int adType, int arg1, int arg2) {}
void TVPFetchSDCardPermission() {}
void TVPSendToOtherApp(const std::string &filename) {}

//---------------------------------------------------------------------------
// device / package information
//---------------------------------------------------------------------------
std::string TVPGetDeviceID() {
	static const char *const files[] = { "/etc/machine-id", "/var/lib/dbus/machine-id" };
	for (const char *path : files) {
		FILE *fp = fopen(path, "r");
		if (!fp) continue;
		char buf[128] = { 0 };
		std::string id;
		if (fgets(buf, sizeof(buf), fp)) id = buf;
		fclose(fp);
		while (!id.empty() && isspace((unsigned char)id.back())) id.pop_back();
		if (!id.empty()) return "MachineID=" + id;
	}
	return std::string();
}

static std::string _EnvLocale() {
	static const char *const names[] = { "LC_ALL", "LC_MESSAGES", "LANGUAGE", "LANG" };
	for (const char *n : names) {
		const char *v = getenv(n);
		if (v && *v && strcmp(v, "C") && strcmp(v, "POSIX")) {
			std::string s(v);
			size_t p = s.find(':'); // LANGUAGE may be a list
			if (p != std::string::npos) s = s.substr(0, p);
			p = s.find_first_of(".@");
			if (p != std::string::npos) s = s.substr(0, p);
			if (!s.empty()) return _ToLower(s);
		}
	}
	return std::string();
}

std::string TVPGetDeviceLanguage() {
	std::string l = _EnvLocale();
	size_t p = l.find_first_of("_-");
	if (p != std::string::npos) l = l.substr(0, p);
	return l.empty() ? std::string("en") : l;
}

// Same format as the android/win32 hosts and cocos/kr2/Resources/res/locale/*.xml
std::string TVPGetCurrentLanguage() {
	std::string l = _EnvLocale();
	std::replace(l.begin(), l.end(), '-', '_');
	if (l.compare(0, 2, "zh") == 0) {
		if (l.find("tw") != std::string::npos || l.find("hk") != std::string::npos ||
			l.find("mo") != std::string::npos || l.find("hant") != std::string::npos)
			return "zh_tw";
		return "zh_cn";
	}
	if (l.compare(0, 2, "ja") == 0) return "ja_jp";
	return "en_us"; // only en_us/ja_jp/zh_cn/zh_tw are shipped
}

std::string TVPGetPackageVersionString() {
	return "1.3.9 (Linux, Kirikinux2)";
}

//---------------------------------------------------------------------------
// storage locations
//---------------------------------------------------------------------------
// [0] is where the file selector starts when there is no history
// (TVPMainFileSelectorForm::show).
std::vector<std::string> TVPGetDriverPath() {
	std::vector<std::string> ret;
	if (const char *dir = getenv("KIRIKINUX_GAME_DIR")) _PushUniqueDir(ret, dir);
	_PushUniqueDir(ret, _HomeDir());
	if (FILE *fp = fopen("/proc/mounts", "r")) { // removable media, like the Android /proc/mounts scan
		char dev[512], mnt[512], type[128];
		while (fscanf(fp, "%511s %511s %127s %*[^\n]", dev, mnt, type) == 3) {
			std::string m = _UnescapeMount(mnt);
			if (m.compare(0, 7, "/media/") == 0 || m.compare(0, 11, "/run/media/") == 0 || m.compare(0, 5, "/mnt/") == 0)
				_PushUniqueDir(ret, m);
		}
		fclose(fp);
	}
	_PushUniqueDir(ret, "/");
	return ret;
}

std::vector<std::string> TVPGetAppStoragePath() {
	std::vector<std::string> ret;
	ret.emplace_back(_AppDataDir());
	return ret;
}

// Saves are written next to the game.  Android refuses read-only locations; on a
// desktop a warning is more useful, the game itself can still run.
bool TVPCheckStartupPath(const std::string &path) {
	std::string dir = path;
	struct stat st;
	if (::stat(path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
		size_t p = path.find_last_of('/');
		dir = p == std::string::npos ? std::string(".") : (p == 0 ? std::string("/") : path.substr(0, p));
	}
	if (access(dir.c_str(), W_OK) != 0) {
		std::vector<ttstr> btns;
		btns.emplace_back(LocaleConfigManager::GetInstance()->GetText("continue_run"));
		TVPShowSimpleMessageBox(ttstr(dir), ttstr(LocaleConfigManager::GetInstance()->GetText("readonly_storage")), btns);
	}
	return true;
}

//---------------------------------------------------------------------------
// startup: "kirikiroid2 <game dir | archive> [name=value ...]" (same rules as the win32 host)
//---------------------------------------------------------------------------
static std::string _AbsPath(const std::string &p) {
	char buf[PATH_MAX];
	if (realpath(p.c_str(), buf)) return buf;
	if (!p.empty() && p[0] != '/' && getcwd(buf, sizeof(buf))) return std::string(buf) + "/" + p;
	return p;
}

std::string TVPSelectGamePath(const std::string &initialPath) {
	auto *locale = LocaleConfigManager::GetInstance();
	std::vector<std::string> labels;
	for (const char *id : {"menu_open_game", "open_game_prompt", "cancel", "open_game_folder", "open_game_file", "open_game_confirm"})
		labels.push_back(locale->GetText(id));
	std::string result;
	_RunOnMainThread([&]() { result = KR2LinuxSelectGame(initialPath, labels); return 0; });
	return result;
}

bool TVPCheckStartupArg() {
	if (TVPLinuxStartupArgs.empty()) return false;
	for (size_t i = 1; i < TVPLinuxStartupArgs.size(); ++i) {
		const std::string &a = TVPLinuxStartupArgs[i];
		size_t pos = a.find('=');
		if (pos == std::string::npos) {
			TVPSetCommandLine(ttstr(a).c_str(), ttstr("yes"));
		} else {
			TVPSetCommandLine(ttstr(a.substr(0, pos)).c_str(), ttstr(a.substr(pos + 1)));
		}
	}
	std::string path = _AbsPath(TVPLinuxStartupArgs[0]);
	if (TVPCheckExistentLocalFile(ttstr(path))) {
		if (TVPCheckArchive(ttstr(path)) == 1) {
			TVPMainScene::GetInstance()->startupFrom(path);
			return true;
		}
		return false;
	}
	if (!TVPCheckExistentLocalFolder(ttstr(path))) return false;
	bool hasStartup = false;
	std::string dataXp3;
	TVPListDir(path, [&](const std::string &name, int mask) {
		if (!(mask & S_IFREG)) return;
		std::string lower = _ToLower(name);
		if (lower == "startup.tjs") hasStartup = true;
		else if (lower == "data.xp3") dataXp3 = path + "/" + name;
	});
	if (hasStartup) {
		TVPMainScene::GetInstance()->startupFrom(path);
		return true;
	}
	if (!dataXp3.empty() && TVPCheckArchive(ttstr(dataXp3)) == 1) {
		TVPMainScene::GetInstance()->startupFrom(dataXp3);
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
// dialogs (Android: KR2Activity.ShowMessageBox/ShowInputBox through JNI)
//---------------------------------------------------------------------------
extern "C" int TVPShowSimpleMessageBox(const char *pszText, const char *pszTitle, unsigned int nButton, const char **btnText) {
	std::string text = pszText ? pszText : "", title = pszTitle ? pszTitle : "";
	std::vector<std::string> btns;
	for (unsigned int i = 0; i < nButton; ++i) btns.emplace_back((btnText && btnText[i]) ? btnText[i] : "");
	int ret = _RunOnMainThread([&]() { return KR2LinuxMessageBox(text, title, btns); });
	if (ret == -2) { // no GUI available: keep going with the first button, like a non-interactive run
		ret = 0;
	}
	return ret;
}

int TVPShowSimpleMessageBox(const ttstr &text, const ttstr &caption, const std::vector<ttstr> &vecButtons) {
	std::vector<std::string> hold;
	hold.reserve(vecButtons.size());
	for (const ttstr &btn : vecButtons) hold.emplace_back(btn.AsStdString());
	std::vector<const char *> ptrs;
	for (const std::string &s : hold) ptrs.emplace_back(s.c_str());
	std::string t = text.AsStdString(), c = caption.AsStdString();
	return TVPShowSimpleMessageBox(t.c_str(), c.c_str(), (unsigned int)ptrs.size(), ptrs.empty() ? nullptr : &ptrs[0]);
}

int TVPShowSimpleInputBox(ttstr &text, const ttstr &caption, const ttstr &prompt, const std::vector<ttstr> &vecButtons) {
	std::string t = text.AsStdString(), c = caption.AsStdString(), p = prompt.AsStdString();
	std::vector<std::string> btns;
	for (const ttstr &btn : vecButtons) btns.emplace_back(btn.AsStdString());
	int ret = _RunOnMainThread([&]() { return KR2LinuxInputBox(t, c, p, btns); });
	if (ret == -2) {
		ret = 0;
	}
	if (ret >= 0) text = t;
	return ret;
}

//---------------------------------------------------------------------------
// files
//---------------------------------------------------------------------------
bool TVPCreateFolders(const ttstr &folder) {
	if (folder.IsEmpty()) return true;
	return _MkdirP(folder.AsStdString());
}

// Write to a temporary file and rename it over the target, so an interrupted save
// never leaves a truncated file (Android keeps a .bak copy for the same reason).
bool TVPWriteDataToFile(const ttstr &filepath, const void *data, unsigned int size) {
	std::string filename = filepath.AsStdString();
	std::string pattern = filename + ".kr2tmp.XXXXXX";
	std::vector<char> tmp(pattern.begin(), pattern.end());
	tmp.push_back(0);
	int fd = mkstemp(tmp.data());
	if (fd < 0) return false;
	FILE *fp = fdopen(fd, "wb");
	if (!fp) { close(fd); unlink(tmp.data()); return false; }
	bool ok = size == 0 || fwrite(data, 1, size, fp) == size;
	if (fflush(fp) != 0) ok = false;
	if (ok && fsync(fd) != 0) ok = false;
	if (fclose(fp) != 0) ok = false;
	if (!ok || rename(tmp.data(), filename.c_str()) != 0) {
		unlink(tmp.data());
		return false;
	}
	return true;
}

bool TVPDeleteFile(const std::string &filename) {
	struct stat st;
	if (::lstat(filename.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
		return rmdir(filename.c_str()) == 0; // like _wunlink on win32: never recursive
	return unlink(filename.c_str()) == 0;
}

bool TVPRenameFile(const std::string &from, const std::string &to) {
	return rename(from.c_str(), to.c_str()) == 0;
}

// Leave without running C++ static destructors.  exit() destroys globals such
// as TVPGraphicType in an arbitrary order while engine worker threads (sound,
// image loading, video) are still running, which aborted with
// "free(): invalid pointer" in tTVPGraphicType::~tTVPGraphicType and left the
// crash handler running after the window had gone.  Everything that must
// survive (game saves, Kirikinux2 settings, recent list) is written to disk
// explicitly before this point, so only stdio buffers need flushing.
void TVPLinuxQuickExit(int code) {
	fflush(nullptr);
	_exit(code);
}

//---------------------------------------------------------------------------
// The window manager's close button ("X").  LinuxMain.cpp cancels GLFW's close
// and calls this on the Cocos thread instead of letting the main loop end.
//  - In a game it does what the in-game menu's "Exit" button does: the game's
//    onCloseQuery runs, so KAG/KAGEX games show their own "quit?" dialog and
//    shut down through TVPSystemUninit (same as clicking X on Windows).
//  - In the launcher it asks "sure_to_exit" like the Back key does.
//  - If an earlier in-game request was never picked up by the engine (script
//    loop stuck, startup error), the player is offered a forced quit.
//---------------------------------------------------------------------------
tTJSNI_Window *TVPGetActiveWindow(); // MainScene.cpp

static bool _ConfirmBox(const std::string &key, const char *fallback) {
	LocaleConfigManager *loc = LocaleConfigManager::GetInstance();
	std::string text = loc->GetText(key);
	if (text.empty() || text == key) text = fallback;
	return TVPShowSimpleMessageBoxYesNo(ttstr(text), ttstr("Kirikinux2")) == 0;
}

void TVPLinuxRequestClose() {
	static bool busy = false;
	static bool gameRequestPending = false;
	if (busy) return;
	busy = true;
	struct Reset { ~Reset() { busy = false; } } reset;

	if (TVPGetActiveWindow() && ::Application) {
		if (gameRequestPending) {
			if (_ConfirmBox("force_quit_game",
				"The game did not respond to the close request.\nForce quit? Unsaved progress will be lost."))
				TVPLinuxQuickExit(0);
			return;
		}
		gameRequestPending = true;
		::Application->PostUserMessage([]() {
			gameRequestPending = false;
			if (tTJSNI_Window *win = TVPGetActiveWindow()) win->Close();
		});
		return;
	}
	if (_ConfirmBox("sure_to_exit", "Sure to exit?"))
		TVPExitApplication(0);
}

void TVPExitApplication(int code) {
	TVPDeliverCompactEvent(TVP_COMPACT_LEVEL_MAX);
	if (!TVPIsSoftwareRenderManager())
		iTVPTexture2D::RecycleProcess();
	TVPLinuxQuickExit(code);
}

// Native stat timestamp fields use the timespec members explicitly.
#undef st_atime
#undef st_ctime
#undef st_mtime
bool TVP_stat(const tjs_char *name, tTVP_stat &s) {
	tTJSNarrowStringHolder holder(name);
	return TVP_stat(holder, s);
}

bool TVP_stat(const char *name, tTVP_stat &s) {
	struct stat t;
	static_assert(sizeof(t.st_size) == 8, "large file support required");
	if (::stat(name, &t) != 0) {
		memset(&s, 0, sizeof(s));
		return false;
	}
	s.st_mode = t.st_mode;
	s.st_size = t.st_size;
	s.atime_seconds = t.st_atim.tv_sec;
	s.mtime_seconds = t.st_mtim.tv_sec;
	s.ctime_seconds = t.st_ctim.tv_sec;
	return true;
}
