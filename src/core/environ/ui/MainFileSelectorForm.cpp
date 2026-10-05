#include "MainFileSelectorForm.h"
#include "DesktopFileLayout.h"
#include "ui/UILayout.h"
#include "platform/CCApplication.h"
#include "DesktopScroll.h"
#include "cocos2d.h"
#include "cocostudio/CocoLoader.h"
#include "cocostudio/CCSSceneReader.h"
#include "Application.h"
#include "Platform.h"
#include "cocostudio/ActionTimeline/CCActionTimeline.h"
#include "ui/UIText.h"
#include "ui/UIHelper.h"
#include "ui/UIButton.h"
#include "ui/UIListView.h"
#include "cocos2d/MainScene.h"
#include "ConfigManager/LocaleConfigManager.h"
#include "ConfigManager/IndividualConfigManager.h"
#include "GlobalPreferenceForm.h"
#include "IndividualPreferenceForm.h"
#include "MessageBox.h"
#include "SimpleMediaFilePlayer.h"
#include "tinyxml2/tinyxml2.h"
#include "StorageImpl.h"
#include "TipsHelpForm.h"
#include "XP3RepackForm.h"
#include "cocos2d/CustomFileUtils.h"

using namespace cocos2d;
using namespace cocos2d::ui;

const float UI_ACTION_DUR = 0.3f;
const char * const FileName_NaviBar = "ui/NaviBarWithMenu.csb";
const char * const FileName_Body = "ui/TableView.csb";
//const char * const FileName_BottomBar = "ui/BottomBar.csb";
//const char * const FileName_BtnPref = "ui/button/Pref.csb";
const char * const FileName_RecentPathListXML = "recentpath.xml";

bool TVPIsFirstLaunch = false;

std::deque<std::string> _HistoryPath;

static void _AskExit() {
	if (TVPShowSimpleMessageBoxYesNo(
		LocaleConfigManager::GetInstance()->GetText("sure_to_exit"),
		"Kirikinux2") == 0) TVPExitApplication(0);
}

bool TVPCheckIsVideoFile(const char *uri);
static std::string _GetHistoryXMLPath() {
	return TVPGetInternalPreferencePath() + FileName_RecentPathListXML;
}

static void _LoadHistory() {
	std::string xmlpath = _GetHistoryXMLPath();
	tinyxml2::XMLDocument doc;
	if (!doc.LoadFile(xmlpath.c_str())) {
		tinyxml2::XMLElement *rootElement = doc.RootElement();
		if (rootElement) {
			for (tinyxml2::XMLElement *item = rootElement->FirstChildElement("Item"); item; item = item->NextSiblingElement("Item")) {
				const char *path = item->Attribute("Path");
				if (path) {
					_HistoryPath.emplace_back(path);
				}
			}
		}
	} else {
		TVPIsFirstLaunch = true;
	}
}

static void _SaveHistory() {
	std::string xmlpath = _GetHistoryXMLPath();

	if (_HistoryPath.empty() && !FileUtils::getInstance()->isFileExist(xmlpath)) return;

	tinyxml2::XMLDocument doc;
	doc.LinkEndChild(doc.NewDeclaration());
	tinyxml2::XMLElement *rootElement = doc.NewElement("RecentPathList");
	for (const std::string& path : _HistoryPath) {
		tinyxml2::XMLElement *item = doc.NewElement("Item");
		item->SetAttribute("Path", path.c_str());
		rootElement->LinkEndChild(item);
	}

	doc.LinkEndChild(rootElement);
	doc.SaveFile(xmlpath.c_str());
}

static void _RemoveHistory(const std::string &path) {
	auto it = std::find(_HistoryPath.begin(), _HistoryPath.end(), path);
	if (it != _HistoryPath.end()) {
		_HistoryPath.erase(it);
		_SaveHistory();
	}
}

static void _AddHistory(const std::string &path) {
	if (!_HistoryPath.empty() && _HistoryPath.front() == path) return;
	_RemoveHistory(path);
	_HistoryPath.emplace_front(path);
	_SaveHistory();
}

