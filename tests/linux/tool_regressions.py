#!/usr/bin/env python3
"""Resource-tool failure policy (serializer double), and patch rejection checks.
The serializer double verifies exit codes, not Cocos CSB serialization itself.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('patch_cocos', ROOT / 'scripts/linux/patch-cocos2dx.py')
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)
assert patch.file_utils_linux('class Unrelated {};') is None
assert patch.image('class Unrelated {};') is None
assert patch.scroll_view('class Unrelated {};') is None

BUILD = ROOT / 'build-port-components'
BUILD.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=BUILD) as tmp:
    tmp = Path(tmp)
    (tmp / 'cocostudio').mkdir()
    (tmp / 'cocos2d.h').write_text('''#pragma once
#include <string>
namespace cocos2d { struct FileUtils {
 static FileUtils* getInstance() { static FileUtils f; return &f; }
 void addSearchPath(const std::string&) {}
}; }
''')
    (tmp / 'cocostudio/FlatBuffersSerialize.h').write_text('''#pragma once
#include <string>
#include <cstdio>
namespace cocostudio { struct FlatBuffersSerialize {
 static FlatBuffersSerialize* getInstance() { static FlatBuffersSerialize s; return &s; }
 std::string serializeFlatBuffersWithXMLFile(const std::string& in, const std::string& out) {
  if (in.find("bad.csd") != std::string::npos) return "injected serialization failure";
  if (in.find("silent.csd") != std::string::npos) return "";
  FILE* f = fopen(out.c_str(), "wb"); if (!f) return "write failed";
  fputs("test-double-output", f); fclose(f); return "";
 }
}; }
''')
    binary = tmp / 'converter'
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++14', '-I', str(tmp),
                    str(ROOT / 'platform/linux/tools/kr2_csd2csb.cpp'), '-o', str(binary)], check=True)
    for names, expected in [(['ok.csd'], 0), (['ok.csd', 'bad.csd'], 1), (['silent.csd'], 1), ([], 1)]:
        case = tmp / ('case-' + str(len(list(tmp.glob('case-*')))))
        case.mkdir()
        for name in names:
            (case / name).write_text('<test/>')
        result = subprocess.run([str(binary), str(case), str(case / 'output')], capture_output=True, text=True)
        assert result.returncode == expected, result.stdout + result.stderr
print('patch mismatch rejection and resource failure policy passed (serializer double)')
