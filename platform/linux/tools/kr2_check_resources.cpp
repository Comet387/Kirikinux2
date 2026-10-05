// SPDX-License-Identifier: AGPL-3.0-only
// Validate using the same FreeType archive Cocos uses, before publishing UI.
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: kr2_check_resources <font> <cursor>\n"); return 2; }
    FT_Library library;
    FT_Face face;
    if (FT_Init_FreeType(&library)) return 1;
    if (FT_New_Face(library, argv[1], 0, &face)) {
        std::fprintf(stderr, "Kirikinux2: UI font cannot be opened by bundled FreeType: %s\n", argv[1]);
        FT_Done_FreeType(library); return 1;
    }
    bool valid = FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0;
    const char32_t sample[] = U"Aa012中文设备信息全局设置游戏目录复制遊戲開啟フォルダー";
    for (const char32_t *ch = sample; *ch; ++ch) {
        if (!FT_Get_Char_Index(face, *ch)) {
            std::fprintf(stderr, "Kirikinux2: UI font lacks U+%04X\n", unsigned(*ch));
            valid = false;
        }
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    std::ifstream cursor(argv[2], std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(cursor)), {});
    if (bytes.size() < 62 || bytes[0] || bytes[1] || bytes[2] != 2 || bytes[3] || !bytes[4]) {
        std::fprintf(stderr, "Kirikinux2: missing or invalid default.cur: %s\n", argv[2]);
        valid = false;
    }
    if (valid) std::puts("Kirikinux2: packaged UI font and cursor verified");
    return valid ? 0 : 1;
}
