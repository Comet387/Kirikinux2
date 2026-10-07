# 代码来源与许可

Kirikinux2 由三部分代码组成：从 Kirikiroid2 等上游项目继承的代码、从 KrKr2 模拟器移植的插件，以及 Kirikinux2 自己编写的代码。每部分都保留各自的许可，本项目不会把第三方代码改为 AGPLv3。

| 部分 | 许可 | 许可文本 |
| --- | --- | --- |
| Kirikinux2 自己编写的代码 | AGPL-3.0-only | [LICENSE-AGPL-3.0](LICENSE-AGPL-3.0) |
| 来自 KrKr2 的插件 | 吉里吉里许可（BSD 风格） | [src/plugins/ext/LICENSE.krkr2](src/plugins/ext/LICENSE.krkr2) |
| 来自 Kirikiroid2 及其他上游的代码 | 吉里吉里许可及各文件中的原有声明 | [LICENSE](LICENSE) 和各源文件 |
| 随包字体 Noto Sans CJK SC | SIL Open Font License 1.1 | [NotoSansCJK-OFL.txt](cocos/kr2/Resources/licenses/NotoSansCJK-OFL.txt) |

判断某个文件属于哪一部分时，以文件头的声明为准：Kirikinux2 自己编写的文件都带有 `SPDX-License-Identifier: AGPL-3.0-only` 标记，没有该标记的文件按其原有声明和下文的来源说明处理。

## Kirikinux2 自己编写的代码

以下文件完全由 Kirikinux2 编写，采用 GNU Affero General Public License version 3 only（AGPL-3.0-only）：

| 位置 | 内容 |
| --- | --- |
| `platform/linux/` | Linux 平台层：程序入口 `LinuxMain.cpp`、GTK 对话框 `LinuxDialogs.cpp`、外部程序调用 `LinuxHost.cpp`、崩溃处理 `LinuxCrashHandler.cpp`、字体选择器 `LinuxFontPicker.*`、win32dialog 的 GTK 渲染 `LinuxWin32Dialog.*`、功能占位 `LinuxFeatureStubs.cpp` 和 `LinuxNoNeonStubs.c`、启动脚本模板 `kirikinux-launcher.in`，以及 `tools/` 下的资源转换和检查工具 |
| `platform/linux/compat/` | 兼容头文件 `Protect.h`、`aligned_allocator.h`、`jinclude.h`、`kr2_ffmpeg_compat.h`、`legacy_libm.c`、`tvpgl_route.h`、`tvpgl_arm_route.h`，以及 `libarchive/`、`lz4/` 下的转发头文件（`xmmlib.h` 和 `p7zip/` 除外，见下文） |
| `src/core/` | 启动上下文 `base/StartupCompatibility.h`，桌面界面 `environ/ui/DesktopScroll.*`、`DesktopFileLayout.h`、`FileNameLayout.h`，字体名称表 `visual/FontNameTable.*` 和字体选择 `visual/FontPicker.*` |
| `src/plugins/ext/` | `compat/` 下的 `getLangName.dll`、`layerExSave.dll`、`PackinOne.dll`、`win32ole.dll` 实现，`MotionPlayerNoD3D.cpp`，以及日志头文件 `kr2_plugin_log.h` |
| `cocos/kr2/Resources/` | 光标 `default.cur`（由 `scripts/linux/generate-cursor.py` 生成）和 `fonts.conf` |
| 构建与测试 | `CMakeLists.txt`、`cmake/`、`scripts/linux/`、`tests/`、`.github/` |
| `tools/` | XP3 提取工具 |

## Kirikinux2 对上游文件的修改

为了移植到 Linux，Kirikinux2 修改了一部分上游文件，例如 `src/core` 中的存储、配置、字体、图层和文件选择界面，`src/plugins/win32dialog.cpp`，以及 `cocos/kr2` 中的多语言文本。`platform/linux/LinuxUtils.cpp` 是参照 Android 版 `AndroidUtils.cpp` 和 win32 版 `Platform.cpp` 编写的 Linux 平台实现，也属于这一类。

这些文件整体上仍然遵循上游的原有许可，其中由 Kirikinux2 新增的部分采用 AGPL-3.0-only。再分发时请同时遵守两者。

## 来自 KrKr2 的代码

