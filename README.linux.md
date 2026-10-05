# kirikinux2 Linux：原版核心移植

本轮按 `claude-work-logs.txt` 的最新方案保留 Cocos2d-x 3.17.2，复用原版
`Window/Layer/MainScene/AppDelegate`、TJS、字体、声音、视频和插件代码。
根目录 CMake 是完整引擎的新构建入口。`linux/` 和 `linux-native/` 是此前的
诊断宿主/前端，单独保留；它们的测试通过不代表游戏可以运行。
历史诊断宿主仅供手动排查归档和脚本；其工作流只接受 `workflow_dispatch`，
不再随 push/PR 自动构建，也不再生成 `.deb`/AppImage。日常 CI 只验收根目录原版引擎。

route04 接入上游原版 extrans 扩展转场，已完成完整链接、全新存档 180 秒开场/正文、
安装目录的保存/重启读档及实际音频混音输出。最新报告见
[docs/linux-route04.md](docs/linux-route04.md)。route03 的 GTK 头搜索顺序修复见
[docs/linux-ci-header-fix.md](docs/linux-ci-header-fix.md)；route02/03 的报告保留为历史证据。

**原版完整引擎已在 Linux 构建成功，fork3 的真实 Spring Days XP3 已进入剧情，鼠标、键盘、音频输出及保存后重启读档通过实测。**
本轮截图、验证范围和复现方法见 [docs/linux-route04.md](docs/linux-route04.md)；
route02 历史记录见 [docs/linux-game-verification.md](docs/linux-game-verification.md)；
移植路线与 Claude 日志的具体修正见 [docs/linux-port.md](docs/linux-port.md)。

## 构建原版核心

以下构建入口已在本轮 Ubuntu 24.04 / GCC 13.3 环境验收。
推荐先使用 Ubuntu 22.04 的 FFmpeg 4.x 开发库；安装脚本会修改系统软件包：

```sh
./scripts/linux/install-deps-debian.sh
./scripts/linux/build.sh -DCMAKE_BUILD_TYPE=Release
./build-linux/bin/kirikiroid2 /absolute/path/to/game-or-data.xp3
```

`build.sh` 先检查工具，再获取缺失依赖；通过 `KR2_BUILD_JOBS=2` 控制并行度。
下载缓存与解压依赖在 `third_party/`，源码包不包含这些依赖。
若系统 FFmpeg >= 5 或没有 FFmpeg 开发库，会构建私有 FFmpeg 4.4.5。
安装时默认将 FFmpeg 与 FMOD 共享库放入引擎的 lib 目录，启动器设置加载路径。
GTK、OpenGL、OpenAL 等系统依赖仍需安装；安装入口 --help、迁移后的启动器实际运行与游戏读档均已通过。

## AppImage

原版引擎的打包入口是 `scripts/linux/package-appimage.sh`，复用根目录构建结果，
不构建 `linux/` 或 `linux-native/` 诊断宿主。安装 `patchelf` 后，将官方
`appimagetool` 放入 PATH，或通过 `APPIMAGETOOL` 指定其绝对路径：

```sh
./scripts/linux/build.sh -DCMAKE_BUILD_TYPE=Release
APPIMAGE_EXTRACT_AND_RUN=1 APPIMAGETOOL=/absolute/path/appimagetool-x86_64.AppImage \
  ./scripts/linux/package-appimage.sh
chmod +x dist/Kirikiroid2-original-x86_64.AppImage
./dist/Kirikiroid2-original-x86_64.AppImage /absolute/path/to/game-or-data.xp3
```

不传游戏路径时打开原版文件选择器。无 FUSE 的常规 Linux 环境可使用
`--appimage-extract-and-run`。输出目录由 `KR2_PACKAGE_DIR` 控制；再次打包时选择
新的输出目录。`BUILD_DIR` 可指定已有的原版 CMake 构建目录。

