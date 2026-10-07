// SPDX-License-Identifier: AGPL-3.0-only
// Native font picker (Linux: GTK, see platform/linux/LinuxFontPicker.*).
#pragma once
#include "tjs.h"

// Shows the picker.  Returns 1 when a face was chosen (stored in `face`),
// 0 when cancelled, -1 when no picker is available on this platform/display.
// rememberFiles: font files added from the picker are registered on every
// later start too (needed when a game stores the chosen face name).
// file (optional): the font file the chosen face came from, if any.
int TVPShowFontPicker(const ttstr &caption, const ttstr &prompt, const ttstr &sample,
	const ttstr &initial, bool rememberFiles, ttstr &face, ttstr *file = nullptr);
