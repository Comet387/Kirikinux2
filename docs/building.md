# 构建与打包

本文介绍如何在 Linux 上从源码构建 Kirikinux2、生成 AppImage 和 deb 安装包，以及如何运行测试。只想玩游戏的话不需要读这篇，直接下载 AppImage 即可。

## 环境要求

- x86_64 Linux，推荐 Ubuntu 22.04 或更新的 Debian/Ubuntu 系发行版
- CMake 3.13+、GCC（或 Clang）、Python 3、git、curl、tar、unzip、make、sha256sum

Debian/Ubuntu 可以用脚本一次装好所有依赖。脚本会通过 `apt-get` 修改系统软件包，执行前请先确认：

```sh
./scripts/linux/install-deps-debian.sh
```

其他发行版请参考脚本中的包列表安装对应的开发包。

## 构建

```sh
./scripts/linux/build.sh -DCMAKE_BUILD_TYPE=Release
./build-linux/bin/kirikinux2
```

`build.sh` 依次完成以下工作：

1. 运行 `fetch-thirdparty.sh`，把 Cocos2d-x 3.17.2 及其预编译依赖、7-Zip、UnRAR 和 blend2d 下载到 `third_party/`，每个压缩包都会校验 SHA-256。
2. 如果系统的 FFmpeg 是 5.0 或更新版本（视频模块依赖的接口在 5.0 中被移除），或系统没有 FFmpeg，则调用 `build-ffmpeg4.sh` 构建一份私有的 FFmpeg 4.4.5。
3. 用 `patch-cocos2dx.py` 给 Cocos2d-x 打上引擎依赖的接口补丁（可重复执行），然后配置并编译整个项目。

传给 `build.sh` 的参数会原样交给 CMake。并行编译数由环境变量 `KR2_BUILD_JOBS` 控制（默认 2），构建目录由 `BUILD_DIR` 指定（默认 `build-linux`）。

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `KR2_THIRD_PARTY` | `third_party/` | 第三方源码目录 |
| `KR2_COCOS2DX_ROOT` | `third_party/cocos2d-x` | Cocos2d-x 3.17.2 源码目录 |
| `KR2_DEFAULT_FONT` | 自动检测 | 界面字体，必须同时覆盖 CJK 和拉丁字形，否则资源检查失败 |
| `KR2_BUILD_UI_RESOURCES` | ON | 构建时把 Cocos Studio 的 `.csd` 布局转换为 `.csb` |
| `KR2_WITH_BLEND2D` | ON | 使用 blend2d 构建 `layerExDraw.dll` |
| `KR2_WITH_JXR` | OFF | 启用 JPEG XR 解码（需要 `libjxr-dev`） |
| `KR2_FORCE_BUNDLED_UNRAR` | OFF | 即使系统提供 libunrar 也使用自带的 UnRAR |
| `KR2_BUNDLE_FFMPEG` | ON | 安装时把链接到的 FFmpeg 共享库一起复制 |
| `KR2_DEBUG_LINE_TABLES` | ON | 以 `-g1` 编译引擎代码，让崩溃报告能解析出函数名和行号 |
| `KR2_WHOLE_ARCHIVE` | ON | 以 `--whole-archive` 链接引擎和插件，保留自注册的插件与解码器 |
| `KR2_WRAP_MALLOC` | ON | 与 Android 版一样包装 `malloc` 等内存函数 |
| `KR2_PERMISSIVE` | ON | 放宽旧代码在新编译器上的部分诊断 |

如果 `.csd` 到 `.csb` 的转换出现问题，可以设置 `-DKR2_BUILD_UI_RESOURCES=OFF`，再用 `scripts/linux/import-apk-assets.sh` 从 Kirikiroid2 1.3.9 的 APK 中导入已发布的界面资源。此时字体、光标和 `.csb` 布局都需要齐全。

## 安装

```sh
cmake --install build-linux --prefix /usr/local
```

引擎及其私有运行库安装在 `lib/kirikinux/` 下，`bin/` 中的 `kirikinux2` 是启动脚本，同时会安装桌面入口和图标。

## 打包

### AppImage

需要 `patchelf` 和官方的 [appimagetool](https://github.com/AppImage/appimagetool)：

```sh
APPIMAGE_EXTRACT_AND_RUN=1 APPIMAGETOOL=/absolute/path/appimagetool-x86_64.AppImage \
  ./scripts/linux/package-appimage.sh
```

产物是 `dist/kirikinux2-x86_64.AppImage` 和 `dist/SHA256SUMS`。AppImage 内包含引擎、界面资源、CJK 字体、光标、FFmpeg、FMOD 以及自动收集的运行库，使用主机的 glibc 和显卡驱动，不包含任何游戏。最低 glibc 版本与构建环境一致，需要支持较旧发行版时请在对应的旧环境中构建。

打包前会检查字体的 CJK/拉丁字形、光标、编译后的界面布局和运行库是否齐全。输出目录可以用 `KR2_PACKAGE_DIR` 修改，已有构建目录用 `BUILD_DIR` 指定。打包脚本不会覆盖已存在的 AppDir，重复打包时请换一个输出目录或先清理 `dist/`。

### deb

在生成 AppImage 之后运行：

```sh
./scripts/linux/package-deb.sh
```

它会复用同一个 AppDir，生成 `dist/kirikinux2_<版本>_amd64.deb`。安装包自带运行库，只依赖主机的 glibc 和 `libgl1`。可以用 `KR2_DEB_VERSION` 和 `KR2_DEB_MAINTAINER` 覆盖版本号和维护者信息。

## 测试

不需要完整构建引擎的组件测试：

```sh
./scripts/linux/check-components.sh
cmake -S tests/linux -B build-components
cmake --build build-components --parallel 2
ctest --test-dir build-components --output-on-failure
```

插件测试（ncbind、psbfile、兼容插件、win32dialog）：

```sh
cmake -S tests/plugins -B build-plugin-tests -G Ninja
cmake --build build-plugin-tests
ctest --test-dir build-plugin-tests --output-on-failure
```

启动流程和 XP3 工具的回归测试：

```sh
python3 scripts/linux/check-startup.py
python3 tests/linux/xp3_tool_regressions.py
```

图形界面回归测试在 Xvfb 中启动引擎、截取真实的 X11 画面并模拟输入，需要 Xvfb、libX11 和 libXtst。安装了 `glxinfo`（mesa-utils）时会额外记录 OpenGL 信息，没有也不影响测试。例如：

```sh
python3 scripts/linux/gui-smoke.py --engine build-linux/bin/kirikinux2 \
  --game tests/linux/games/startup-compat --output startup-test --seconds 7 \
  --require-log KIRIKINUX_STARTUP_COMPAT_TEXT_AND_BYTECODE_OK
```

`tests/linux/games/startup-compat-override` 用同样的方式检查游戏补丁能否覆盖引擎提供的默认值，对应的日志标记是 `KIRIKINUX_STARTUP_COMPAT_PATCH_OVERRIDE_OK`。

## 持续集成

`.github/workflows/build.yml` 在每次推送和拉取请求时运行两个任务：`tests` 执行组件和插件测试，`package` 完整构建引擎并打包。全部成功时只上传 `kirikinux2-appimage` 和 `kirikinux2-deb` 两个产物；任一任务失败、取消或超时时，会上传该任务的整个工作区（`*-workspace`）用于排查。
