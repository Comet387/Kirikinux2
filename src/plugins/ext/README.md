# Ported plugins (src/plugins/ext)

Source: KrKr2 emulator, https://github.com/2468785842/krkr2
Revision: dca7264572a63e753c1b07ca7053513d1751ce70 (2026-06-11)
Licence of the imported code: KiriKiri BSD-style licence, see LICENSE.krkr2
(compatible with this project's AGPL-3.0; keep the notice when redistributing).

| Module name (Plugins.link) | Directory / file           | State |
|----------------------------|----------------------------|-------|
| psbfile.dll                | psbfile/                   | compiles; parser verified on real .scn/.mtn (tests/plugins) |
| motionplayer.dll           | motionplayer/              | compiles (-fsyntax-only and -c); not run |
| emoteplayer.dll            | EmotePlayer.cpp            | alias that loads motionplayer.dll |
| motionplayer_nod3d.dll     | MotionPlayerNoD3D.cpp      | Kirikinux2 alias that loads motionplayer.dll |
| fstat.dll                  | fstat/main.cpp             | compiles; adapted to Kirikinux2 tTVP_stat/TVP_utime |
| scriptsEx.dll              | scriptsEx.cpp              | compiles |
| TextRender.dll             | TextRender.cpp             | compiles |
| windowEx.dll               | windowEx.cpp (+win32_dt.h) | compiles |
| KAGParserEx.dll            | kagparserex/               | original plugin (krkrz/krkr2 svn) wrapped in a namespace; compiles |
| layerExDraw.dll (GdiPlus)  | layerExDraw/               | KrKr2 blend2d backend; compiles; needs third_party/blend2d |
| getLangName.dll            | compat/getLangName.cpp     | new; tested (tests/plugins compat_harness) |
| layerExSave.dll            | compat/layerExSave.cpp     | new; forwards to Layer.saveLayerImage; tested |
| PackinOne.dll              | compat/PackinOne.cpp       | new; loads saveStruct/varfile/fstat/scriptsEx/csvParser/dirlist/layerExSave |
| win32ole.dll               | compat/win32ole.cpp        | new; class exists, constructor throws (no COM on Linux); tested |
| krkrsteam.dll              | steam/krkrsteam.cpp        | KrKr2 stub; Kirikinux2: locale-based getLanguage, cloud disabled |

Local changes versus upstream (search for "Kirikinux2"):
- spdlog/fmt/boost replaced by kr2_plugin_log.h (no new runtime libraries).
- psbfile ImageMetadata: boost::regex/lexical_cast replaced by std code.
- fstat: field names of tTVP_stat, TVP_stat success test, utime result.
- windowEx: ToUpperCase -> ToUppserCase (Kirikinux2 core spelling).

Core additions required by these plugins (also from KrKr2):
- tTJSBinaryStream::ReadI8LE (src/core/tjs2/tjs.h/.cpp)
- tTJSNI_BaseLayer::BezierPatchCopy/MeshCopy/OperateBezierPatch/OperateMesh and
  ConstructResolvedTreeOwnerLike_0x800438 (src/core/visual/LayerIntf.h/.cpp)

Build: CMakeLists.txt target krkr2plugin_ext (C++17), always linked with
--whole-archive so ncbind self-registration survives.
Logging: set KR2_PLUGIN_LOG=trace|debug|info|warn|error|off (default warn).