static bool _CheckGameFolder(const std::string &path) {
	bool isValidGameFolder = false;
	std::vector<std::string> subFolders;
	TVPListDir(path, [&](const std::string &name, int mask) {
		if (isValidGameFolder || name.empty() || name.front() == '.') return;
		if (mask & S_IFREG) {
			std::string lowername = name;
			std::transform(lowername.begin(), lowername.end(), lowername.begin(), [](int c)->int {
				if (c <= 'Z' && c >= 'A')
					return c - ('A' - 'a');
				return c;
			});
			size_t pos = lowername.rfind('.');
			if (pos == lowername.npos) return;
			if (lowername.substr(pos) == ".xp3") {
				isValidGameFolder = true;
			}
		} else if (mask & S_IFDIR) {
			subFolders.emplace_back(path + "/" + name);
		}
	});
	while (!isValidGameFolder) {
		if (subFolders.empty()) break;
		isValidGameFolder = _CheckGameFolder(subFolders.back());
		subFolders.pop_back();
	}
	return isValidGameFolder;
}

TVPMainFileSelectorForm::TVPMainFileSelectorForm() {
	_menu = nullptr;
}

void TVPMainFileSelectorForm::onEnter()
{
	inherit::onEnter();
	if (_historyList) {
		_historyList->doLayout();
		auto & allcell = _historyList->getItems();
		for (Widget* cell : allcell) {
			static_cast<HistoryCell*>(cell)->rearrangeLayout();
		}
	}
}

void TVPMainFileSelectorForm::rearrangeLayout() {
    bool menuShown=isMenuShowed();
    if (_browserRoot) {
        _browserRoot->setContentSize(TVPMainScene::GetInstance()->getUINodeSize());
        ui::Helper::doLayout(_browserRoot);
    }
    inherit::rearrangeLayout();
    layoutDesktopMenu(menuShown);
    if (_historyList) {
        for (Widget *cell : _historyList->getItems()) {
            Size size=cell->getContentSize();size.width=_historyList->getContentSize().width;
            cell->setContentSize(size);
        }
        _historyList->requestDoLayout();
    }
}

void TVPMainFileSelectorForm::bindBodyController(const NodeMap &allNodes) {
	TVPBaseFileSelectorForm::bindBodyController(allNodes);

	if (NaviBar.Right) {
		NaviBar.Right->addClickEventListener(std::bind(&TVPMainFileSelectorForm::showMenu, this, std::placeholders::_1));
	}
}

extern "C" void TVPGL_ASM_Test();
void TVPMainFileSelectorForm::show() {
#ifdef _DEBUG
	TVPGL_ASM_Test();
#endif
	ListHistory(); // filter history data

	bool first = true;
	std::string lastpath;
	if (!_HistoryPath.empty()) lastpath = _HistoryPath.front();
	while (first || (lastpath.size() > RootPathLen && !FileUtils::getInstance()->isDirectoryExist(lastpath))) {
		first = false;
		std::pair<std::string, std::string> split_path = PathSplit(lastpath);
		if (split_path.second.empty()) {
			lastpath.clear();
			break;
		}
		lastpath = split_path.first;
	}
	if (lastpath.size() <= RootPathLen) {
		lastpath = TVPGetDriverPath()[0];
	}
	ListDir(lastpath); // getCurrentDir()
	// TODO show usage
}

static const std::string str_startup_tjs("startup.tjs");

bool TVPMainFileSelectorForm::CheckDir(const std::string &path) {
	for (const FileInfo &info : CurrentDirList) {
		if (info.NameForCompare == str_startup_tjs) return true;
	}
	return false;
}

int TVPCheckArchive(const ttstr &localname);
void TVPMainFileSelectorForm::onCellClicked(int idx) {
	FileInfo info = CurrentDirList[idx];
	TVPBaseFileSelectorForm::onCellClicked(idx);
	int archiveType;
	if (info.IsDir) {
		if (CheckDir(info.FullPath)) {
			startup(info.FullPath);
		}
	} else if ((archiveType = TVPCheckArchive(info.FullPath.c_str())) == 1) {
		startup(info.FullPath);
	} else if (archiveType == 0 && TVPCheckIsVideoFile(info.FullPath.c_str())) {
		SimpleMediaFilePlayer *player = SimpleMediaFilePlayer::create();
		TVPMainScene::GetInstance()->addChild(player, 10);// pushUIForm(player);
		player->PlayFile(info.FullPath.c_str());
	} else if (archiveType && FileUtils::getInstance()->getFileExtension(info.NameForCompare) == ".skin") {
		// maybe skin
		if (TVPSkinManager::Check(info.FullPath)) {
			std::vector<ttstr> btns;
			btns.emplace_back("Direct Use");
			btns.emplace_back("Install");
			btns.emplace_back("Cancel");
			switch (TVPShowSimpleMessageBox("Install or direct use it ? (restart needed)", "Skin found", btns)) {
			case 0: // direct use
				TVPSkinManager::Use(info.FullPath);
				TVPShowSimpleMessageBox("Active after restart.", "Skin");
				break;
			case 1: // install
				TVPSkinManager::InstallAndUse(info.FullPath);
				TVPShowSimpleMessageBox("Active after restart.", "Skin");
				break;
			default:
				break;
			}
		}
	}
}

