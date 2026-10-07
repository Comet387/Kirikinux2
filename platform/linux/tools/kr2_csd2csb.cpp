// SPDX-License-Identifier: AGPL-3.0-only
//---------------------------------------------------------------------------
// kirikinux2 : publishes cocos/kr2/cocosstudio/**/*.csd to .csb with the
// cocos2d-x serializer Cocos Studio itself uses (FlatBuffersSerialize).  The
// public repository only has the Cocos Studio project, the APK has the
// published files (scripts/linux/import-apk-assets.sh copies those instead).
//
//   kr2_csd2csb <cocosstudio dir> <Resources/res dir>
//---------------------------------------------------------------------------
#include "cocos2d.h"
#include "cocostudio/FlatBuffersSerialize.h"

#include <cerrno>
#include <cstdio>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

static void collect(const std::string &root, const std::string &rel, std::vector<std::string> &out) {
	DIR *d = opendir((root + "/" + rel).c_str());
	if (!d) return;
	while (struct dirent *e = readdir(d)) {
		std::string name = e->d_name;
		if (name == "." || name == "..") continue;
		std::string r = rel.empty() ? name : rel + "/" + name;
		struct stat st;
		if (stat((root + "/" + r).c_str(), &st) != 0) continue;
		if (S_ISDIR(st.st_mode)) collect(root, r, out);
		else if (r.size() > 4 && r.compare(r.size() - 4, 4, ".csd") == 0) out.push_back(r);
	}
	closedir(d);
}

static bool mkdirs(const std::string &dir) {
	for (size_t p = 1; p <= dir.size(); ++p) {
		if (p == dir.size() || dir[p] == '/') {
			std::string sub = dir.substr(0, p);
			if (mkdir(sub.c_str(), 0755) != 0 && errno != EEXIST) return false;
		}
	}
	return true;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s <cocosstudio dir> <output res dir>\n", argv[0]);
		return 2;
	}
	std::string in = argv[1], out = argv[2];
	std::vector<std::string> files;
	collect(in, "", files);
	auto *fileutils = cocos2d::FileUtils::getInstance();
	if (!fileutils) {
		fprintf(stderr, "kr2_csd2csb: cannot initialize Cocos file paths\n");
		return 1;
	}
	fileutils->addSearchPath(in);
	int ok = 0, failed = 0;
	for (const std::string &rel : files) {
		std::string dst = out + "/" + rel.substr(0, rel.size() - 4) + ".csb";
		if (!mkdirs(dst.substr(0, dst.find_last_of('/')))) {
			++failed;
			fprintf(stderr, "kr2_csd2csb: cannot create output directory for %s\n", dst.c_str());
			continue;
		}
		std::string err = cocostudio::FlatBuffersSerialize::getInstance()->serializeFlatBuffersWithXMLFile(in + "/" + rel, dst);
		struct stat output;
		if (err.empty() && (stat(dst.c_str(), &output) != 0 || !S_ISREG(output.st_mode) || output.st_size == 0))
			err = "serializer produced no nonempty .csb file";
		if (err.empty()) {
			++ok;
		} else {
			++failed;
			fprintf(stderr, "kr2_csd2csb: %s: %s\n", rel.c_str(), err.c_str());
		}
	}
	printf("kr2_csd2csb: %d published, %d failed\n", ok, failed);
	return ok > 0 && failed == 0 ? 0 : 1;
}
