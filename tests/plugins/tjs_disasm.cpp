// SPDX-License-Identifier: AGPL-3.0-only
// Dumps compiled TJS2 bytecode (TJS2100) with the engine's own disassembler.
//   tjs_disasm FILE.tjs > out.txt
#include <cstdio>
#include <iostream>
#include <vector>
#include "tjs.h"
#include "tjsScriptBlock.h"
#include "tjsByteCodeLoader.h"
using namespace TJS;
namespace TJS { extern void TVPConsoleLog(const tjs_char *line); }
struct StdoutConsole : public iTJSConsoleOutput {
    void ExceptionPrint(const tjs_char *msg) override { std::cout << ttstr(msg).AsStdString() << "\n"; }
    void Print(const tjs_char *msg) override { std::cout << ttstr(msg).AsStdString() << "\n"; }
};
int main(int argc, char **argv) {
    if(argc < 2) return 2;
    FILE *fp = std::fopen(argv[1], "rb");
    if(!fp) return 1;
    std::vector<tjs_uint8> buf;
    tjs_uint8 tmp[65536]; size_t n;
    while((n = std::fread(tmp, 1, sizeof tmp, fp)) > 0) buf.insert(buf.end(), tmp, tmp + n);
    std::fclose(fp);
    tTJS *engine = new tTJS();
    static StdoutConsole console;
    engine->SetConsoleOutput(&console);
    try {
        tTJSByteCodeLoader loader;
        tTJSScriptBlock *b = loader.ReadByteCode(engine, ttstr(argv[1]).c_str(), buf.data(), buf.size());
        if(!b) { std::cerr << "not bytecode\n"; return 1; }
        b->Dump();
        b->Release();
    } catch(const eTJS &e) {
        std::cerr << "error: " << e.GetMessage().AsStdString() << "\n";
        return 1;
    }
    return 0;
}
