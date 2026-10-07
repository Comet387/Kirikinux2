# 字体选择弹窗与关闭崩溃修复（2026-10-07）

## 1. 关闭窗口时 SIGABRT（`free(): invalid pointer`）

崩溃栈：`TVPWindowLayer::Close` → `ncbInstanceAdaptor<WindowEx>::Invalidate` →
`WindowEx::~WindowEx` → `deleteOverlayBitmap()`（`src/plugins/ext/windowEx.cpp`）。

`WindowEx` 的构造函数没有初始化 `ovbmp`，析构时对随机值执行 `delete`。
现在构造函数初始化全部成员（`ovbmp`、`cachedHWND`、`sysMenu`、`externalIcon`、
`has*`、`bitHooks`）。

## 2. 游戏内“选择字体”没有反应

反汇编游戏脚本后的调用链：

1. `system.tjs` 把 `KAGWindow.selectFontDoUserSelect` 换成游戏自己的
   `MyFontSelectDialog`（`exmenuintf.tjs`，基于 `win32dialog.tjs`）。
2. 该对话框初始化时用 `WIN32Dialog.LB_SETITEMHEIGHT`（owner-draw 列表），
   兼容层没有这个常量 → 异常。
3. 游戏捕获异常后回退到 `Font.doUserSelect`，而引擎里它是返回 0 的 TODO 桩。

## 3. 新的原生字体选择弹窗

不再画游戏的 Win32 模板，也不用 cocos2d 界面，而是弹出 GTK 对话框，支持三种方式：

- 从系统字体（以及引擎已注册的字体）列表里搜索、选择，带预览；
- “从文件添加…”：选择 `.ttf/.otf/.ttc/.otc`，所有字形注册进引擎，
  并记录到全局配置 `user_font_files`，以后每次启动都会自动加载；
- 在“字体名”输入框里直接输入任意字体名。

接入点：

| 场景 | 文件 |
|---|---|
| GTK 界面（只用标准类型） | `platform/linux/LinuxFontPicker.{h,cpp}` |
| 引擎侧：字体列表、注册字体文件、记住文件 | `src/core/visual/FontPicker.{h,cpp}`、`FontImpl.cpp`（`TVPEnumFontsProcCollect`） |
| `Font.doUserSelect`（不再是桩） | `src/core/visual/LayerIntf.cpp` |
| 基于 win32dialog.tjs 的字体选择对话框（有 `fontList` + `initialSelect`，如 `MyFontSelectDialog`、k2compat `FontSelectDialog`）直接转到原生弹窗，并按游戏期望填好 `items.Select` | `src/plugins/win32dialog.cpp` |
| Kirikinux2 设置 → 默认字体：同一个弹窗；保存字体名（旧的文件路径值仍然有效） | `PreferenceConfig.h`、`PreferenceForm.{h,cpp}`、`FontImpl.cpp` |

其他：

- `WIN32Dialog` 补充 `LB_SETITEMHEIGHT`、`LB_GETITEMHEIGHT`、`LB_[GS]ETITEMDATA`、`ODS_*`、`ODT_*`、`ODA_*`、`WM_DRAWITEM` 等常量。
- GTK 初始化统一调用 `gtk_disable_setlocale()`：之前 `LinuxWin32Dialog` 先初始化 GTK 时会把进程切到系统 locale，可能影响 TJS2 的数字格式。
- 没有显示环境时弹窗返回“不可用”，win32dialog 回退到原来的模板渲染。

## 验证

- `tests/plugins/font_picker_gui`（新增，Xvfb）：使用游戏原版编译后的
  `system/win32dialog.tjs`，模拟 `MyFontSelectDialog` 形状的对话框，自动完成
  “搜索后从列表选择 / 输入字体名 / 从文件添加 / 取消”四种操作并检查返回值：PASS。

  ```sh
  xvfb-run -a ./build-plugin-tests/font_picker_gui path/to/system/win32dialog.tjs \
    tests/plugins/font_picker_gui_test.tjs /usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf
  ```
- 原有 `compat_harness`、`win32dialog_gui`、`gtk_dialog_components`（无显示回退）：PASS。
- 修改过的引擎源文件已对照 cocos2d-x 3.17.2 头文件做语法检查。
- **未做**：完整引擎 / AppImage 未在本环境编译和运行，需要在 CI 或本机构建后实测。
