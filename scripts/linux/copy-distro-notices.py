#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Retain distribution copyright notices under Kirikinux2's own doc directory."""
import argparse
import os
from pathlib import Path
import shutil


def copy_notices(root, appdir):
    root = Path(root).resolve()
    destination = Path(appdir) / "usr/share/doc/kirikinux2/distro"
    for notice in (root / "usr/share/doc").glob("*/copyright"):
        source = notice
        # Absolute links in a private sysroot refer to that sysroot, even if
        # the same path also happens to exist on the packaging host.
        seen = set()
        while source.is_symlink():
            if source in seen:
                raise RuntimeError(f"circular copyright symlink: {notice}")
            seen.add(source)
            linked = source.readlink()
            source = root / str(linked).lstrip("/") if linked.is_absolute() else source.parent / linked
            source = Path(os.path.normpath(source))
        if not source.is_file():
            continue
        target = destination / notice.parent.name / "copyright"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("appdir", type=Path)
    args = parser.parse_args()
    copy_notices(args.root, args.appdir)


if __name__ == "__main__":
    main()
