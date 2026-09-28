#!/usr/bin/env python3
"""Validate and run the external fork3 XP3 regression fixture."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import subprocess
import sys


FORK3_SHA256 = "15f008b19fbd10d08002c8419670c0d7c98d03958047f2ded9dc3f1e8c58b02b"
FORK3_SIZE = 14_396_068
FORK3_ENTRIES = 181


def fail(message: str, output: str = "") -> int:
    print(f"FAIL: {message}", file=sys.stderr)
    if output:
        lines = output.splitlines()
        print("--- game output (last 80 lines) ---", file=sys.stderr)
        print("\n".join(lines[-80:]), file=sys.stderr)
    return 1


def run(command: list[str], timeout: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
        check=False,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=pathlib.Path)
    parser.add_argument("--xp3", required=True, type=pathlib.Path)
    parser.add_argument("--minimum", choices=("regexp", "layer", "success"), default="layer")
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()

    binary = args.binary.resolve()
    xp3 = args.xp3.resolve()
    if not binary.is_file():
        return fail(f"binary does not exist: {binary}")
    if not xp3.is_file():
        return fail(f"XP3 fixture does not exist: {xp3}")

    size = xp3.stat().st_size
    if size != FORK3_SIZE:
        return fail(f"XP3 size mismatch: expected {FORK3_SIZE}, got {size}")
    digest = hashlib.sha256(xp3.read_bytes()).hexdigest()
    if digest != FORK3_SHA256:
        return fail(f"XP3 SHA-256 mismatch: expected {FORK3_SHA256}, got {digest}")

    try:
        listed = run([str(binary), "--list", str(xp3)], args.timeout)
    except subprocess.TimeoutExpired:
        return fail("archive listing timed out")
    if listed.returncode != 0:
        return fail(f"archive listing exited with {listed.returncode}", listed.stdout)
    entries = [line for line in listed.stdout.splitlines() if line.strip()]
    if len(entries) != FORK3_ENTRIES:
        return fail(f"expected {FORK3_ENTRIES} archive entries, got {len(entries)}", listed.stdout)

    try:
        game = run([str(binary), "--run", str(xp3)], args.timeout)
    except subprocess.TimeoutExpired:
        return fail("game startup timed out")
    output = game.stdout

    if game.returncode == 0:
        print(f"PASS: fixture valid ({len(entries)} entries); game startup completed")
        return 0

    reached_kag = "KAG System" in output
    regexp_blocker = 'Member "RegExp" does not exist' in output
    layer_blocker = (
        'Member "Layer" does not exist' in output
        and "system/LayerEx.tjs:39" in output
        and reached_kag
    )

    if args.minimum == "regexp" and (regexp_blocker or layer_blocker):
        stage = "Layer" if layer_blocker else "RegExp"
        print(f"PASS: fixture valid ({len(entries)} entries); reached {stage} compatibility boundary")
        return 0
    if args.minimum == "layer" and layer_blocker:
        print(f"PASS: fixture valid ({len(entries)} entries); reached LayerEx.tjs:39")
        return 0

    expected = {
        "regexp": "the RegExp boundary or later",
        "layer": "the Layer boundary or a successful startup",
        "success": "a successful startup",
    }[args.minimum]
    return fail(
        f"game exited with {game.returncode} before reaching {expected}",
        output,
    )


if __name__ == "__main__":
    raise SystemExit(main())
