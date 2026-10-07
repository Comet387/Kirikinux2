# 插件

吉里吉里游戏通过 `Plugins.link("xxx.dll")` 加载插件。Windows 上这些是真正的 DLL，Kirikinux2 则把对应的实现直接编译进引擎，按 DLL 名注册。引擎遇到未知的 DLL 名时会静默忽略，所以缺少插件通常不会在加载时报错，而是在脚本稍后使用插件提供的类时出现“成员不存在”之类的错误。排查启动问题时，可以先确认游戏链接了哪些插件。

## 内置插件

Kirikiroid2 公开源码中自带的插件位于 `src/plugins`。很多商业游戏还依赖 APK 中内置、但公开源码里没有的插件，这些插件从 [KrKr2 模拟器](https://github.com/2468785842/krkr2) 移植而来，放在 `src/plugins/ext`，详细列表和与上游的差异见 [src/plugins/ext/README.md](../src/plugins/ext/README.md)。

| 插件 | 用途 |
| --- | --- |
| `psbfile.dll` | 读取 PSB 格式的剧本（`.scn`）、动画（`.mtn`）和图层图片（`.pimg`），并提供 `psb://` 存储介质 |
| `motionplayer.dll`、`emoteplayer.dll`、`motionplayer_nod3d.dll` | M2 Motion / E-mote 动画播放，后两个是加载同一实现的别名 |
| `KAGParserEx.dll` | 扩展 KAG 解析器，链接后替换全局 `KAGParser`，解除链接时恢复 |
| `layerExDraw.dll` | GDI+ 风格的矢量绘图，基于 blend2d |
| `fstat.dll`、`scriptsEx.dll`、`TextRender.dll`、`windowEx.dll` | 文件信息、脚本扩展、文字渲染、窗口扩展 |
| `getLangName.dll`、`layerExSave.dll`、`PackinOne.dll`、`win32ole.dll` | 按游戏脚本的调用方式编写的兼容实现 |
| `win32dialog.dll` | Win32 对话框模板的 GTK 实现，字体选择对话框会转到原生字体选择器 |
| `krkrsteam.dll` | Steam 接口占位实现，语言取自系统 locale，云存档不可用 |

## 不需要实现的插件

许多游戏在链接插件前会用 `CanLoadPlugin(name)` 检查对应的 DLL 是否随游戏一起分发，不存在就不会链接。`process`、`shellExecute`、`clipboardEx`、`gamepad`、`kztouch` 等通常不随游戏分发，因此不需要提供实现。只在 Windows 或 Steam 上有意义的插件（如 Direct3D 绘制设备、Flash 播放器）也属于这种情况，游戏会走自己的替代分支。

`psd.dll` 只在游戏直接读取 `.psd` 文件时才会被链接，使用 PIMG 的游戏由 `psbfile.dll` 处理，目前没有移植。`AlphaMovie` 是可选功能，也没有实现。

## 调试工具

**插件日志**：设置 `KR2_PLUGIN_LOG=debug`（可选 `trace`、`debug`、`info`、`warn`、`error`、`off`，默认 `warn`）可以输出移植插件的详细日志。

**脚本反汇编**：商业游戏的脚本通常是编译后的 TJS2 字节码。`tests/plugins` 中的 `tjs_disasm` 使用引擎自带的反汇编器输出可读的指令，比从字符串中猜测调用关系可靠得多。配合 [XP3 提取工具](../tools/README.md) 先导出脚本：

```sh
cmake -S tests/plugins -B build-plugin-tests -G Ninja
cmake --build build-plugin-tests --target tjs_disasm psb_harness
./build-plugin-tests/tjs_disasm path/to/system/Initialize.tjs
```

**PSB 查看**：`psb_harness` 可以解析 PSB 文件并按路径查询其中的值，例如：

```sh
./build-plugin-tests/psb_harness path/to/scenario.scn 'root.scenes.count'
```

## 添加新插件

新插件放在 `src/plugins/ext` 下即可，CMake 会自动收录该目录中的 `.cpp` 文件，以 C++17 编译为 `krkr2plugin_ext` 静态库，并以 `--whole-archive` 链接，保证 ncbind 的自注册代码不会被链接器丢弃。日志请使用 `kr2_plugin_log.h`，不要引入新的运行时依赖。建议同时在 `tests/plugins` 中添加通过真实 ncbind 注册路径运行的测试。