包内包含引擎、原版 UI、CJK 字体、FFmpeg、FMOD 和自动解析出的运行库；保留主机
glibc 和 OpenGL/显卡驱动。此次本机构建版本面向 x86_64 Ubuntu 24.04 或更新版本，
其他发行版尚未验收。不包含游戏。打包验证及限制见
[docs/linux-appimage.md](docs/linux-appimage.md)。远端 CI 已增加原版 AppImage 打包、
真实 XP3 回归和独立制品上传步骤，尚未在远端执行。

分步执行：

```sh
./scripts/linux/fetch-thirdparty.sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel 2
# 安装到自定义目录：
cmake --install build-linux --prefix "$PWD/install-linux"
```

常用选项：`KR2_COCOS2DX_ROOT` / `KR2_THIRD_PARTY` 指定依赖源目录；
`KR2_DEFAULT_FONT` 指定 CJK 字体；`KR2_WITH_JXR=ON` 要求 jxrlib；
`KR2_WRAP_MALLOC=OFF` 关闭原来的 malloc 包装；`KR2_WHOLE_ARCHIVE` 默认打开，
用于保留自注册插件。修改这些选项后需要重新构建。

UI `.csd` 在构建时由 Cocos 原版序列化器转换成 `.csb`，任一资源失败都会使构建失败。
如果已有自己提供的原版 APK，可用 `import-apk-assets.sh <apk>` 提取 UI，
并以 `-DKR2_BUILD_UI_RESOURCES=OFF` 关闭转换。关闭转换而未提供 `.csb` 会缺少 UI。

## 已可重复的组件检查

不需要 Cocos/OpenGL/FFmpeg 开发库：

```sh
./scripts/linux/check-components.sh
cmake -S tests/linux -B build-components
cmake --build build-components --parallel 2
ctest --test-dir build-components --output-on-failure
```

依赖已下载后，加 `-DKR2_COMPONENT_ARCHIVES=ON` 检查真实 7-Zip/UnRAR 静态库链接。
旧诊断宿主可以独立执行 `cmake -S linux -B build-diagnostic` 和 CTest；
该程序名称是 `kirikiroid2-linux`，完整引擎名称是 `kirikiroid2`。

## 游戏回归与后续验收

已完成完整编译、34 项 UI 资源转换、原版选择器、真实 XP3 标题及剧情、鼠标/键盘、
音频解码混音输出，以及游戏内保存后重启读档。GUI 回归入口：

```sh
python3 scripts/linux/gui-smoke.py --engine build-linux/bin/kirikiroid2 \
  --game /absolute/path/data.xp3 --output build-linux/gui-evidence --seconds 180 \
  --capture-at 24 --capture-at 48 --action click:10:450:235 --action focus:150 \
  --action key:154:space --action key:158:Return --action click:162:800:650 \
  --require-log 'Scenario loaded : ch2004.ks' --require-log 'Startup script ended.' \
  --require-log 'returned to : *ch01 line offset 11' \
  --forbid-log 'Cannot find transition handler' --forbid-log 'Cannot find transition hander'
python3 scripts/linux/check-frame-region.py --report build-linux/gui-evidence/report.json \
  --reference frame-151.0.png --after 151 --region 160:0:1120:400 \
  --output build-linux/gui-evidence/background-region.json
```

这些坐标和日志只适用于本次 fork3 样本。普通 Linux 环境需 Xvfb、mesa-utils、libXtst；
脚本会启动真实 X11/OpenGL 引擎、发送输入并保存截图及 JSON 报告。
全新存档需走完约 111 秒开场，不能用 30 秒超时判定失败。route04 已通过现有
Plugins.link/ncbind 链加载原版 extrans；真实开场执行旋转与波纹，不再依赖缺失提示和回退。
区域比对只用于场景应保持不变的区间；人物出现、菜单打开等正常画面变化需要排除。
后续继续验证实际视频播放、更多游戏，以及桌面发行包。
GitHub CI 已加入真实 XP3 GUI 检查。本源码的最终 CI 输入序列已本机通过；公开仓库的
旧提交仍构建失败，新修正尚未写入该仓库或触发远端工作流，不能宣称远端通过。

BPG、XP3 重打包暂未接入；JPEG XR 默认关闭。遇到这些功能会明确报错。
保留原版 XP3 过滤器代码不等于已经验证所有受保护游戏兼容性。
