#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Build only the original TJS VM and run the production startup regression."""
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
compiler = os.environ.get('CXX', 'c++')
sources = sorted((root / 'src/core/tjs2').glob('*.cpp'))
sources = [p for p in sources if p.name != 'tjsRegExp.cpp']
sources += [root / 'linux/tjs_platform.cpp', root / 'tests/linux/startup_context.cpp']
flags = ['-std=c++17', '-O0', '-w', '-pthread', '-D__STDC_CONSTANT_MACROS',
         '-DUSE_UNICODE_FSTRING', '-DTJS_NO_REGEXP=1', '-DTJS_TEXT_OUT_CRLF=1']
for folder in ['src/core/tjs2', 'src/core/utils', 'src/core/environ', 'src/core/base']:
    flags += ['-I', str(root / folder)]
if __name__ == '__main__':
    temp = root / 'build-startup-check'
    temp.mkdir(exist_ok=True)
    os.environ['TMPDIR'] = str(temp)
    objects = [Path(temp) / f'{i}.o' for i in range(len(sources))]
    def compile_one(pair):
        src, obj = pair
        header = root / 'src/core/base/StartupCompatibility.h'
        newest = max(src.stat().st_mtime, header.stat().st_mtime if src.name == 'startup_context.cpp' else 0)
        if obj.exists() and obj.stat().st_mtime >= newest:
            return
        subprocess.run([compiler, *flags, '-c', str(src), '-o', str(obj)], check=True)
    compile_one((sources[-1], objects[-1]))
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(compile_one, zip(sources, objects)))
    program = Path(temp) / 'startup-context'
    subprocess.run([compiler, '-pthread', *map(str, objects), '-o', str(program)], check=True)
    subprocess.run([str(program)], check=True)
