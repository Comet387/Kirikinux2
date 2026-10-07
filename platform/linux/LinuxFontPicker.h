// SPDX-License-Identifier: AGPL-3.0-only
// Native (GTK) font picker used for in-game font selection
// (Font.doUserSelect, the game's own win32dialog font selectors) and for
// Preferences -> Default Font.  Three ways to choose a font:
//   1. pick an installed / registered face from a searchable list,
//   2. add a font file (.ttf/.otf/.ttc/.otc),
//   3. type a face name by hand.
// Standard types only, so the engine side needs no GTK headers.
#pragma once
#include <functional>
#include <string>
#include <vector>

struct KR2FontPickRequest {
	std::string title;
	std::string prompt;
	std::string sample;
	std::string initial;                 // preselected / prefilled face name
	std::vector<std::string> faces;      // faces offered in the list
	// Registers a font file; returns the faces it contains (empty on failure,
	// with an error message in `error`).
	std::function<std::vector<std::string>(const std::string &path, std::string &error)> addFile;
};

struct KR2FontPickResult {
	std::string face;   // chosen face name (may be typed by hand)
	std::string file;   // font file the face came from, if added from a file
};

// 1: OK, 0: cancelled, -2: no GUI available.
int KR2LinuxPickFont(const KR2FontPickRequest &req, KR2FontPickResult &out);
