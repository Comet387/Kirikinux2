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

static void usage(const char *argv0) {
	printf("usage: %s [--size=WIDTHxHEIGHT] [--fullscreen] [game-dir | archive.xp3 [name=value ...]]\n"
	       "  without a game the original Kirikinux2 file selector is shown\n"
	       "  KIRIKINUX_GAME_DIR sets the folder the file selector starts in\n", argv0);
}

int main(int argc, char **argv) {
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
	return cocos2d::Application::getInstance()->run();
}
