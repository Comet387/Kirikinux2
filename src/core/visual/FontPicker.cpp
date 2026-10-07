// SPDX-License-Identifier: AGPL-3.0-only
// Engine side of the native font picker: supplies the registered face list,
// registers font files the user adds, and remembers them.
#include "tjsCommHead.h"
#include "FontPicker.h"

#if defined(LINUX) && !defined(__ANDROID__)
#include "FontImpl.h"
#include "DebugIntf.h"
#include "ConfigManager/GlobalConfigManager.h"
#include "../../../platform/linux/LinuxFontPicker.h"
#include <algorithm>
#include <string>
#include <vector>

static void TVPRememberUserFontFile(const std::string &path) {
	GlobalConfigManager *cfg = GlobalConfigManager::GetInstance();
	std::string files = cfg->GetValue<std::string>("user_font_files", "");
	size_t start = 0;
	while (start < files.size()) {
		size_t end = files.find('\n', start);
		if (end == std::string::npos) end = files.size();
		if (files.compare(start, end - start, path) == 0) return; // already known
		start = end + 1;
	}
	if (!files.empty() && files.back() != '\n') files += '\n';
	files += path;
	cfg->SetValue("user_font_files", files);
	cfg->SaveToFile();
}

int TVPShowFontPicker(const ttstr &caption, const ttstr &prompt, const ttstr &sample,
	const ttstr &initial, bool rememberFiles, ttstr &face, ttstr *file)
{
	KR2FontPickRequest req;
	req.title = caption.AsStdString();
	req.prompt = prompt.AsStdString();
	req.sample = sample.AsStdString();
	req.initial = initial.AsStdString();
	if (!req.initial.empty() && req.initial[0] == '@') req.initial.erase(0, 1); // vertical variant
	{
		std::vector<ttstr> list;
		TVPGetAllFontList(list);
		req.faces.reserve(list.size());
		for (const ttstr &n : list) req.faces.push_back(n.AsStdString());
	}
	req.addFile = [rememberFiles](const std::string &path, std::string &error) {
		std::vector<std::string> names;
		try {
			std::vector<ttstr> faces;
			if (TVPEnumFontsProcCollect(ttstr(path), faces) > 0) {
				for (const ttstr &f : faces) names.push_back(f.AsStdString());
				if (rememberFiles) TVPRememberUserFontFile(path);
				TVPAddLog(ttstr(TJS_W("Kirikinux2: font file added: ")) + ttstr(path));
			}
		} catch (const eTJS &e) {
			error = ttstr(e.GetMessage()).AsStdString();
		} catch (...) {
			error = path;
		}
		return names;
	};
	KR2FontPickResult res;
	const int r = KR2LinuxPickFont(req, res);
	if (r == -2) return -1;
	if (r != 1) return 0;
	face = ttstr(res.face);
	if (file) *file = ttstr(res.file);
	return 1;
}

#else

int TVPShowFontPicker(const ttstr &, const ttstr &, const ttstr &, const ttstr &, bool, ttstr &, ttstr *) {
	return -1;
}

#endif
