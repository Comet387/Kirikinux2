#include "CustomFileUtils.h"
#include <limits>
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
#include "platform/win32/CCFileUtils-win32.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_IOS
#import <Foundation/NSBundle.h>
#import "platform/apple/CCFileUtils-apple.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
#include "platform/android/CCFileUtils-android.h"
#elif CC_TARGET_PLATFORM == CC_PLATFORM_LINUX // [kirikinux2]
#include "platform/linux/CCFileUtils-linux.h"
#endif
#ifdef MINIZIP_FROM_SYSTEM
#include <minizip/unzip.h>
#else // from our embedded sources
#include "external/unzip/unzip.h"
#endif
#include "ConfigManager/LocaleConfigManager.h"

// [kirikinux2] cocos2d-x >= 3.15 made the FileUtils readers const and routes all of them
// through getContents(); the engine build Kirikiroid2 was written for still had the old
// non-const virtuals.  Keep the old overrides for that engine, add getContents() here.
#if COCOS2D_VERSION >= 0x00031500
#define KR2_FILEUTILS_GETCONTENTS 1
#define KR2_FILEUTILS_LEGACY_OVERRIDE
#else
#define KR2_FILEUTILS_LEGACY_OVERRIDE override
#endif
NS_CC_BEGIN

typedef
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
FileUtilsWin32
#elif CC_TARGET_PLATFORM == CC_PLATFORM_IOS
FileUtilsApple
#elif CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
FileUtilsAndroid
#elif CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
FileUtilsLinux
#endif
FileUtilsInherit;

class CustomFileUtils : public FileUtilsInherit
{
public:
	CustomFileUtils();

	void addAutoSearchArchive(const std::string& path);
	virtual std::string fullPathForFilename(const std::string &filename) const override;
	virtual std::string getStringFromFile(const std::string& filename) KR2_FILEUTILS_LEGACY_OVERRIDE;
	virtual Data getDataFromFile(const std::string& filename) KR2_FILEUTILS_LEGACY_OVERRIDE;
	virtual unsigned char* getFileData(const std::string& filename, const char* mode, ssize_t *size) KR2_FILEUTILS_LEGACY_OVERRIDE;
	virtual bool isFileExistInternal(const std::string& strFilePath) const override;
	virtual bool isDirectoryExistInternal(const std::string& dirPath) const override;
#ifdef KR2_FILEUTILS_GETCONTENTS
	using FileUtilsInherit::getContents;
	virtual Status getContents(const std::string& filename, ResizableBuffer* buffer) const override;
#endif
	virtual bool init() override {
		return FileUtilsInherit::init();
	}

private:
	unsigned char* getFileDataFromArchive(const std::string& filename, ssize_t *size);

	std::unordered_map<std::string, std::pair<unzFile, unz_file_pos> > _autoSearchArchive;
	std::mutex _lock;
};

CustomFileUtils::CustomFileUtils()
{
}

void CustomFileUtils::addAutoSearchArchive(const std::string& path)
{
	if (!this->isFileExist(path)) return;
	unzFile file = nullptr;
	file = unzOpen(FileUtils::getInstance()->getSuitableFOpen(path).c_str());
	unz_file_info file_info;
	do {
		unz_file_pos entry;
		if (unzGetFilePos(file, &entry) == UNZ_OK) {
			char filename_inzip[1024];
			if (unzGetCurrentFileInfo(file, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) == UNZ_OK) {
				_autoSearchArchive[filename_inzip] = std::make_pair(file, entry);
			}
		}
	} while (unzGoToNextFile(file) == UNZ_OK);
}

std::string CustomFileUtils::fullPathForFilename(const std::string &filename) const
{
	auto it = _autoSearchArchive.find(filename);
	if (_autoSearchArchive.end() != it) {
		return filename;
	}
	return FileUtilsInherit::fullPathForFilename(filename);
}

unsigned char* CustomFileUtils::getFileData(const std::string& filename, const char* mode, ssize_t *size)
{
	unsigned char* ret = getFileDataFromArchive(filename, size);
	if (ret) return ret;
	return FileUtilsInherit::getFileData(filename, mode, size);
}

