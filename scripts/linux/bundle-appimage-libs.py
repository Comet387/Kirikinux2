#!/usr/bin/env python3
"""Copy the installed original engine's shared-library closure into its AppDir."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

# Use the host's glibc and graphics driver stack. Shipping a particular Mesa or
# GLVND build would prevent the same AppImage from using the user's GPU driver.
HOST_LIBS = re.compile(
    r"^(?:ld-linux.*|lib(?:c|m|pthread|dl|rt|resolv|util|anl|nss_.*)\.so.*|"
    r"lib(?:GL|GLX|GLX_.*|GLdispatch|EGL|GLESv[12]|gbm|drm|drm_.*)\.so.*)$"
)


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("runtime", type=Path)
    ap.add_argument("--patchelf", default="patchelf")
    args = ap.parse_args()
    runtime = args.runtime.resolve()
    engine = runtime / "kirikinux"
    libdir = runtime / "lib"
    libdir.mkdir(exist_ok=True)
    if not engine.is_file():
        raise SystemExit("original engine is missing")
    queued = [engine] + list(libdir.glob("*.so*"))
    seen = set()
    manifest = {}
    host = set()
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(libdir) + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    while queued:
        elf = queued.pop()
        if elf.resolve() in seen:
            continue
        seen.add(elf.resolve())
        needed = run(args.patchelf, "--print-needed", str(elf)).splitlines()
        listing = subprocess.check_output(["ldd", str(elf)], env=env, text=True)
        resolved = {}
        for line in listing.splitlines():
            match = re.match(r"\s*(\S+)\s+=>\s+(\S+)", line)
            if match and match[2] != "not":
                resolved[match[1]] = Path(match[2])
        for name in needed:
            if HOST_LIBS.fullmatch(name):
                host.add(name)
                continue
            source = resolved.get(name)
            if source is None or not source.is_file():
                raise RuntimeError(f"unresolved dependency of {elf}: {name}\n{listing}")
            target = libdir / name
            if not target.exists():
                shutil.copy2(source.resolve(), target)
            manifest[name] = {
                "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                "source": str(source.resolve()),
            }
            queued.append(target)
    run(args.patchelf, "--set-rpath", "$ORIGIN/lib", str(engine))
    for elf in libdir.glob("*.so*"):
        if not elf.is_symlink():
            run(args.patchelf, "--set-rpath", "$ORIGIN", str(elf))
    for name, record in manifest.items():
        record["sha256"] = hashlib.sha256((libdir / name).read_bytes()).hexdigest()
    (runtime / "bundled-libraries.json").write_text(
        json.dumps({"bundled": manifest, "host_libraries": sorted(host)}, indent=2) + "\n"
    )
    print(f"Bundled {len(manifest)} shared libraries; host libraries: {', '.join(sorted(host))}")


if __name__ == "__main__":
    main()