void TVPMainFileSelectorForm::getShortCutDirList(std::vector<std::string> &pathlist) {
	if (!_lastpath.empty()) {
		pathlist.emplace_back(_lastpath);
	}
	TVPBaseFileSelectorForm::getShortCutDirList(pathlist);
}

TVPMainFileSelectorForm * TVPMainFileSelectorForm::create() {
	TVPMainFileSelectorForm *ret = new  TVPMainFileSelectorForm();
	ret->autorelease();
	ret->initFromFile();
	ret->show();
	return ret;
}

void TVPMainFileSelectorForm::initFromFile()
{
	_LoadHistory();
//	if (!_HistoryPath.empty())
	{
		CSBReader reader;
		Node *root = reader.Load("ui/MainFileSelector.csb");
        _browserRoot=root;
		_fileList = reader.findController("fileList");
		_historyList = static_cast<ListView*>(reader.findController("recentList"));
		// TODO new node
		_fileOperateMenuNode = _historyList;
		LocaleConfigManager::GetInstance()->initText(static_cast<Text*>(reader.findController("recentTitle", false)));
		addChild(root);
		Size sceneSize = TVPMainScene::GetInstance()->getUINodeSize();
		setContentSize(sceneSize);
		root->setContentSize(sceneSize);
		ui::Helper::doLayout(root);
	}
	inherit::initFromFile(FileName_NaviBar, FileName_Body, nullptr/*FileName_BottomBar*/, _fileList);
}

// std::string _getLastPathFilePath() {
// 	return TVPGetInternalPreferencePath() + "lastpath.txt";
// }

void TVPMainFileSelectorForm::startup(const std::string &path) {
	if (TVPIsFirstLaunch) {
		TVPTipsHelpForm::show()->setOnExitCallback([this, path](){
			scheduleOnce([this, path](float) {doStartup(path); }, 0, "startup");
		});
	} else {
		doStartup(path);
	}
}

void TVPMainFileSelectorForm::doStartup(const std::string &path) {
	if (TVPMainScene::GetInstance()->startupFrom(path)) {
		if (GlobalConfigManager::GetInstance()->GetValue<bool>("remember_last_path", true)) {
			_AddHistory(path);
		}
	}
}

void TVPMainFileSelectorForm::openGame() {
	hideMenu(nullptr);
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
	std::string path = TVPSelectGamePath(CurrentPath);
	if (path.empty()) return; // cancel leaves the current browser/history intact
	std::string lower = path;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
	if (PathSplit(lower).second == "startup.tjs") path = PathSplit(path).first;
	bool valid = false;
	if (TVPCheckExistentLocalFolder(path)) {
		bool hasStartup = false;
		std::vector<std::string> archives;
		std::string dataArchive;
		TVPListDir(path, [&](const std::string &name, int mask) {
			if (!(mask & S_IFREG)) return;
			std::string file = name;
			std::transform(file.begin(), file.end(), file.begin(), [](unsigned char c) { return std::tolower(c); });
			if (file == "startup.tjs") hasStartup = true;
			else if (file.size() > 4 && file.compare(file.size() - 4, 4, ".xp3") == 0) {
				archives.push_back(path + "/" + name);
				if (file == "data.xp3") dataArchive = archives.back();
			}
		});
		if (hasStartup) valid = true;
		else {
			// A packed game directory needs its main archive mounted first.
			// Never guess between several patch/resource archives without data.xp3.
			if (!dataArchive.empty()) path = dataArchive;
			else if (archives.size() == 1) path = archives.front();
			if (!TVPCheckExistentLocalFolder(path)) valid = TVPCheckArchive(ttstr(path)) == 1;
		}
	} else {
		valid = TVPCheckArchive(ttstr(path)) == 1;
	}
	if (!valid) {
		auto *locale = LocaleConfigManager::GetInstance();
		TVPShowSimpleMessageBox(locale->GetText("open_game_invalid"), locale->GetText("menu_open_game"));
		return;
	}
	startup(path);
#else
	// Other platforms keep their in-app file browser; directory creation is
	// no longer exposed as a game-launch action.
	ListDir(CurrentPath);
#endif
}

