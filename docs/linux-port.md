# Linux 移植说明

本文介绍 Kirikinux2 如何把 Kirikiroid2 的引擎移植到 Linux 桌面，以及与 Android 版行为不同的地方。构建方法见 [构建与打包](building.md)，插件相关内容见 [插件](plugins.md)。

## 整体结构

`src/core` 和 `src/plugins` 是 Kirikiroid2 的引擎代码，Linux 版按照 `src/core/Android.mk` 和 `src/plugins/Android.mk` 的方式编译它们，源文件列表在 `cmake/Kr2Sources.cmake` 中维护。两个平台之间只有平台层不同：

| Android | Linux |
| --- | --- |
| `src/core/environ/android/AndroidUtils.cpp` | `platform/linux/LinuxUtils.cpp` |
| `project/android/jni/src/SDL_android_main.cpp` | `platform/linux/LinuxMain.cpp` |
| 上游私有修改版 Cocos2d-x | 官方 Cocos2d-x 3.17.2 + `scripts/linux/patch-cocos2dx.py` |

`LinuxUtils.cpp` 导出与 `AndroidUtils.cpp` 相同的符号，桌面行为参照 win32 版的 `Platform.cpp`。它不能放进 `src/core/environ/linux/`，因为 Android 构建会收录该目录，两个文件会被同时链接。

`platform/linux/compat/` 存放上游源码引用但公开仓库中缺失的头文件，以及发行版中路径不同的头文件，它只在 Linux 构建中位于包含路径的最前面，因此 `src/` 中的文件无需为此修改。Android 版仍然可以通过 `project/android` 用 ndk-build 构建。

主要目录：

| 目录 | 内容 |
| --- | --- |
| `src/core` | 引擎核心：TJS2、存储、窗口与图层、声音、视频 |
| `src/plugins` | 内置插件，其中 `src/plugins/ext` 是从 KrKr2 移植的插件 |
| `platform/linux` | Linux 平台层、GTK 对话框、字体选择器、崩溃处理、资源工具 |
| `cocos/kr2` | 界面布局（Cocos Studio）、字体、光标和多语言文本 |
| `cmake` | 源文件列表与第三方依赖配置 |
| `scripts/linux` | 依赖安装、构建、打包和检查脚本 |
| `tests` | 组件测试、插件测试和启动回归游戏 |
| `tools` | XP3 提取工具 |

## 打开游戏

首页和右上角菜单中的 **打开游戏** 会弹出系统文件选择框，可以选择 XP3 封包、`startup.tjs` 或游戏目录。选择目录时按以下顺序决定入口：

1. 目录中有 `startup.tjs`，直接从该目录启动；
2. 否则挂载 `data.xp3`；
3. 否则如果目录里只有一个 XP3，就挂载它。

目录中有多个 XP3 但没有 `data.xp3` 时，播放器不会猜测哪个是主封包，请直接选择主文件，以免误开资源包或补丁包。从目录启动时，游戏目录中的 `patch.tjs` 会被加载，游戏的独立设置也保存在该目录。

也可以通过命令行启动，这主要用于开发和测试：

```sh
kirikinux2 [--size=WIDTHxHEIGHT] [--fullscreen] [game-dir | archive.xp3 [name=value ...]]
```

不带参数时显示文件选择界面，环境变量 `KIRIKINUX_GAME_DIR` 可以指定它的初始目录。

## 启动流程

引擎直接用存储执行器运行 `startup.tjs`，文本脚本和编译后的 TJS 字节码都可以使用。只有在启动脚本不存在时，才会回退到执行 `System/Initialize.tjs`；启动脚本本身的解码错误、插件错误和脚本异常都会如实报告，不会被吞掉后继续初始化。`patch.tjs` 出错会中止启动，`AfterStartup.tjs` 的错误也会报告。

### 启动上下文

许多 KAG/KAGEX 游戏和上游补丁会在启动早期读取几个框架约定的全局变量。`src/core/base/StartupCompatibility.h` 在存储和过滤器准备就绪之后、游戏补丁运行之前统一设置它们。已经存在的值不会被覆盖，游戏和补丁仍然可以修改：

| 变量 | 默认值 |
| --- | --- |
| `kirikiriz` | `false`，本引擎使用 krkr2 API |
| `debugWindowEnabled` | `false`，与 VM 的 `-debug` 选项无关 |
| `inXP3archivePacked` | 启动入口位于封包内时为 `true`，散文件为 `false` |
| `convertMode` | `false` |

这些是框架层面的约定，并不是原版引擎的原生接口。引擎不会对任意未知的变量名返回默认值，其他缺失的成员仍会正常报错。

### Motion 命名空间

