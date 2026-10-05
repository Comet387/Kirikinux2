# Kirikinux2 Linux 构建

日常使用直接双击 AppImage，通过首页 **打开游戏** 选择 XP3 或游戏目录。
命令行仅用于开发和自动回归，用户不需要使用它。

窗口可直接拖动边缘调整大小，文件和设置列表支持滚轮与右侧拖动滚动条。
大 XP3 诊断可在 [本地浏览器工具](tools/README.md) 中只导出脚本。

## 完整引擎

根目录 CMake 编译 Kirikinux2 原引擎、TJS2、窗口/图层、声音/视频和插件，使用
Cocos2d-x 3.17.2 与 Linux 平台层。`linux/` 和 `linux-native/` 为历史诊断宿主。

```sh
./scripts/linux/install-deps-debian.sh
KR2_BUILD_JOBS=2 ./scripts/linux/build.sh -DCMAKE_BUILD_TYPE=Release
./build-linux/bin/kirikinux2
```

依赖安装脚本会修改系统软件包。依赖源及下载缓存在 `third_party/`，
源码交付包保留第三方补丁，但不包含可重新下载的依赖树和编译产物。
FFmpeg 5 及以后移除了视频模块依赖的 API，`build.sh` 会准备私有 FFmpeg 4.4.5。

常用选项：`KR2_COCOS2DX_ROOT`、`KR2_THIRD_PARTY`、`KR2_WITH_JXR`、
`KR2_WRAP_MALLOC` 和 `KR2_WHOLE_ARCHIVE`。`KR2_DEFAULT_FONT` 可以覆盖包内字体，
但所提供的字体必须同时覆盖 CJK 与拉丁字形；资源检查不通过时构建失败。
UI `.csd` 在构建时转换为 `.csb`。手动导入 APK UI 后可设置
`KR2_BUILD_UI_RESOURCES=OFF`，但仍须提供完整字体、光标与 `.csb` 资源。

## AppImage

```sh
APPIMAGE_EXTRACT_AND_RUN=1 APPIMAGETOOL=/absolute/path/appimagetool-x86_64.AppImage \
  ./scripts/linux/package-appimage.sh
```

默认输出 `dist/kirikinux2-x86_64.AppImage`，用 `KR2_PACKAGE_DIR` 改变输出目录，
用 `BUILD_DIR` 指定已有构建目录。重复打包时指定新的输出目录。
包含引擎、UI、CJK 字体、光标、FFmpeg、FMOD 与自动解析的运行库；
保留主机 glibc 和显卡驱动栈，不包含游戏。无 FUSE 时可用
`--appimage-extract-and-run`，开发环境可设 `APPIMAGE_EXTRACT_AND_RUN=1`。
本轮提供的 x86_64 构建要求 glibc 2.38 或更新版本，使用 Ubuntu 24.04
依赖构建。较旧发行版请在目标环境从源码构建。

## 验证

```sh
./scripts/linux/check-components.sh
cmake -S tests/linux -B build-components
cmake --build build-components --parallel 2
ctest --test-dir build-components --output-on-failure
```

真实 X11 / OpenGL 游戏回归入口是 `scripts/linux/gui-smoke.py`。
`glxinfo`（mesa-utils）可选；缺少它时 smoke 会跳过 OpenGL 诊断并继续启动，
结果会在 `report.json` 的 `glxinfo.skipped` 中标明。
本轮 UI 回归与实际执行结果见 [docs/ui-desktop-fixes.md](docs/ui-desktop-fixes.md)。
没有测试的游戏、桌面环境和发行版不作兼容性承诺。
BPG 与 XP3 重打包仍未接入，JPEG XR 默认关闭。

项目与许可说明见 [README.md](README.md) 和 [LICENSING.md](LICENSING.md)。