std::string TVPGetOpenGLInfo();
void TVPOpenPatchLibUrl();

bool TVPMainFileSelectorForm::acceptsDesktopScroll(Node *node) {
    if (!isMenuShowed()) return true;
    while(node && node!=this) { if(node==_menu)return true;node=node->getParent(); }
    return false;
}

void TVPMainFileSelectorForm::layoutDesktopMenu(bool shown) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    if (!_menu) return;
    Size scene=getContentSize(),size=_menu->getContentSize();
    float scale=TVPMainScene::GetInstance()->getUIScale();
    size.height=scene.height/scale;
    _menu->setScale(scale);_menu->setContentSize(size);ui::Helper::doLayout(_menu);
    _menu->stopAllActions();_menu->setPosition(Vec2(shown?scene.width-size.width*scale:scene.width,0));
    _mask->setContentSize(scene);_touchHideMenu->setContentSize(scene);
#endif
}

void TVPMainFileSelectorForm::showMenu(Ref*) {
	if (!_menu) {
		Size uiSize = getContentSize();
		CSBReader reader;
		_menu = reader.Load("ui/MenuList.csb");
		_menu->setAnchorPoint(Vec2::ZERO);
		_menu->setPosition(Vec2(uiSize.width, 0));
		_mask = LayerColor::create(Color4B::BLACK, uiSize.width, uiSize.height);
		_mask->setOpacity(0);
		_touchHideMenu = ui::Widget::create();
		_touchHideMenu->setAnchorPoint(Vec2::ZERO);
		_touchHideMenu->setContentSize(uiSize);
		_touchHideMenu->addClickEventListener([this](Ref*) {
			if (isMenuShowed())
				hideMenu(nullptr);
		});
		_mask->addChild(_touchHideMenu);
		addChild(_mask);
		addChild(_menu);
		if (uiSize.width > uiSize.height) {
			uiSize.width /= 3;
		} else {
			uiSize.width *= 0.6f;
		}
		Size menuSize = _menu->getContentSize();
		float scale = uiSize.width / menuSize.width;
		menuSize.height = uiSize.height / scale;
		_menu->setScale(scale);
		_menu->setContentSize(menuSize);
		ui::Helper::doLayout(_menu);

		newLocalPref = reader.findController("newLocalPref");
		localPref = reader.findController("localPref");
		sizeNewLocalPref = newLocalPref->getContentSize();
		sizeLocalPref = localPref->getContentSize();

		_menuList = dynamic_cast<ui::ListView*>(reader.findController("menulist"));
        layoutDesktopMenu(false);

		// captions
		LocaleConfigManager *localeMgr = LocaleConfigManager::GetInstance();
		localeMgr->initText(reader.findController<Text>("titleRotate"));
		localeMgr->initText(reader.findController<Text>("titleGlobalPref"));
		localeMgr->initText(reader.findController<Text>("titleNewLocalPref"));
		localeMgr->initText(reader.findController<Text>("titleLocalPref"));
		localeMgr->initText(reader.findController<Text>("titleHelp"));
		localeMgr->initText(reader.findController<Text>("titleAbout"));
		localeMgr->initText(reader.findController<Text>("titleExit"));
		localeMgr->initText(reader.findController<Text>("titleRepack"));
		localeMgr->initText(reader.findController<Text>("titleNewFolder"), "menu_open_game");

		// button events
		reader.findWidget("btnRotate")->addClickEventListener([](Ref*) {
			TVPMainScene::GetInstance()->pushUIForm(TVPGlobalPreferenceForm::create());
		});
		reader.findWidget("btnGlobalPref")->addClickEventListener([](Ref*) {
			TVPMainScene::GetInstance()->pushUIForm(TVPGlobalPreferenceForm::create());
		});
		reader.findWidget("btnNewLocalPref")->addClickEventListener([this](Ref*) {
			if (IndividualConfigManager::GetInstance()->CreatePreferenceAt(CurrentPath)) {
				TVPMainScene::GetInstance()->pushUIForm(IndividualPreferenceForm::create());
				hideMenu(nullptr);
			}
		});
		reader.findWidget("btnLocalPref")->addClickEventListener([this](Ref*) {
			onShowPreferenceConfigAt(CurrentPath);
		});
		reader.findWidget("btnHelp")->addClickEventListener([this](Ref*) {
			TVPTipsHelpForm::show();
		});
		bool showSimpleAbout = false;
		if(showSimpleAbout) {
			reader.findWidget("btnAbout")->addClickEventListener([](Ref*) {
				std::string versionText = "Version ";
				versionText += TVPGetPackageVersionString();

				std::string btnText = LocaleConfigManager::GetInstance()->GetText("ok");
				const char *pszBtnText = btnText.c_str();
				std::string strCaption = LocaleConfigManager::GetInstance()->GetText("menu_about");
				const char *caption = strCaption.c_str();
				TVPShowSimpleMessageBox(versionText.c_str(), caption, 1, &pszBtnText);
			});
			reader.findWidget("btnExit")->addClickEventListener([](Ref*) {
				if (TVPShowSimpleMessageBoxYesNo(
					LocaleConfigManager::GetInstance()->GetText("sure_to_exit"),
					"XP3Player") == 0) TVPExitApplication(0);
			});
		} else {
			reader.findWidget("btnAbout")->addClickEventListener([](Ref*) {
				std::string versionText = "Version ";
				versionText += TVPGetPackageVersionString();
				versionText += "\n";
				versionText += LocaleConfigManager::GetInstance()->GetText("about_content");

				const char * pszBtnText[] = {
					LocaleConfigManager::GetInstance()->GetText("ok").c_str(),
					LocaleConfigManager::GetInstance()->GetText("browse_patch_lib").c_str(),
					LocaleConfigManager::GetInstance()->GetText("device_info").c_str(),
                    "GitHub",
				};

				std::string strCaption = LocaleConfigManager::GetInstance()->GetText("menu_about");
				int n = TVPShowSimpleMessageBox(versionText.c_str(), strCaption.c_str(),
					sizeof(pszBtnText) / sizeof(pszBtnText[0]), pszBtnText);

				switch (n) {
                case 3:
                    cocos2d::Application::getInstance()->openURL("https://github.com/Comet387/kirikinux2");
                    break;
				case 1:
					TVPOpenPatchLibUrl();
					break;
				case 2:
					cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread([]{
						std::string text = TVPGetOpenGLInfo();
						const char *pOK = LocaleConfigManager::GetInstance()->GetText("ok").c_str();
						TVPShowSimpleMessageBox(text.c_str(),
							LocaleConfigManager::GetInstance()->GetText("device_info").c_str(),
							1, &pOK);
					});
					break;
				}
			});
			reader.findWidget("btnExit")->addClickEventListener([](Ref*) {
				_AskExit();
			});
		}
		reader.findWidget("btnRepack")->addClickEventListener([this](Ref*) {
			TVPProcessXP3Repack(CurrentPath);
			hideMenu(nullptr);
		});
		reader.findWidget("btnNewFolder")->addClickEventListener([this](Ref*) { openGame(); });

	}
	const Size &uiSize = getContentSize();
	const Vec2 &pos = _menu->getPosition();
	const Size &size = _menu->getContentSize();
	float w = size.width * _menu->getScale();
	if (pos.x > uiSize.width - w / 10.0f) {
		if (IndividualConfigManager::CheckExistAt(CurrentPath)) {
			localPref->setVisible(true);
			localPref->setContentSize(sizeLocalPref);
			newLocalPref->setVisible(false);
			newLocalPref->setContentSize(Size::ZERO);
		} else {
			newLocalPref->setVisible(true);
			newLocalPref->setContentSize(sizeNewLocalPref);
			localPref->setVisible(false);
			localPref->setContentSize(Size::ZERO);
		}
		_menuList->requestDoLayout();
		_mask->stopAllActions();
		_mask->runAction(FadeTo::create(UI_ACTION_DUR, 128));
		_menu->stopAllActions();
		_menu->runAction(EaseQuadraticActionOut::create(
			MoveTo::create(UI_ACTION_DUR, Vec2(uiSize.width - w, pos.y))));
		_touchHideMenu->setTouchEnabled(true);
	}
}

