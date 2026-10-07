# Kirikinux2

Kirikinux2 是一个运行在 Linux 桌面上的吉里吉里（KiriKiri2 / KiriKiriZ）游戏播放器。它基于 [Kirikiroid2](https://github.com/zeas2/Kirikiroid2) 的引擎、TJS2 脚本引擎、界面、音视频和插件代码移植而来，让你在 Linux 上直接打开 XP3 封包或游戏目录，无需 Wine。

## 功能

- 支持 XP3 封包和散文件形式的游戏，文本脚本和编译后的 TJS2 字节码均可运行
- 内置 psbfile、motionplayer、KAGParserEx、layerExDraw 等商业游戏常用插件
- 完整的图形界面：打开游戏、最近游戏、全局与单个游戏的设置
- 窗口可自由缩放，列表支持鼠标滚轮和拖动滚动条
- 原生 GTK 对话框和字体选择器，可使用系统字体或从文件添加字体
- 随包附带 CJK 字体，中文、日文界面开箱即用
- 界面提供简体中文、繁体中文、日文和英文
- 以 AppImage 和 deb 两种形式发布

## 安装

从 [Releases](https://github.com/Comet387/kirikinux2/releases) 页面下载最新版本。

**AppImage**：添加可执行权限后直接运行，也可以在文件管理器中双击打开。

```sh
chmod +x kirikinux2-x86_64.AppImage
./kirikinux2-x86_64.AppImage
```

如果系统没有 FUSE，可以加上 `--appimage-extract-and-run` 参数运行。

**deb**：适用于 Debian、Ubuntu 及其衍生发行版。

```sh
sudo apt install ./kirikinux2_<版本>_amd64.deb
```

两种安装包都自带运行库，只使用系统的 glibc 和显卡驱动。目前只提供 x86_64 版本。

## 使用

1. 先把游戏解压到一个可写的普通目录，并保留游戏附带的资源和补丁。
2. 启动 Kirikinux2，点击首页的 **打开游戏**（右上角菜单中也有）。
3. 选择游戏的主封包（通常是 `data.xp3`）、`startup.tjs`，或者直接选择游戏所在的目录。

成功启动过的游戏会出现在首页的最近游戏列表中，之后点一下就能继续玩。如果目录里有多个 XP3 而没有 `data.xp3`，请直接选择主封包。

部分游戏需要专门的补丁才能正常运行，可以在 Kirikiroid2 的 [补丁库](https://zeas2.github.io/Kirikiroid2_patch/patch) 中查找，把补丁文件放进游戏目录即可。补丁由上游社区维护。Kirikinux2 不包含任何商业游戏。

## 问题反馈

遇到问题请在 [Issues](https://github.com/Comet387/kirikinux2/issues) 中反馈，并尽量附上游戏名称、发行版和终端输出。

如果程序崩溃，调用栈会保存在 `~/.local/share/kirikinux/crash.log`，请一并附上。游戏启动时报脚本错误的话，可以用 [XP3 提取工具](tools/README.md) 导出游戏脚本，它只包含脚本和配置，不含图片、音频等资源。

## 从源码构建

在 Debian/Ubuntu 上：

```sh
./scripts/linux/install-deps-debian.sh
./scripts/linux/build.sh -DCMAKE_BUILD_TYPE=Release
./build-linux/bin/kirikinux2
```

构建脚本会自动下载 Cocos2d-x 3.17.2 等依赖，必要时还会编译一份私有的 FFmpeg 4.4.5。完整的构建选项、打包方法和测试说明见 [构建与打包](docs/building.md)。

## 文档

- [构建与打包](docs/building.md)
- [Linux 移植说明](docs/linux-port.md)：架构、启动流程、桌面界面、崩溃报告和环境变量
- [插件](docs/plugins.md)：内置插件和脚本调试工具
- [XP3 提取工具](tools/README.md)

## 代码来源与许可

Kirikinux2 站在许多项目的肩膀上，代码分为三部分，各自保留原有许可：

| 部分 | 主要位置 | 许可 |
| --- | --- | --- |
| 继承自 [Kirikiroid2](https://github.com/zeas2/Kirikiroid2) 的引擎、TJS2、界面和插件 | `src/core`、`src/plugins`、`cocos/kr2` | 吉里吉里许可及各文件中的原有声明，见 [LICENSE](LICENSE) |
| 从 [KrKr2 模拟器](https://github.com/2468785842/krkr2) 移植的插件（psbfile、motionplayer、KAGParserEx、layerExDraw 等） | `src/plugins/ext` | 吉里吉里许可（BSD 风格），见 [LICENSE.krkr2](src/plugins/ext/LICENSE.krkr2) |
| Kirikinux2 自己编写的代码：Linux 平台层、桌面界面、字体选择器、兼容插件、构建脚本、测试和工具 | `platform/linux`、`scripts`、`tests`、`tools` 及 `src` 中的部分文件 | [AGPL-3.0-only](LICENSE-AGPL-3.0) |

Kirikinux2 自己编写的文件都在文件头带有 `SPDX-License-Identifier: AGPL-3.0-only` 标记。Kirikinux2 对上游文件所做的修改，仅新增部分采用 AGPL-3.0-only，文件其余部分仍遵循上游许可。

其他来源还包括 [吉里吉里2](http://kikyou.info/tvp/) 与 [吉里吉里Z](https://github.com/krkrz/krkrz)，来自 [Kodi](https://github.com/xbmc/xbmc) 的视频模块，来自 [krkrz/SamplePlugin](https://github.com/krkrz/SamplePlugin) 的扩展转场，经 [Kirikiroid2Yuri](https://github.com/YuriSizuku/Kirikiroid2Yuri) 恢复的若干上游头文件，以及 Kirikiroid2 中标注的 glibc、Apple Libc、etcpak、pvrtccompressor、astcrt 和 AmazeFileManager 代码。随包附带的 Noto Sans CJK 字体遵循 SIL Open Font License 1.1。

逐个文件的来源、构建时下载的依赖及其许可见 [LICENSING.md](LICENSING.md)。游戏、第三方补丁和用户资源的许可由各自的权利人决定。
