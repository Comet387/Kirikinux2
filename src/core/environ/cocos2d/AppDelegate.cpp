#include "AppDelegate.h"
#include "MainScene.h"
#include "ui/MainFileSelectorForm.h"
#include "ui/extension/UIExtension.h"
#include "cocostudio/FlatBuffersSerialize.h"
#include "ConfigManager/LocaleConfigManager.h"
#include "ConfigManager/GlobalConfigManager.h"
#include "Application.h"
#include "Platform.h"
#include "ui/MessageBox.h"
#include "ui/GlobalPreferenceForm.h"
#include "CustomFileUtils.h"

USING_NS_CC;

cocos2d::FileUtils *TVPCreateCustomFileUtils();
extern "C" void SDL_SetMainReady(void);
extern std::thread::id TVPMainThreadID;
static Size designResolutionSize(960, 640);
bool TVPCheckStartupArg();
cocos2d::FileUtils *TVPCreateCustomFileUtils();

void TVPAppDelegate::applicationWillEnterForeground() {
	::Application->OnActivate();
	Director::getInstance()->startAnimation();
}

void TVPAppDelegate::applicationDidEnterBackground() {
	::Application->OnDeactivate();
	Director::getInstance()->stopAnimation();
}

bool TVPAppDelegate::applicationDidFinishLaunching() {
	SDL_SetMainReady();
	TVPMainThreadID = std::this_thread::get_id();
	cocos2d::log("applicationDidFinishLaunching");
	// initialize director
	FileUtils::setDelegate(TVPCreateCustomFileUtils());
	auto director = Director::getInstance();
	auto glview = director->getOpenGLView();
	if (!glview) {
		glview = GLViewImpl::create("kirikiri2 frame");
		director->setOpenGLView(glview);
#if CC_PLATFORM_WIN32 == CC_TARGET_PLATFORM
		glview->setFrameSize(960, 640);
#endif
	}
	// Set the design resolution
	Size screenSize = glview->getFrameSize();
	if (screenSize.width < screenSize.height) {
		std::swap(screenSize.width, screenSize.height);
	}
	Size designSize = designResolutionSize;
	designSize.height = designSize.width * screenSize.height / screenSize.width;
	glview->setDesignResolutionSize(designSize.width, designSize.height, ResolutionPolicy::SHOW_ALL);

	Size frameSize = glview->getFrameSize();

	std::vector<std::string> searchPath;

	// In this demo, we select resource according to the frame's height.
	// If the resource size is different from design resolution size, you need to set contentScaleFactor.
	// We use the ratio of resource's height to the height of design resolution,
	// this can make sure that the resource's height could fit for the height of design resolution.
	searchPath.emplace_back("res");

	TVPSkinManager::getInstance()->InitSkin();

	// set searching path
	FileUtils::getInstance()->setSearchPaths(searchPath);
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
	// Native GTK/Pango dialogs use the packaged face even on hosts without CJK fonts.
	extern void KR2LinuxSetUIFont(const std::string &path);
	KR2LinuxSetUIFont(FileUtils::getInstance()->fullPathForFilename("DroidSansFallback.ttf"));
#endif

	// turn on display FPS
	director->setDisplayStats(false);

	// set FPS. the default value is 1.0/60 if you don't call this
	director->setAnimationInterval(1.0f / 60);

	TVPInitUIExtension();

	// initialize something
	LocaleConfigManager::GetInstance()->Initialize(TVPGetCurrentLanguage());
	// create a scene. it's an autorelease object
	TVPMainScene *scene = TVPMainScene::CreateInstance();

#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    director->getEventDispatcher()->addCustomEventListener(GLViewImpl::EVENT_WINDOW_RESIZED, [this](EventCustom*) {
        Size size=Director::getInstance()->getOpenGLView()->getFrameSize();
        applicationScreenSizeChanged(size.width,size.height);
    });
#endif
	// run
	director->runWithScene(scene);

	//director->getConsole()->listenOnTCP(16006);

	scene->scheduleOnce([](float dt){
		TVPMainScene::GetInstance()->unschedule("launch");
		TVPGlobalPreferenceForm::Initialize();
		if (!TVPCheckStartupArg()) {
			TVPMainScene::GetInstance()->pushUIForm(TVPMainFileSelectorForm::create());
		}
	}, 0, "launch");

	return true;
}

void TVPAppDelegate::initGLContextAttrs() {
	GLContextAttrs glContextAttrs = {
		8, 8, 8, 8, 24, 8
	};
	GLView::setGLContextAttrs(glContextAttrs);
}

void TVPAppDelegate::applicationScreenSizeChanged(int newWidth, int newHeight)
{
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    if (newWidth <= 0 || newHeight <= 0) return;
    auto *view=Director::getInstance()->getOpenGLView();
    Size design(960.f,960.f*newHeight/newWidth);
    view->setDesignResolutionSize(design.width,design.height,ResolutionPolicy::SHOW_ALL);
    if (auto *scene=TVPMainScene::GetInstance()) scene->resizeDesktopView(design);
#endif
}

void TVPOpenPatchLibUrl() {
	cocos2d::Application::getInstance()->openURL("https://zeas2.github.io/Kirikiroid2_patch/patch");
}

#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
bool TVPAppDelegate::openURL(const std::string &url) {
    extern bool KR2LinuxOpenURL(const std::string &);
    return KR2LinuxOpenURL(url);
}
#endif