void TVPMainFileSelectorForm::hideMenu(cocos2d::Ref*)
{
	if (!_menu) return;
	_mask->stopAllActions();
	_mask->runAction(FadeOut::create(UI_ACTION_DUR));
	_menu->stopAllActions();
	_menu->runAction(EaseQuadraticActionOut::create(
		MoveTo::create(UI_ACTION_DUR, Vec2(getContentSize().width, _menu->getPositionY()))));
	_touchHideMenu->setTouchEnabled(false);
}

bool TVPMainFileSelectorForm::isMenuShowed()
{
	if (!_menu) return false;
	const Vec2 &pos = _menu->getPosition();
	const Size &size = _menu->getContentSize();
	float w = size.width * _menu->getScale();
	if (pos.x < getContentSize().width - w * 0.9f) {
		return true;
	}
	return false;
}

bool TVPMainFileSelectorForm::isMenuShrinked()
{
	if (!_menu) return true;
	const Vec2 &pos = _menu->getPosition();
	const Size &size = _menu->getContentSize();
	float w = size.width * _menu->getScale();
	if (pos.x > getContentSize().width - w / 10.0f) {
		return false;
	}
	return true;
}

void TVPMainFileSelectorForm::onShowPreferenceConfigAt(const std::string &path)
{
	if (IndividualConfigManager::GetInstance()->UsePreferenceAt(path)) {
		TVPMainScene::GetInstance()->pushUIForm(IndividualPreferenceForm::create());
	}
}

