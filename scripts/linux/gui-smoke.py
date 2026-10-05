#!/usr/bin/env python3
"""Launch the original engine on Xvfb, capture real X11 frames and inject input.

Requires Xvfb, libX11 and libXtst. No game resources are created or substituted.
Example: gui-smoke.py --engine build-linux/bin/kirikinux2 --game /path/data.xp3
Actions: --action click:5:640:360 --action key:10:space
Use --action focus:9 before keyboard input when Xvfb has no window manager.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import socket
import shutil
import struct
import subprocess
import time
import zlib


class XImage(C.Structure):
    _fields_ = [("width", C.c_int), ("height", C.c_int), ("xoffset", C.c_int),
                ("format", C.c_int), ("data", C.c_void_p),
                ("byte_order", C.c_int), ("bitmap_unit", C.c_int),
                ("bitmap_bit_order", C.c_int), ("bitmap_pad", C.c_int),
                ("depth", C.c_int), ("bytes_per_line", C.c_int),
                ("bits_per_pixel", C.c_int), ("red_mask", C.c_ulong),
                ("green_mask", C.c_ulong), ("blue_mask", C.c_ulong)]


class Display:
    def __init__(self, name):
        self.x = C.CDLL("libX11.so.6")
        self.t = C.CDLL("libXtst.so.6")
        self.x.XOpenDisplay.argtypes = [C.c_char_p]
        self.x.XOpenDisplay.restype = C.c_void_p
        self.d = self.x.XOpenDisplay(name.encode())
        if not self.d:
            raise RuntimeError("XOpenDisplay failed")
        self.x.XDefaultRootWindow.argtypes = [C.c_void_p]
        self.x.XDefaultRootWindow.restype = C.c_ulong
        self.root = self.x.XDefaultRootWindow(self.d)
        self.x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                                    C.c_uint, C.c_uint, C.c_ulong, C.c_int]
        self.x.XGetImage.restype = C.POINTER(XImage)
        self.x.XDestroyImage.argtypes = [C.POINTER(XImage)]
        self.x.XFlush.argtypes = [C.c_void_p]
        self.x.XCloseDisplay.argtypes = [C.c_void_p]
        self.x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong),
                                     C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)),
                                     C.POINTER(C.c_uint)]
        self.x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
        self.x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        self.x.XInternAtom.restype = C.c_ulong
        self.x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong, C.c_long, C.c_long,
            C.c_int, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_int),
            C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.c_void_p)]
        self.x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
        self.x.XRaiseWindow.argtypes = [C.c_void_p, C.c_ulong]
        self.x.XResizeWindow.argtypes = [C.c_void_p, C.c_ulong, C.c_uint, C.c_uint]
        self.x.XFree.argtypes = [C.c_void_p]
        self.x.XStringToKeysym.argtypes = [C.c_char_p]
        self.x.XStringToKeysym.restype = C.c_ulong
        self.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
        self.x.XKeysymToKeycode.restype = C.c_uint
        self.x.XGetKeyboardMapping.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.POINTER(C.c_int)]
        self.x.XGetKeyboardMapping.restype = C.POINTER(C.c_ulong)
        self.t.XTestFakeMotionEvent.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_ulong]
        self.t.XTestFakeButtonEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
        self.t.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]

    def capture(self, path, width, height):
        p = self.x.XGetImage(self.d, self.root, 0, 0, width, height, C.c_ulong(-1).value, 2)
        if not p:
            raise RuntimeError("XGetImage failed")
        try:
            im = p.contents
            if im.bits_per_pixel != 32 or im.byte_order != 0 or im.red_mask != 0xff0000:
                raise RuntimeError("Unsupported X11 framebuffer layout")
            raw = C.string_at(im.data, im.bytes_per_line * height)
            rows = []
            nonblack = 0
            for y in range(height):
                row = raw[y * im.bytes_per_line:y * im.bytes_per_line + width * 4]
                rgb = bytearray(width * 3)
                rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
                nonblack += sum(bool(rgb[x] or rgb[x + 1] or rgb[x + 2]) for x in range(0, len(rgb), 3))
                rows.append(b"\0" + rgb)
            def chunk(kind, data):
                return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
            png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            png += chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b"")
            path.write_bytes(png)
            return {"file": path.name, "nonblack_pixels": nonblack}
        finally:
            self.x.XDestroyImage(p)

    def action(self, parts):
        result = None
        if parts[0] in ("focus", "resize") and len(parts) in (2, 3, 4):
            target_title = parts[2] if parts[0] == "focus" and len(parts) == 3 else "Kirikinux2"
            root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
            children = C.POINTER(C.c_ulong)()
            if not self.x.XQueryTree(self.d, self.root, C.byref(root), C.byref(parent),
                                    C.byref(children), C.byref(count)):
                raise RuntimeError("XQueryTree failed")
            try:
                for i in range(count.value):
                    name = C.c_void_p()
                    atom = self.x.XInternAtom(self.d, b"_NET_WM_NAME", 0)
                    kind, size, length, remaining = C.c_ulong(), C.c_int(), C.c_ulong(), C.c_ulong()
                    self.x.XGetWindowProperty(self.d, children[i], atom, 0, 1024, 0, 0,
                        C.byref(kind), C.byref(size), C.byref(length), C.byref(remaining), C.byref(name))
                    if not length.value:
                        if name:
                            self.x.XFree(name)
                        name = C.c_void_p()
                        self.x.XFetchName(self.d, children[i], C.byref(name))
                    if name.value:
                        try:
                            title = C.string_at(name).decode(errors="replace")
                        finally:
                            self.x.XFree(name)
                        if title == target_title:
                            self.x.XRaiseWindow(self.d, children[i])
                            self.x.XSetInputFocus(self.d, children[i], 2, 0)
                            if parts[0] == "resize":
                                self.x.XResizeWindow(self.d, children[i], int(parts[2]), int(parts[3]))
                            result = {"focused_window": int(children[i]), "title": title}
                            break
                if result is None:
                    raise RuntimeError("X11 window not found: " + target_title)
            finally:
                if children:
                    self.x.XFree(children)
        elif parts[0] == "drag" and len(parts) == 6:
            x0,y0,x1,y1=map(int,parts[2:])
            self.t.XTestFakeMotionEvent(self.d,-1,x0,y0,0)
            self.t.XTestFakeButtonEvent(self.d,1,1,0);self.x.XFlush(self.d);time.sleep(.1)
            for i in range(1,11):
                self.t.XTestFakeMotionEvent(self.d,-1,x0+(x1-x0)*i//10,y0+(y1-y0)*i//10,0)
                self.x.XFlush(self.d);time.sleep(.03)
            self.t.XTestFakeButtonEvent(self.d,1,0,0)
        elif parts[0] == "wheel" and len(parts) == 5:
            self.t.XTestFakeMotionEvent(self.d, -1, int(parts[2]), int(parts[3]), 0)
            for _ in range(abs(int(parts[4]))):
                button = 5 if int(parts[4]) > 0 else 4
                self.t.XTestFakeButtonEvent(self.d, button, 1, 0)
                self.t.XTestFakeButtonEvent(self.d, button, 0, 0)
        elif parts[0] in ("click", "rightclick") and len(parts) == 4:
            button = 1 if parts[0] == "click" else 3
            self.t.XTestFakeMotionEvent(self.d, -1, int(parts[2]), int(parts[3]), 0)
            self.t.XTestFakeButtonEvent(self.d, button, 1, 0)
            self.t.XTestFakeButtonEvent(self.d, button, 0, 0)
        elif parts[0] in ("keydown", "keyup") and len(parts) == 3:
            key = self.x.XKeysymToKeycode(self.d, self.x.XStringToKeysym(parts[2].encode()))
            if not key:
                raise ValueError("Unknown key: " + parts[2])
            self.t.XTestFakeKeyEvent(self.d, key, parts[0] == "keydown", 0)
        elif parts[0] == "type" and len(parts) == 3:
            # Real keyboard input to the native file chooser, using ASCII paths.
            # Unicode filename handling is covered by desktop_dialogs.cpp.
            shift = self.x.XKeysymToKeycode(self.d, self.x.XStringToKeysym(b"Shift_L"))
            for char in parts[2]:
                sym = ord(char)
                if sym > 127:
                    raise ValueError("type action requires ASCII; use native chooser Unicode test")
                key = self.x.XKeysymToKeycode(self.d, sym)
                if not key:
                    raise ValueError("Unmapped character: " + char)
                count = C.c_int()
                mapping = self.x.XGetKeyboardMapping(self.d, key, 1, C.byref(count))
                try:
                    shifted = count.value > 1 and mapping[1] == sym and mapping[0] != sym
                finally:
                    self.x.XFree(mapping)
                if shifted:
                    self.t.XTestFakeKeyEvent(self.d, shift, 1, 0)
                self.t.XTestFakeKeyEvent(self.d, key, 1, 0)
                self.t.XTestFakeKeyEvent(self.d, key, 0, 0)
                if shifted:
                    self.t.XTestFakeKeyEvent(self.d, shift, 0, 0)
                self.x.XFlush(self.d)
                time.sleep(0.015)
        elif parts[0] == "key" and len(parts) == 3:
            key = self.x.XKeysymToKeycode(self.d, self.x.XStringToKeysym(parts[2].encode()))
            if not key:
                raise ValueError("Unknown key: " + parts[2])
            self.t.XTestFakeKeyEvent(self.d, key, 1, 0)
            self.x.XFlush(self.d)
            # KAG checks getKeyState while handling its queued key-down event.
            # A press and release in one X11 batch can both be consumed before
            # that handler runs. Hold the physical key across engine frames.
            time.sleep(0.12)
            self.t.XTestFakeKeyEvent(self.d, key, 0, 0)
            result = {"held_ms": 120}
        else:
            raise ValueError("Invalid action: " + ":".join(parts))
        self.x.XFlush(self.d)
        return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--engine", required=True, type=Path)
    ap.add_argument("--engine-arg", action="append", default=[])
    ap.add_argument("--game", type=Path)
    ap.add_argument("--output", type=Path, default=Path("gui-smoke"))
    ap.add_argument("--xvfb", default="Xvfb")
    ap.add_argument("--xvfb-cwd", type=Path)
    ap.add_argument("--xvfb-preload")
    ap.add_argument("--display", type=int, default=100)
    ap.add_argument("--seconds", type=float, default=12)
    ap.add_argument("--action", action="append", default=[])
    ap.add_argument("--capture-at", action="append", type=float, default=[],
                    help="Capture a real frame at this time, in seconds (repeatable)")
    ap.add_argument("--require-log", action="append", default=[],
                    help="Require this literal text in engine.log (repeatable)")
    ap.add_argument("--forbid-log", action="append", default=[],
                    help="Fail if this literal text appears in engine.log (repeatable)")
    args = ap.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["DISPLAY"] = f"127.0.0.1:{args.display}"
    for key, sub in [("XDG_CONFIG_HOME", "config"), ("XDG_DATA_HOME", "data"),
                     ("XDG_CACHE_HOME", "cache"), ("XDG_RUNTIME_DIR", "runtime")]:
        p = out / sub
        p.mkdir(exist_ok=True, mode=0o700)
        env[key] = str(p)
    xenv = env.copy()
    if args.xvfb_preload:
        xenv["LD_PRELOAD"] = args.xvfb_preload
    server = engine = display = None
    report = {"engine": str(args.engine.resolve()), "game": str(args.game.resolve()) if args.game else None,
              "captures": [], "actions": []}
    if args.game:
        if args.game.is_file():
            report["game_sha256"] = hashlib.sha256(args.game.read_bytes()).hexdigest()
        else:
            report["game_files_sha256"] = {
                str(p.relative_to(args.game)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(args.game.rglob("*")) if p.is_file()
            }
    try:
        with (out / "xvfb.log").open("w") as xlog, (out / "engine.log").open("w") as elog:
            server = subprocess.Popen([args.xvfb, f":{args.display}", "-screen", "0", "1280x720x24",
                                       "-nolock", "-nolisten", "unix", "-nolisten", "local", "-listen", "tcp",
                                       "-ac", "-noreset", "-fp", "built-ins"],
                                      cwd=args.xvfb_cwd, env=xenv, stdout=xlog, stderr=subprocess.STDOUT)
            for _ in range(100):
                if server.poll() is not None:
                    raise RuntimeError("Xvfb exited; see xvfb.log")
                try:
                    with socket.create_connection(("127.0.0.1", 6000 + args.display), timeout=0.2):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                raise RuntimeError("Xvfb did not become ready")
            display = Display(env["DISPLAY"])
            # glxinfo is useful diagnostics, but it is not required to launch
            # the engine.  Minimal CI/Xvfb images often omit mesa-utils even
            # though the X server and GL loader are available.  Continue in
            # that case and leave an explicit record in the report.
            glxinfo = os.environ.get("KR2_GLXINFO", "glxinfo")
            if shutil.which(glxinfo):
                glx = subprocess.run([glxinfo, "-B"], env=env,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, timeout=15)
                (out / "glxinfo.log").write_text(glx.stdout)
                report["glxinfo"] = {"command": glxinfo, "returncode": glx.returncode}
                if glx.returncode:
                    raise RuntimeError("OpenGL probe failed; see glxinfo.log")
            else:
                report["glxinfo"] = {"command": glxinfo, "skipped": "not installed"}
            cmd = [str(args.engine.resolve())] + args.engine_arg
            if args.game:
                cmd.append(str(args.game.resolve()))
            engine = subprocess.Popen(cmd, env=env, stdout=elog, stderr=subprocess.STDOUT)
            start = time.monotonic()
            actions = sorted([a.split(":") for a in args.action], key=lambda a: float(a[1]))
            frames = sorted(set([2.0, 5.0, args.seconds - 0.2] + args.capture_at
                                + [float(a[1]) + 1 for a in actions]))
            while time.monotonic() - start < args.seconds:
                elapsed = time.monotonic() - start
                if engine.poll() is not None:
                    report["early_exit"] = engine.returncode
                    break
                while actions and float(actions[0][1]) <= elapsed:
                    a = actions.pop(0)
                    result = display.action(a)
                    entry = {"time": elapsed, "action": a}
                    if result:
                        entry["result"] = result
                    report["actions"].append(entry)
                while frames and frames[0] <= elapsed:
                    t = frames.pop(0)
                    frame = display.capture(out / f"frame-{t:05.1f}.png", 1280, 720)
                    frame["time"] = elapsed
                    report["captures"].append(frame)
                    print(json.dumps(frame), flush=True)
                time.sleep(0.05)
            report["alive_at_end"] = engine.poll() is None
    except Exception as exc:
        report["error"] = str(exc)
    finally:
        if display:
            display.x.XCloseDisplay(display.d)
        for p in [engine, server]:
            if p and p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
        log = (out / "engine.log").read_text(errors="replace") if (out / "engine.log").exists() else ""
        report["missing_log_markers"] = [s for s in args.require_log if s not in log]
        report["unexpected_log_markers"] = [s for s in args.forbid_log if s in log]
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)
    return 0 if (report.get("alive_at_end") and report.get("captures")
                 and not report.get("error") and not report["missing_log_markers"]
                 and not report["unexpected_log_markers"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
