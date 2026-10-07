# Ported plugins (src/plugins/ext)

Source: KrKr2 emulator, https://github.com/2468785842/krkr2
Revision: dca7264572a63e753c1b07ca7053513d1751ce70 (2026-06-11)
Licence of the imported code: KiriKiri BSD-style licence, see LICENSE.krkr2
(compatible with this project's AGPL-3.0; keep the notice when redistributing).

| Module name (Plugins.link) | Directory / file           | Notes |
|----------------------------|----------------------------|-------|
| psbfile.dll                | psbfile/                   | PSB/PIMG/SCN/MTN parser and `psb://` storage media |
| motionplayer.dll           | motionplayer/              | M2 Motion / E-mote player |
| emoteplayer.dll            | EmotePlayer.cpp            | alias that loads motionplayer.dll |
| motionplayer_nod3d.dll     | MotionPlayerNoD3D.cpp      | Kirikinux2 alias that loads motionplayer.dll |
| fstat.dll                  | fstat/main.cpp             | adapted to Kirikinux2 tTVP_stat/TVP_utime |
| scriptsEx.dll              | scriptsEx.cpp              | |
| TextRender.dll             | TextRender.cpp             | |
| windowEx.dll               | windowEx.cpp (+win32_dt.h) | all members initialised in the constructor |
| KAGParserEx.dll            | kagparserex/               | original plugin (krkrz/krkr2 svn) wrapped in a namespace |
| layerExDraw.dll (GdiPlus)  | layerExDraw/               | blend2d backend; needs third_party/blend2d |
| getLangName.dll            | compat/getLangName.cpp     | Kirikinux2 implementation |
| layerExSave.dll            | compat/layerExSave.cpp     | Kirikinux2 implementation; forwards to Layer.saveLayerImage |
| PackinOne.dll              | compat/PackinOne.cpp       | Kirikinux2 implementation; loads saveStruct/varfile/fstat/scriptsEx/csvParser/dirlist/layerExSave |
| win32ole.dll               | compat/win32ole.cpp        | Kirikinux2 implementation; class exists, constructor throws (no COM on Linux) |
| krkrsteam.dll              | steam/krkrsteam.cpp        | KrKr2 stub; locale-based getLanguage, cloud disabled |

Local changes versus upstream (search for "Kirikinux2"):
- spdlog/fmt/boost replaced by kr2_plugin_log.h (no new runtime libraries).
- psbfile ImageMetadata: boost::regex/lexical_cast replaced by std code.
- psbfile main.cpp: `PSBFile.load()` registers its resources through the
  `psb://` media registry instead of an unassigned static pointer.
- fstat: field names of tTVP_stat, TVP_stat success test, utime result.
- windowEx: ToUpperCase -> ToUppserCase (Kirikinux2 core spelling); members
  initialised in the constructor.

Core additions required by these plugins (also from KrKr2):
- tTJSBinaryStream::ReadI8LE (src/core/tjs2/tjs.h/.cpp)
- tTJSNI_BaseLayer::BezierPatchCopy/MeshCopy/OperateBezierPatch/OperateMesh and
  ConstructResolvedTreeOwnerLike_0x800438 (src/core/visual/LayerIntf.h/.cpp)

Build: CMakeLists.txt target krkr2plugin_ext (C++17), always linked with
--whole-archive so ncbind self-registration survives.
Logging: set KR2_PLUGIN_LOG=trace|debug|info|warn|error|off (default warn).