void TVPMainFileSelectorForm::ListHistory()
{
	if (!_historyList) return;
	_historyList->removeAllChildren();
    TVPEnableDesktopScroll(_historyList);
	HistoryCell *nullcell = new HistoryCell();
	nullcell->autorelease();
	nullcell->init();
	Size cellsize = _historyList->getContentSize();
	cellsize.height = 190;
	nullcell->setContentSize(cellsize);
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
	auto *locale = LocaleConfigManager::GetInstance();
	const std::string font = FileUtils::getInstance()->fullPathForFilename("DroidSansFallback.ttf");
	auto *background = Layout::create();
	background->setName("open_game");
	background->setAnchorPoint(Vec2::ZERO);
	background->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
	background->setBackGroundColor(Color3B(54, 75, 60));
	background->setContentSize(Size(std::max(100.f, cellsize.width - 48), 78));
	background->setPosition(Vec2(24, 94));
	nullcell->addChild(background);
	auto *open = Button::create();
	open->setTitleFontName(font);
	open->setTitleFontSize(40);
	open->setTitleText(locale->GetText("menu_open_game"));
	open->ignoreContentAdaptWithSize(false);
	open->setContentSize(background->getContentSize());
	open->setPosition(Vec2(background->getContentSize().width / 2, 39));
	open->addClickEventListener([this](Ref*) { openGame(); });
	background->addChild(open);
	auto *hint = Text::create(locale->GetText("open_game_hint"), font, 24);
	hint->setName("open_game_hint");
	hint->ignoreContentAdaptWithSize(false);
	hint->setTextAreaSize(Size(std::max(100.f, cellsize.width - 48), 72));
	hint->setAnchorPoint(Vec2(0, 1));
	hint->setPosition(Vec2(24, 83));
	nullcell->addChild(hint);
#endif
	nullcell->rearrangeLayout();
	_historyList->pushBackCustomItem(nullcell);
	for (auto it = _HistoryPath.begin(); it != _HistoryPath.end();) {
		const std::string &fullpath = *it;
		HistoryCell *cell;
		if (TVPCheckExistentLocalFile(fullpath) || TVPCheckExistentLocalFolder(fullpath)) {
			std::pair<std::string, std::string> split_path = PathSplit(fullpath);
			std::string lastname = split_path.second;
			std::string path = split_path.first;
			split_path = PathSplit(path);
			cell = HistoryCell::create(fullpath, split_path.first + "/", split_path.second, "/" + lastname);
			Widget::ccWidgetClickCallback funcConf;
			if (IndividualConfigManager::CheckExistAt(path))
				funcConf = [this, path](Ref*){ onShowPreferenceConfigAt(path); };
			cell->initFunction(std::bind(&TVPMainFileSelectorForm::RemoveHistoryCell, this, std::placeholders::_1, cell),
				[this, path](Ref*){ ListDir(path); }, funcConf, [this, fullpath](Ref*) { startup(fullpath); });
			Size cellsize = cell->getContentSize();
			cellsize.width = _historyList->getContentSize().width;
			cell->setContentSize(cellsize);
			_historyList->pushBackCustomItem(cell);
			++it;
		} else {
			it = _HistoryPath.erase(it);
			continue;
		}
	}
	nullcell = new HistoryCell();
	nullcell->autorelease();
	nullcell->init();
	cellsize.height = 100;
	nullcell->setContentSize(cellsize);
	_historyList->pushBackCustomItem(nullcell);
}

