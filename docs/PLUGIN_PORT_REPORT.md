# Kirikinux2 plugin port report (handoff)

Date: 2026-10-07. Test game: Senren\*Banka (Yuzusoft, Steam build, KAGEX/M2).

## 1. Conclusion: "D3D is broken" was the wrong diagnosis

- Direct3D is not needed. The game has a full non-D3D path:
  `Config.tjs` sets `DefaultD3DMode = false`; `motion.tjs` links
  `motionplayer_nod3d.dll` when `-nod3dm` is set; the standard Kirikiroid2
  patch `startup.tjs` just forces `Motion.Player.useD3D = 0`.
  Kirikiroid2 never implemented D3D either.
- The real blocker: Kirikiroid2's public source (zeas2/Kirikiroid2) never
  contained the plugins its APK has built in (psbfile, motionplayer,
  layerExDraw, KAGParserEx, fstat, windowEx, TextRender, ...).
  Kirikinux2 inherited that gap. `TVPLoadPlugin` silently ignores unknown
  DLL names, so failures appear later as "member not found" errors.
- `StartupCompatibility.h` replaced Motion with no-op stubs. That hid the
  missing plugin instead of fixing it.

## 2. What this game actually needs (from its scripts)

- **psbfile.dll** — critical. `system/StorageData.tjs` loads every scenario
  (`*.scn`, a PSB file) with `new PSBFile(storage).root`. Without it the
  story cannot run. Also used by psdlayer.tjs and diffimage2.tjs.
- **motionplayer.dll** (M2 Motion, `.mtn`) — logo intro (`yuzulogo.mtn`,
  `m2logo.mtn`), animated title (`title_bg.mtn`), all SD chibi scenes
  (`SD001.mtn`...). Standing sprites are ordinary images.
- fstat.dll (save/load, file times), scriptsEx.dll (Utils.tjs),
  TextRender.dll, windowEx.dll, KAGParserEx.dll (scnchart),
  layerExDraw.dll (AffineSource/scnchart_ui), psd.dll, csvParser (exists).
- Windows/Steam-only, can be stubs: krkrsteam, SteamDrawDevice, win32ole,
  shellexecute, process, bishamon, drawdeviceD3D, flashplayer, gamepad.

Full list of DLL names referenced by the scripts: see section 6.

## 3. Source of the implementations

The KrKr2 emulator (https://github.com/2468785842/krkr2, revision in
`src/plugins/ext/README.md`) is an open rebuild of Kirikiroid2 that already
implements most of these, including a ~27k-line Motion player
reconstructed from Kirikiroid2's `libkrkr2.so`. Licence: KiriKiri
BSD-style (permissive; compatible with AGPL; notice kept in
`src/plugins/ext/LICENSE.krkr2`). Its core is the same Kirikiroid2 core,
so porting is mostly mechanical.

## 4. Done in this checkpoint

| Item | Status |
|---|---|
| psbfile.dll ported (`src/plugins/ext/psbfile`) | compiles with -Wall; **parser verified on real files** |
| motionplayer.dll + emoteplayer.dll alias | compiles to objects; **not run** |
| motionplayer_nod3d.dll alias (new) | compiles |
| fstat.dll, scriptsEx.dll, TextRender.dll, windowEx.dll | compile; not run |
| Core: `tTJSBinaryStream::ReadI8LE` | added |
| Core: Layer `BezierPatchCopy/MeshCopy/OperateBezierPatch/OperateMesh`, `ConstructResolvedTreeOwnerLike_0x800438` | ported from KrKr2 core |
| spdlog/fmt/boost removed from ported code | `src/plugins/ext/kr2_plugin_log.h` shim |
| CMake target `krkr2plugin_ext` (C++17, --whole-archive) | added, **not built** |
| Headless test `tests/plugins` (psb_harness) | builds and passes |

psb_harness results on the uploaded files:

```
yuzulogo.mtn  version=3 type=Motion resources=14  root keys: source,object,spec(krkr),version,id,label,metadata,...
sd001.mtn     version=3 type=Motion resources=34
001_アーサー王ver1_07_ks.scn  version=3 type=Scn
   root.name   => 001・アーサー王ver1.07.ks   (UTF-8 → TJS correct)
   root.scenes.count => 12, scenes[0].label => *com_part_1
   scenes[2].texts.count => 107
```

Run it yourself:
```sh
cmake -S tests/plugins -B build-plugin-tests -G Ninja && cmake --build build-plugin-tests
./build-plugin-tests/psb_harness path/to/file.scn 'root.scenes.count'
```

Link check (symbol level): all external symbols needed by the new
objects were checked against Kirikinux2 sources; the only gaps found were
the Layer mesh functions (now ported) and fmt (now shimmed).
This is a static check, not a real link.

## 5. Not verified / next steps (in order)

1. **Full engine build** (`scripts/linux/build.sh`). Not completed in the
   sandbox (1 CPU; FFmpeg 4 + Cocos build too slow). Expect a few small
   compile/link fixes in `krkr2plugin_ext`. Note: the SourceForge p7zip
   URL in `fetch-thirdparty.sh` was failing; a mirror with the same
   SHA-256 (`master.dl.sourceforge.net`) worked.
2. Run the game; set `KR2_PLUGIN_LOG=debug`. First milestone: title screen
   via scenario (psbfile). Second: logo `.mtn` animation (motionplayer).
3. `StartupCompatibility.h` still installs the no-op `Motion` dictionary
   before scripts run. The real plugin replaces global `Motion` on
   `Plugins.link("motionplayer.dll")` (ncbind PropSet). If a game
   inspects `Motion` without linking, it still sees the stub. Consider
   removing the stub once motionplayer is confirmed working.
4. Port `layerex_draw` (KrKr2 uses blend2d on non-Windows: new dependency)
   and `psdfile` (needs boost spirit/iostreams).
5. KAGParserEx: KrKr2 claims it is "built into core", but neither core
   implements `multiLineTagEnabled`/`paramMacros`. Port the real plugin
   (krkrz repo, `KAGParserEx`) into `src/core/utils/KAGParser.cpp` or as
   a plugin.
6. Stubs for Windows/Steam-only DLLs listed above.
7. Remove debug-only paths from motionplayer (`MotionTraceWeb.cpp`) if
   not needed.

## 6. DLL names referenced by Senren\*Banka scripts

addfont, alphamovie, bishamon, bishamonlayer, clipboardex, csvparser,
drawdeviced3d(z), emoteplayer, extnagano, extrans, fftgraph, flashplayer,
fstat, gamepad, getlangname, getsample, gfxeffect, k2compat, kagparserex,
krmovie, kztouch, layerexbtoa, layerexdraw, layereximage, layerexraster,
layerexsave, layerextexture(2), menu, messenger, motionplayer(_nod3d),
opus, packinone, panic, process, proxyfs, psbfile, psd, savestruct,
scriptsex, shellexecute, shrinkcopy, squirrel, textrender, wfbasiceffect,
wftypicaldsp, win32dialog, win32ole, windowex, wuopus, wuvorbis.
Many are loaded only after `CanLoadPlugin` checks; which ones are hard
requirements must be confirmed by running the game.

## 7. Materials

- APK of Kirikiroid2 1.3.9: never reached the sandbox (upload failed 3x).
  Not needed: KrKr2's motionplayer is already derived from it.
- Game scripts: `data-scripts.zip` (bytecode TJS2, UTF-16 text).
- Real PSB samples: `001_アーサー王ver1_07_ks.scn`, `sd001.mtn`,
  `yuzulogo.mtn` (copyrighted game data — not included in the source zip).
