// SPDX-License-Identifier: AGPL-3.0-only
//---------------------------------------------------------------------------
// kirikinux2 : process entry for Linux (Android: project/android/jni/src/
// SDL_android_main.cpp, win32: the cocos2d-x win32 main).  Everything after
// window creation is the original TVPAppDelegate (environ/cocos2d/AppDelegate.cpp).
//---------------------------------------------------------------------------
#include "cocos2d.h"
#include "AppDelegate.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern std::vector<std::string> TVPLinuxStartupArgs; // LinuxUtils.cpp, read by TVPCheckStartupArg()
void TVPLinuxInstallCrashHandler(); // LinuxCrashHandler.cpp
void TVPLinuxRequestClose();        // LinuxUtils.cpp: confirm / let the game decide
void TVPLinuxQuickExit(int code);   // LinuxUtils.cpp: flush stdio, _exit()

// The title bar's close button.  cocos2d-x would end its main loop, destroy the
// window and return from main(), running static destructors while the engine
// is still alive (SIGABRT in tTVPGraphicType::~tTVPGraphicType) and without
// asking anything.  Keep the window open and route the request instead.
static void onWindowCloseRequested(GLFWwindow *window) {
	glfwSetWindowShouldClose(window, GLFW_FALSE);
	cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread(TVPLinuxRequestClose);
}

static void usage(const char *argv0) {
	printf("usage: %s [--size=WIDTHxHEIGHT] [--fullscreen] [game-dir | archive.xp3 [name=value ...]]\n"
	       "  without a game the Kirikinux2 file selector is shown\n"
	       "  KIRIKINUX_GAME_DIR sets the folder the file selector starts in\n", argv0);
}

int main(int argc, char **argv) {
	TVPLinuxInstallCrashHandler();
	int width = 1280, height = 720;
	bool fullscreen = false;
	for (int i = 1; i < argc; ++i) {
		const char *a = argv[i];
		if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(argv[0]); return 0; }
		if (!strcmp(a, "--fullscreen")) { fullscreen = true; continue; }
		if (!strncmp(a, "--size=", 7)) {
			if (sscanf(a + 7, "%dx%d", &width, &height) != 2 || width <= 0 || height <= 0) {
				fprintf(stderr, "invalid --size, expected e.g. --size=1280x720\n");
				return 2;
			}
			continue;
		}
		TVPLinuxStartupArgs.emplace_back(a);
	}

	TVPAppDelegate app;
	// TVPAppDelegate::initGLContextAttrs() asks for RGBA8 + depth24 + stencil8; the window is
	// created here (so --size/--fullscreen work), therefore the attributes must be set first.
	// applicationDidFinishLaunching() only creates a view when none is set.
	GLContextAttrs attrs = { 8, 8, 8, 8, 24, 8 };
	cocos2d::GLView::setGLContextAttrs(attrs);
	cocos2d::GLView *glview = fullscreen
		? cocos2d::GLViewImpl::createWithFullScreen("Kirikinux2")
		: cocos2d::GLViewImpl::createWithRect("Kirikinux2", cocos2d::Rect(0, 0, width, height), 1.f, true);
	if (!glview) {
		fprintf(stderr, "Kirikinux2: could not create the OpenGL window\n");
		return 1;
	}
	cocos2d::Director::getInstance()->setOpenGLView(glview);
	if (GLFWwindow *window = static_cast<cocos2d::GLViewImpl*>(glview)->getWindow())
		glfwSetWindowCloseCallback(window, onWindowCloseRequested);
	const int ret = cocos2d::Application::getInstance()->run();
	TVPLinuxQuickExit(ret); // never run static destructors (see TVPExitApplication)
	return ret;
}