bool CustomFileUtils::isFileExistInternal(const std::string& strFilePath) const
{
	auto it = _autoSearchArchive.find(strFilePath);
	if (_autoSearchArchive.end() != it) {
		return true;
	}
	return FileUtilsInherit::isFileExistInternal(strFilePath);
}

bool CustomFileUtils::isDirectoryExistInternal(const std::string& dirPath) const
{
	for (auto &it : _autoSearchArchive) {
		if (it.first.size() <= dirPath.size()) continue;
		if (!strncmp(it.first.c_str(), dirPath.c_str(), dirPath.size()) && it.first[dirPath.size()] == '/') {
			return true;
		}
	}
	return FileUtilsInherit::isDirectoryExistInternal(dirPath);
}

unsigned char* CustomFileUtils::getFileDataFromArchive(const std::string& filename, ssize_t *size)
{
	auto it = _autoSearchArchive.find(filename);
	if (_autoSearchArchive.end() != it) {
		std::lock_guard<std::mutex> lock(_lock);
		if (unzGoToFilePos(it->second.first, &it->second.second) != UNZ_OK) return nullptr;
		unz_file_info fileInfo;
		if (unzGetCurrentFileInfo(it->second.first, &fileInfo, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) return nullptr;
		// The stock Cocos reader needs an explicit open/close for each entry.
		if (fileInfo.uncompressed_size > static_cast<unsigned long>(std::numeric_limits<int>::max())) return nullptr;
		if (unzOpenCurrentFile(it->second.first) != UNZ_OK) return nullptr;
		unsigned char *buffer = (unsigned char*)malloc(fileInfo.uncompressed_size ? fileInfo.uncompressed_size : 1);
		if (!buffer) { unzCloseCurrentFile(it->second.first); return nullptr; }
		int readedSize = unzReadCurrentFile(it->second.first, buffer, static_cast<unsigned>(fileInfo.uncompressed_size));
		int closeResult = unzCloseCurrentFile(it->second.first);
		if (readedSize != (int)fileInfo.uncompressed_size || closeResult != UNZ_OK) {
			free(buffer);
			return nullptr;
		}
		*size = fileInfo.uncompressed_size;
		return buffer;
	}
	return nullptr;
}

cocos2d::Data CustomFileUtils::getDataFromFile(const std::string& filename)
{
	ssize_t size;
	unsigned char* buffer = getFileDataFromArchive(filename, &size);
	if (buffer) {
		Data ret;
		ret.fastSet(buffer, size);
		return ret;
	}
	return FileUtilsInherit::getDataFromFile(filename);
}

std::string CustomFileUtils::getStringFromFile(const std::string& filename)
{
	Data data = getDataFromFile(filename);
	if (data.isNull())
		return "";

	std::string ret((const char*)data.getBytes());
	return ret;
}

#ifdef KR2_FILEUTILS_GETCONTENTS
// [kirikinux2] every reader of cocos2d-x >= 3.15 ends up here, so skin archives keep working
FileUtils::Status CustomFileUtils::getContents(const std::string& filename, ResizableBuffer* buffer) const
{
	ssize_t size = 0;
	unsigned char *data = const_cast<CustomFileUtils*>(this)->getFileDataFromArchive(filename, &size);
	if (data) {
		buffer->resize(size);
		if (size > 0) memcpy(buffer->buffer(), data, size);
		free(data);
		return Status::OK;
	}
	return FileUtilsInherit::getContents(filename, buffer);
}
#endif

NS_CC_END

cocos2d::FileUtils *TVPCreateCustomFileUtils() {
	cocos2d::CustomFileUtils *ret = new cocos2d::CustomFileUtils;
	ret->init();
	return ret;
}

void TVPAddAutoSearchArchive(const std::string &path)
{
	cocos2d::CustomFileUtils *fileutils =static_cast<cocos2d::CustomFileUtils*>(cocos2d::FileUtils::getInstance());
	fileutils->addAutoSearchArchive(path);
}

#include "StorageImpl.h"
#include "Platform.h"
#include "ConfigManager/GlobalConfigManager.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

static bool TVPCopyFolder(const std::string &from, const std::string &to) {
	if (!TVPCheckExistentLocalFolder(to) && !TVPCreateFolders(to)) {
		return false;
	}

	bool success = true;
	TVPListDir(from, [&](const std::string &_name, int mask) {
		if (_name == "." || _name == "..") return;
		if (!success) return;
		if (mask & S_IFREG) {
			success = TVPCopyFile(from + "/" + _name, to + "/" + _name);
		}
		else if (mask & S_IFDIR) {
			success = TVPCopyFolder(from + "/" + _name, to + "/" + _name);
		}
	});
	return success;
}

bool TVPCopyFile(const std::string &from, const std::string &to)
{
	FILE * ffrom = fopen(from.c_str(), "rb");
	if (!ffrom) { // try folder copy
		return TVPCopyFolder(from, to);
	}
	FILE * fto = fopen(to.c_str(), "wb");
	if (!fto) {
		if (ffrom) fclose(ffrom);
		return false;
	}
	const int bufSize = 1 * 1024 * 1024;
	std::vector<char> buffer; buffer.resize(bufSize);
	int readed = 0;
	while ((readed = fread(&buffer.front(), 1, bufSize, ffrom))) {
		fwrite(&buffer.front(), 1, readed, fto);
	}
	fclose(ffrom);
	fclose(fto);
	return true;
}

TVPSkinManager* TVPSkinManager::getInstance()
{
	static TVPSkinManager instance;
	return &instance;
}

void TVPSkinManager::InitSkin()
{
	std::string skinpath = GlobalConfigManager::GetInstance()->GetValue<std::string>("skin_path", "");
	if (!skinpath.empty()) {
		if (!Check(skinpath)) {
			TVPShowSimpleMessageBox(
				LocaleConfigManager::GetInstance()->GetText("invalid_skin_desc"),
				LocaleConfigManager::GetInstance()->GetText("invalid_skin"));
			Reset();
		} else {
			static_cast<CustomFileUtils*>(FileUtils::getInstance())->addAutoSearchArchive(skinpath);
		}
	}
}

static const char *_adapted_skin_version = "1.3.4";

bool TVPSkinManager::Check(const std::string &path)
{
	if (!cocos2d::FileUtils::getInstance()->isFileExist(path)) {
		return false;
	}

	tinyxml2::XMLDocument doc;

	unzFile file = nullptr;
	file = unzOpen(FileUtils::getInstance()->getSuitableFOpen(path).c_str());
	unz_file_info file_info;
	do {
		unz_file_pos entry;
		if (unzGetFilePos(file, &entry) == UNZ_OK) {
			char filename_inzip[1024];
			if (unzGetCurrentFileInfo(file, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) == UNZ_OK) {
				if (strcmp(filename_inzip, "meta.xml")) {
					if (unzGoToFilePos(&file, &entry) != UNZ_OK)
						break;
					unsigned char *buffer = (unsigned char*)malloc(file_info.uncompressed_size);
					int readedSize = unzReadCurrentFile(&file, buffer, static_cast<unsigned>(file_info.uncompressed_size));
					if (readedSize != (int)file_info.uncompressed_size) {
						free(buffer);
						break;
					}
					doc.Parse((char*)buffer);
					free(buffer);
					break;
				}
			}
		}
	} while (unzGoToNextFile(file) == UNZ_OK);
	unzClose(file);

	tinyxml2::XMLElement* root = doc.RootElement();
	if (!root)
		return false;

	const char *s = root->Attribute("version");
	if (!s)
		return false;
	
	return !strcmp(s, _adapted_skin_version);
}

void TVPSkinManager::Reset()
{
	GlobalConfigManager::GetInstance()->SetValue("skin_path", "");
	GlobalConfigManager::GetInstance()->SaveToFile();
}

bool TVPSkinManager::Use(const std::string &skin_path)
{
	GlobalConfigManager::GetInstance()->SetValue("skin_path", skin_path);
	GlobalConfigManager::GetInstance()->SaveToFile();
	return true;
}

bool TVPSkinManager::InstallAndUse(const std::string &skin_path)
{
	std::string path = FileUtils::getInstance()->getWritablePath() + "default.skin";
	FileUtils::getInstance()->removeFile(path);
	if (!TVPCopyFile(skin_path, path)) {
		return false;
	}
	GlobalConfigManager::GetInstance()->SetValue("skin_path", path);
	return true;
}