面向 Android 的补丁常在 `startup.tjs` 一开始就检查 `Motion.D3DAdaptor`，然后关闭 `Motion.Player.useD3D` 和 `Motion.EmotePlayer.useD3D`。如果 `Motion` 本身不存在，这次检查就会抛出异常。因此启动上下文会预先建立 `Motion.Player`、`Motion.EmotePlayer` 和可构造的 `Motion.ResourceManager`，其中的方法和回调设置都是空操作；`Motion.D3DAdaptor` 保持未定义，让脚本走非 D3D 分支。游戏链接 `motionplayer.dll` 后，真正的插件会替换这个命名空间。

## 桌面界面

- **窗口**：可以自由缩放。缩放时会更新设计分辨率、界面和游戏显示区域，游戏自身的逻辑分辨率不变，最小到 320×180 时控件仍然可读。
- **列表**：文件列表、设置页、最近游戏和菜单都支持鼠标滚轮，并带有常驻、可拖动的滚动条。
- **文件名显示**：名字按字体实际宽度在 Unicode 字符边界截断并加上 `...`，控制字符和无效 UTF-8 会被替换，但打开文件时始终使用完整的原始路径。
- **对话框**：消息框、输入框和文件选择使用 GTK。短消息使用紧凑布局；长文本（如设备信息）可以滚动和选取，**复制全部** 会复制完整内容且不关闭对话框。
- **外部链接**：通过 `xdg-open` 打开，URL 作为单个参数传递，调用前会恢复启动前的主机环境变量，避免包内运行库与桌面环境冲突。
- **界面字体**：随包提供 Noto Sans CJK SC，覆盖中文、日文和拉丁字符。AppImage 使用自带的 `fonts.conf`，避免旧版 Fontconfig 解析较新主机配置时出错。
- **光标**：随包提供 `default.cur`，缺失或损坏时会跳过光标预览，不影响进入设置。

## 字体

引擎会通过 Fontconfig 列出系统中安装的字体，支持 Windows 风格的本地化字体名和全名查找，并能按语言选择 `.ttc` 字体集合中的具体字形。设置环境变量 `KIRIKINUX_NO_SYSTEM_FONTS=1` 可以禁止枚举系统字体。

`Font.doUserSelect`、基于 `win32dialog.tjs` 的游戏字体选择对话框，以及 **设置 → 默认字体** 都会打开同一个 GTK 字体选择器。它支持三种方式：在系统字体列表中搜索并预览、从文件添加 `.ttf/.otf/.ttc/.otc`，或直接输入字体名。从文件添加的字体会记录在全局配置的 `user_font_files` 中，之后每次启动自动加载。没有图形环境时，字体选择器会报告不可用，win32dialog 回退到模板渲染。

GTK 初始化时会调用 `gtk_disable_setlocale()`，防止进程 locale 被切换而影响 TJS2 的数字格式。

## 退出

点击窗口的关闭按钮时，如果正在运行游戏，会执行游戏自己的关闭确认（与游戏内菜单的“退出”相同）；在文件选择界面则询问是否退出。如果游戏没有响应关闭请求，再次点击关闭按钮会提供强制退出。退出时先刷新输出，再用 `_exit()` 结束进程，跳过 C++ 静态析构，避免引擎仍在运行时析构全局对象导致崩溃。

## 崩溃报告

收到 SIGSEGV、SIGBUS、SIGFPE、SIGILL 或 SIGABRT 时，`platform/linux/LinuxCrashHandler.cpp` 会在进程内回溯调用栈，输出“模块 + 偏移”和主程序中的函数名；如果系统安装了 `addr2line`，还会给出源码行号（最多等待 15 秒）。报告写到标准错误和 `~/.local/share/kirikinux/crash.log`，随后恢复默认信号处理并重新触发信号，core dump 照常生成。

引擎代码默认以 `-g1` 编译并带有 build-id，因此报告可以被符号化。用 gdb 调试时可以设置 `KIRIKINUX_NO_CRASH_HANDLER=1` 关闭崩溃处理。

## 环境变量

| 变量 | 作用 |
| --- | --- |
| `KIRIKINUX_GAME_DIR` | 文件选择界面的初始目录 |
| `KIRIKINUX_NO_CRASH_HANDLER` | 设为 `1` 时关闭崩溃处理 |
| `KIRIKINUX_NO_SYSTEM_FONTS` | 设为 `1` 时不枚举系统字体 |
| `KR2_PLUGIN_LOG` | 移植插件的日志级别：`trace`、`debug`、`info`、`warn`（默认）、`error`、`off` |

## 已知限制

- 没有实现 Direct3D，依赖 D3D 的功能会走游戏自带的非 D3D 分支。
- BPG 图片和 XP3 重新封包尚未接入，JPEG XR 默认关闭。
- 使用游戏专用加密的封包需要对应的补丁或解密实现。
- 只发布 x86_64 版本，其他架构需要自行从源码构建。