`src/plugins/ext/` 中除上面列出的 Kirikinux2 文件之外，其余插件移植自 [KrKr2 模拟器](https://github.com/2468785842/krkr2)（版本 `dca7264572a63e753c1b07ca7053513d1751ce70`），采用吉里吉里的 BSD 风格许可，许可文本见 [src/plugins/ext/LICENSE.krkr2](src/plugins/ext/LICENSE.krkr2)（与根目录的 `LICENSE` 内容相同），再分发时须保留该声明。包括：

| 插件 | 位置 | 说明 |
| --- | --- | --- |
| `psbfile.dll` | `psbfile/` | |
| `motionplayer.dll`、`emoteplayer.dll` | `motionplayer/`、`EmotePlayer.cpp` | `motionplayer/docs/` 是 KrKr2 的开发文档 |
| `fstat.dll`、`scriptsEx.dll`、`TextRender.dll`、`windowEx.dll` | `fstat/`、`scriptsEx.cpp`、`TextRender.cpp`、`windowEx.cpp` | |
| `KAGParserEx.dll` | `kagparserex/` | 原作者 miahmie、wtnbgo，最初发布于 [krkrz/krkr2](https://github.com/krkrz/krkr2) |
| `layerExDraw.dll` | `layerExDraw/` | 原作者わたなべごう（wtnbgo），blend2d 后端由 KrKr2 编写 |
| `krkrsteam.dll` | `steam/` | |

此外，`src/core` 中为这些插件补充的 `tTJSBinaryStream::ReadI8LE`，以及图层的 `BezierPatchCopy`、`MeshCopy`、`OperateBezierPatch`、`OperateMesh` 等函数也来自 KrKr2。Kirikinux2 对移植代码的修改见 [src/plugins/ext/README.md](src/plugins/ext/README.md)。

## 来自其他项目的代码

| 来源 | 位置 | 许可 |
| --- | --- | --- |
| [Kirikiroid2](https://github.com/zeas2/Kirikiroid2)（zeas2） | `src/core`、`src/plugins`、`cocos/kr2` 界面资源、`project/android`、`privacy_policy.txt` | 吉里吉里许可及第三方声明，见 [LICENSE](LICENSE) |
| [吉里吉里2](http://kikyou.info/tvp/) / [吉里吉里Z](https://github.com/krkrz/krkrz)（W.Dee 及贡献者） | Kirikiroid2 所继承的引擎核心和 TJS2 | 同上 |
| [Kodi](https://github.com/xbmc/xbmc) | `src/core/movie/ffmpeg` 视频模块（经 Kirikiroid2 引入） | 见上游声明 |
| glibc、Apple Libc、etcpak、pvrtccompressor、astcrt、AmazeFileManager | Kirikiroid2 中标注的相应文件 | 见各文件及 [LICENSE](LICENSE) |
| [krkrz/SamplePlugin](https://github.com/krkrz/SamplePlugin)（extrans） | `src/plugins/extrans/` | 吉里吉里Z 许可，见 [UPSTREAM.md](src/plugins/extrans/UPSTREAM.md) |
| [Kirikiroid2Yuri](https://github.com/YuriSizuku/Kirikiroid2Yuri) 恢复的上游文件 | `platform/linux/compat/xmmlib.h` | Risa/吉里吉里3 与 Ogg Vorbis 的 BSD 风格许可，见 [VORBIS-COPYING](platform/linux/compat/VORBIS-COPYING) |
| 同上 | `platform/linux/compat/p7zip/` | LZMA SDK，Igor Pavlov，公有领域 |
| [Noto Sans CJK](https://github.com/notofonts/noto-cjk)（Adobe） | `cocos/kr2/Resources/DroidSansFallback.ttf` | SIL OFL 1.1 |

`DroidSansFallback.ttf` 实际上是未经修改的 Noto Sans CJK SC Regular，沿用这个文件名是为了兼容上游界面。字体不属于 AGPLv3 代码，作者信息和保留字体名称声明见 `cocos/kr2/Resources/licenses/`。

## 构建时下载的依赖

以下依赖不在仓库中，由 `scripts/linux/fetch-thirdparty.sh` 和 `build-ffmpeg4.sh` 在构建时下载，各自遵循原有许可。打包脚本会把它们的许可文件和发行版的版权声明一并放进安装包的 `usr/share/doc/` 中。

| 依赖 | 许可 |
| --- | --- |
| Cocos2d-x 3.17.2 及其预编译依赖 | MIT；其中的 FMOD 等预编译库遵循各自的许可 |
| p7zip 16.02（LZMA SDK 部分） | 公有领域 |
| UnRAR 6.0.7 | unRAR 许可 |
| blend2d | Zlib |
| FFmpeg 4.4.5（私有构建） | LGPL-2.1-or-later（未启用 GPL 组件）；使用发行版 FFmpeg 时以发行版为准 |

游戏、第三方补丁和用户资源的许可由各自的权利人决定，Kirikinux2 不包含任何商业游戏。代码中保留的旧名称、上游项目地址、补丁库地址和旧配置文件名仅用于标明来源和兼容旧设置。