void TVPMainFileSelectorForm::RemoveHistoryCell(cocos2d::Ref* btn, HistoryCell* cell)
{
	static_cast<Widget*>(btn)->setEnabled(false);
	cell->runAction(Sequence::createWithTwoActions(
		EaseQuadraticActionOut::create(MoveBy::create(0.25, Vec2(-cell->getContentSize().width, 0))),
		CallFuncN::create([this](Node* p){
		HistoryCell* cell = static_cast<HistoryCell*>(p);
		ssize_t idx = _historyList->getIndex(cell);
		if (idx < 0) return;
		_historyList->removeItem(idx);
	})));
	_RemoveHistory(cell->getFullpath());
	_SaveHistory();
}

void TVPMainFileSelectorForm::onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event) {
	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_BACK) {
		if (isMenuShowed()) {
			hideMenu(nullptr);
		} else {
			_AskExit();
		}
	} else if (keyCode == EventKeyboard::KeyCode::KEY_MENU) {
		if (isMenuShrinked()) {
			showMenu(nullptr);
		}
	} else {
		inherit::onKeyPressed(keyCode, event);
	}
}

void TVPMainFileSelectorForm::HistoryCell::initInfo(const std::string &fullpath, const std::string &prefix, const std::string &pathname, const std::string &filename)
{
	_fullpath = fullpath;

#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    _displayName = pathname + filename;
    auto *root = Layout::create();
    root->setAnchorPoint(Vec2::ZERO);
    root->setClippingEnabled(true);
    root->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    root->setBackGroundColor(Color3B(40, 46, 41));
    _root = root;
    const std::string font = FileUtils::getInstance()->fullPathForFilename("DroidSansFallback.ttf");
    auto button = [&](const std::string &title) {
        auto *node = Button::create("img/empty.png", "img/white.png");
        node->setTitleFontName(font);
        node->setTitleFontSize(36);
        node->setTitleText(title);
        node->ignoreContentAdaptWithSize(false);
        node->setPropagateTouchEvents(true);
        root->addChild(node);
        return node;
    };
    _btn_delete = button("×");
    _btn_jump = button(">");
    _btn_conf = button("⚙");
    _btn_play = button("▶");
    _path = Text::create("", font, 44);
    _prefix = Text::create("", font, 30);
    _prefix->setTextColor(Color4B(176, 186, 177, 255));
    for (auto *label : {_path, _prefix}) {
        label->setAnchorPoint(Vec2(0, .5f));
        root->addChild(label);
    }
    addChild(root);
    setContentSize(Size(640, 238));
    rearrangeLayout();
#else

	CSBReader reader;
	_root = reader.Load("ui/RecentListItem.csb");
	_scrollview = static_cast<cocos2d::ui::ScrollView*>(reader.findController("scrollview"));
	_btn_delete = static_cast<cocos2d::ui::Widget*>(reader.findController("btn_delete"));
	_btn_jump = static_cast<cocos2d::ui::Widget*>(reader.findController("btn_jump"));
	_btn_conf = static_cast<cocos2d::ui::Widget*>(reader.findController("btn_conf"));
	_btn_play = static_cast<cocos2d::ui::Widget*>(reader.findController("btn_play"));
	_prefix = static_cast<cocos2d::ui::Text*>(reader.findController("prefix"));
	_path = static_cast<cocos2d::ui::Text*>(reader.findController("path"));
	_file = static_cast<cocos2d::ui::Text*>(reader.findController("file"));
	_panel_delete = reader.findController("panel_delete");
	if (!_panel_delete) _panel_delete = _btn_delete;
	_scrollview->setScrollBarEnabled(false);
	_scrollview->setPropagateTouchEvents(true);

	_prefix->setString(prefix);
	_path->setString(pathname);
	_file->setString(filename);

	setContentSize(_root->getContentSize());
	addChild(_root);
#endif
}

