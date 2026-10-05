# Kirikinux2

Linux 桌面的 KiriKiri2 / KiriKiriZ 游戏播放器，项目仓库：
[Comet387/kirikinux2](https://github.com/Comet387/kirikinux2)。

Kirikinux2 复用 Kirikiroid2 的引擎、TJS2、Cocos 界面、声音、视频与插件代码，
在 Linux 平台继续移植与维护。根目录 CMake 构建的是完整引擎。
`linux/` 和 `linux-native/` 保留历史诊断宿主，它们不能代替完整游戏播放器。

## 打开游戏：只需图形界面

1. 给 `kirikinux2-x86_64.AppImage` 添加可执行权限，然后双击打开。
2. 点首页的 **打开游戏**，或右上角菜单 → **打开游戏**。
3. 选择 **游戏文件**（通常为 `data.xp3`），或 **游戏目录**（其中包含
   `startup.tjs` 或 XP3）。选 `startup.tjs` 时会启动其所在目录。
4. 点击 **打开** 后由引擎启动游戏。成功启动后记录最近游戏，之后可直接点播放。

也可以继续使用内置文件浏览器，进入游戏目录并点击 XP3。不需要命令行参数。
“新建文件夹”原来只是创建磁盘目录，并不会新建或导入游戏；本轮已用“打开游戏”替换。
游戏请先解压到可写的普通目录，保留配套资源和补丁。播放器不包含商业游戏。

## 插件移植检查点（2026-10-07）

从 KrKr2 模拟器移植 psbfile / motionplayer / fstat / scriptsEx / TextRender / windowEx，
D3D 并非问题根源。详见 [插件移植报告](docs/PLUGIN_PORT_REPORT.md)（英文）。

## 当前源码检查点

Linux 启动兼容层现在会预先提供 Android 补丁常探测的 `Motion` 命名空间；其 D3D/E-mote 方法是明确的空操作，D3D 仍报告为不可用，因此不会把 Linux 伪装成 Android 渲染器。


连续缺成员的共同启动路径已修正：取消启动文件的文本预读取，不再在任意异常后
直接执行 Initialize；统一管理上游 KAGEX 的四项启动上下文，封包状态来自实际
入口，保留游戏/补丁覆盖。Linux 文件行和最近游戏布局已重写，XP3 工具取消
固定 MiB 限制、支持流式 ZIP64，并独立保留校验异常诊断字节。

**本检查点已验证原 TJS VM 启动行为及工具输出；新完整引擎/AppImage 尚未构建，
新 Cocos 布局和用户游戏尚未实机验证。** 详情见
[检查点记录](docs/startup-ui-xp3-checkpoint.md)。旧安装包不能视为包含本版源码修复。

## 此前桌面修复

- 将窗口、可执行文件、桌面入口和 AppImage 的产品名称统一为 `Kirikinux2`。
- 随包提供 Noto Sans CJK SC 字体，兼顾中文、日文和拉丁字符；Linux 界面使用包内字体的绝对路径。
- 补齐原创 `default.cur`，并在缺失或损坏时关闭可选光标预览，避免全局设置因此退出。
- 设备信息等弹窗限制初始高度，内容可滚动和选取，**复制全部**复制完整文本且不关闭弹窗。
- 关于页移除百度贴吧，项目及问题反馈链接指向本仓库；保留上游补丁库。
- 为 KAG/KAGEX 集中管理 `kirikiriz`、`debugWindowEnabled`、`inXP3archivePacked`、`convertMode`，保留游戏/补丁覆盖，兼容 TJS 字节码；目录启动从游戏目录加载补丁。
- AppImage 使用自带 Fontconfig 配置，避免旧运行库解析较新主机的配置时报错；打开浏览器等主机程序时恢复用户原环境。
- Linux 文件/设置/最近列表支持鼠标滚轮与常驻拖动滚动条；窗口可动态缩放，320×180 时控件保持可读。
- 短退出确认框使用紧凑布局；长信息保留滚动和完整复制，弹窗正文不再打印到终端。
- 外部链接使用主机环境调用浏览器，避免 AppImage 的 OpenSSL 与 KDE 混用；关于页增加 GitHub 按钮。
- 提供本地 XP3 脚本提取工具，保留编译字节码并跳过大资源，见 [工具说明](tools/README.md)。
- `readme.md` 更名为 `README.md`。验证情况详见 [本轮记录](docs/ui-desktop-fixes.md)。

上游补丁库：<https://zeas2.github.io/Kirikiroid2_patch/patch>。
补丁由上游维护，不属于 Kirikinux2 的新增代码。

## 源码构建

Ubuntu / Debian 开发环境：

```sh
./scripts/linux/install-deps-debian.sh
./scripts/linux/build.sh -DCMAKE_BUILD_TYPE=Release
./build-linux/bin/kirikinux2
```

安装依赖脚本会修改系统软件包。`build.sh` 下载带校验值的 Cocos2d-x 3.17.2、
7-Zip 和 UnRAR 源码；如系统 FFmpeg 版本太新，另行构建私有 FFmpeg 4.4.5。
并行度通过 `KR2_BUILD_JOBS` 控制。详情见 [Linux 构建说明](README.linux.md)。

AppImage 打包需 `patchelf` 和官方 `appimagetool`：

```sh
APPIMAGE_EXTRACT_AND_RUN=1 APPIMAGETOOL=/absolute/path/appimagetool-x86_64.AppImage \
  ./scripts/linux/package-appimage.sh
```

产物为 `dist/kirikinux2-x86_64.AppImage` 与 `dist/SHA256SUMS`。
打包检查包括 CJK / 拉丁字形、光标资源、编译后的 Cocos 布局和运行库。
构建日志与实测状态以验证记录为准；不能将编译成功等同于所有游戏兼容。

## 开发进度与下载

[公开进度、验证记录和源码包](https://kirikinux2-progress.lively-plum-4208.chatgpt.site/)
无需认证。页面每 15 秒读取最新发布的进度；源码包与补丁在下载区提供。开发按完成的功能批次发布源码检查点，安装包标注已验证版本。

## 许可与来源

**Kirikinux2 复用的代码按照各自原协议；Kirikinux2 新增的代码采用 AGPLv3
（AGPL-3.0-only）。** 本声明不变更上游或第三方的版权及许可。

- 上游代码的许可、版权和第三方声明保留在 [LICENSE](LICENSE) 及源文件内。
- 新增代码的完整许可见 [LICENSE-AGPL-3.0](LICENSE-AGPL-3.0)，范围见
  [LICENSING.md](LICENSING.md)。对原文件的新增实现仅就新增版权部分适用该声明。
- 包内 Noto Sans CJK 字体遵循 SIL Open Font License 1.1，原声明位于
  `cocos/kr2/Resources/licenses/NotoSansCJK-OFL.txt`。
- 游戏、第三方补丁和用户资源的许可由其权利人决定。

来源包括 [Kirikiroid2](https://github.com/zeas2/Kirikiroid2)、
[Kirikiri2](http://kikyou.info/tvp/)、[KirikiriZ](https://github.com/krkrz/krkrz)、
视频模块 [Kodi](https://github.com/xbmc/xbmc)，以及原仓库标注的 glibc、Apple Libc、
etcpak、pvrtccompressor、astcrt 和 AmazeFileManager。保留全部原有声明。