void TVPMainFileSelectorForm::HistoryCell::rearrangeLayout()
{
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    const float width = std::max(1.f, getContentSize().width - 16.f / .30f);
    const float padding = std::min(24.f, width / 8);
    if (!_root) {
        // The launch shortcut is also a list item and must follow list resize.
        if (auto *intro = getChildByName("open_game")) {
            intro->setContentSize(Size(std::max(1.f, width - padding * 2), 78));
            intro->setPositionX(padding);
            for (Node *child : intro->getChildren()) {
                child->setContentSize(intro->getContentSize());
                child->setPosition(intro->getContentSize() / 2);
            }
        }
        if (auto *hint = dynamic_cast<Text*>(getChildByName("open_game_hint"))) {
            hint->setTextAreaSize(Size(std::max(1.f, width - padding * 2), 72));
            hint->setPositionX(padding);
        }
        return;
    }
    _root->setContentSize(Size(width, 238));
    _path->setPosition(Vec2(padding, 191));
    _prefix->setPosition(Vec2(padding, 136));
    TVPFitFileText(_path, _displayName, std::max(0.f, width - 2 * padding));
    TVPFitFileText(_prefix, _fullpath, std::max(0.f, width - 2 * padding));
    Widget *buttons[] = {_btn_delete, _btn_jump, _btn_conf, _btn_play};
    const float column = std::max(0.f, width - padding * 2) / 4;
    for (int i = 0; i < 4; ++i) {
        static_cast<Button*>(buttons[i])->setTitleFontSize(std::min(36.f, std::max(1.f, column * .6f)));
        buttons[i]->setContentSize(Size(column, 88));
        buttons[i]->setPosition(Vec2(padding + column * (i + .5f), 55));
    }
#else

	if (!_root) return;
	_root->setContentSize(this->getContentSize());
	ui::Helper::doLayout(_root);
	Vec2 pt = Vec2::ZERO;
	pt.x = _file->getContentSize().width;
	Vec2 ptWorld = _file->convertToWorldSpace(pt);
	Size viewSize = _scrollview->getContentSize();
	Node *container = _scrollview->getInnerContainer();
	pt = container->convertToNodeSpace(ptWorld);
	float btnw = _panel_delete->getContentSize().width;
	float offsetx = 0;
	if (pt.x > viewSize.width) {
		float neww = pt.x;
		pt.y = 0; pt.x = _path->getContentSize().width;
		ptWorld = _path->convertToWorldSpace(pt);
		pt = container->convertToNodeSpace(ptWorld);
		if (pt.x > viewSize.width) {
			offsetx = viewSize.width - pt.x;
		}
		viewSize.width = neww;
	}
	_panel_delete->setPositionX(viewSize.width + btnw);
	viewSize.width += btnw + btnw;
	_scrollview->setInnerContainerSize(viewSize);
	container->setPosition(offsetx, 0);
#endif
}

void TVPMainFileSelectorForm::HistoryCell::initFunction(const ccWidgetClickCallback &funcDel, const ccWidgetClickCallback &funcJump, const ccWidgetClickCallback &funcConf, const ccWidgetClickCallback &funcPlay)
{
	_btn_delete->addClickEventListener(funcDel);
	_btn_play->addClickEventListener(funcPlay);
	if (funcConf) _btn_conf->addClickEventListener(funcConf);
	else _btn_conf->setVisible(false);
	_btn_jump->addClickEventListener(funcJump);
}

void TVPMainFileSelectorForm::HistoryCell::onSizeChanged()
{
    Widget::onSizeChanged();
    rearrangeLayout();
}
