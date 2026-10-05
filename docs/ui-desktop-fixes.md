# 2026-10-05 桌面与启动修复记录

基线：Comet387/kirikinux2 `b6323df`。根目录构建完整原引擎，
不是 `linux/` 或 `linux-native/` 中的诊断宿主。

## 变更及原因

- **中文方框**：公开源码没有提供 UI 字体。加入未改字形和元数据的
  Noto Sans CJK SC Regular（SIL OFL），保留上游资源文件名；Linux 的
  Cocos 文字/按钮使用资源绝对路径，GTK 注册同一字体。构建和打包都使用
  Cocos 自带 FreeType 检查 CJK、日文、拉丁字形。AppImage 使用自含
  `fonts.conf`，读取包内和普通主机字体目录，避免旧库解析新版主机 conf.d；
  外部主机程序恢复原 `FONTCONFIG_FILE`，引擎使用包内配置。
- **全局设置退出**：资源中缺少 `default.cur`，旧加载器会继续解码空流。
  加入原创光标及可复现生成脚本；路径/长度/解码失败时跳过可选预览，
  不再把空光标传给 `addChild`。同时处理空渲染器字符串和空初始化文字。
  用户 coredump 未包含可用符号栈，不能断言这是该 coredump 的唯一原因。
- **设备信息太长**：GTK 弹窗约束初始尺寸，使用可滚动、可选取的文本视图。
  “复制全部”复制完整内容并保留弹窗；原有按钮返回值不变。
- **名称和关于**：产品显示为 Kirikinux2；项目和反馈地址为
  Comet387/kirikinux2，删除贴吧，中文中的 GitHub 两侧保留空格，补丁库 URL 不变。
  上游来源、版权、历史文件名和旧设置读取兼容保留。
- **打开游戏**：首页和菜单都有明确按钮，可选 XP3、startup.tjs 或游戏目录。
  目录优先本地 startup.tjs，否则挂载 data.xp3，或唯一 XP3。
  多个 XP3 且没有主入口时请直接选择主文件，避免误开资源/补丁包。
  原“新建文件夹”只创建磁盘目录，已替换为“打开游戏”。
- **启动缺成员**：附加日志显示 startup.tjs 访问缺失的 Motion 后，旧回退路径
  执行编译后的 initialize.tjs，在 VM ip 265 读取全局 debugWindowEnabled 失败。
  引擎在游戏和补丁运行前注册可写的 false 默认值，适用于原始文本和 TJS 字节码。
  游戏仍可覆盖；没有关闭其他缺成员异常。Linux 只提供 Motion 的显式探测兼容命名空间，D3D/E-mote 方法保持空操作并报告不可用，不伪造 HTTP 等插件能力。
- **目录补丁**：TVPGetAppPath 原来提前缓存了未带尾斜线目录的父路径，
  导致目录启动忽略游戏目录中的 patch.tjs。现在根据原本地选择区分目录和归档，
  同时让目录游戏的独立设置使用游戏目录。
- **文档和许可**：readme.md 改为 README.md，原 LICENSE 保留；新增代码
  AGPL-3.0-only 的范围与完整文本见 LICENSING.md / LICENSE-AGPL-3.0。

## Linux Android 插件兼容

Android 补丁常在 `startup.tjs` 第一步读取 `Motion.D3DAdaptor`，再把 `Motion.Player.useD3D`、`Motion.EmotePlayer.useD3D` 设为关闭并注册 E-mote 解密/相机回调。Linux 原先没有顶层 `Motion`，因此 `typeof Motion.D3DAdaptor` 本身就会抛出“Member \"Motion\" does not exist”。现在 Linux 在启动上下文建立时提供 `Motion.Player`、`Motion.EmotePlayer` 和 `Motion.ResourceManager`，并加入安全空操作方法；`Motion.D3DAdaptor` 仍未定义，所以补丁会走 Linux 的非 D3D 分支。这个兼容层解决启动崩溃，不声称实现 Android 专用动画渲染或游戏专用解密。

## 已执行验证

环境：Ubuntu 24.04 依赖、GCC 13、RelWithDebInfo、Cocos2d-x 3.17.2、
私有 FFmpeg 4.4.5；Xvfb / Mesa llvmpipe OpenGL 4.5。最终二进制和所带库
最高要求的 glibc 符号版本为 2.38，不能当作适用于所有旧发行版的构建。

| 项目 | 实测结果 |
| --- | --- |
| 完整引擎构建 | 成功；34 个 CSD 生成 CSB，字体和光标资源检查成功 |
| 原组件回归 | 3/3 通过；容器/分配器 ASan+UBSan，265 个 TVPGL 入口，GTK 无显示回退 |
| 中文全局设置 | 实际点击菜单进入，页面中文字可读，进程存活 |
| 缺少光标 | 临时移除 default.cur 后进入全局设置，仍存活；测试后恢复 |
| 关于与设备信息 | 实际点击，链接和中文正确，设备框高度受限 |
| GTK 行为 | 3000 行文本有滚动范围；完整剪贴板包含末尾；复制不关闭；按钮索引和取消正确；Unicode/空格文件选择通过 |
| 缺成员基线 | 原实现运行同名全局读取，出现 debugWindowEnabled does not exist |
| TJS 文本及字节码 | 编译后重新执行，默认值和可写性通过；其他缺成员错误仍正常抛出 |
| 补丁覆盖 | 游戏目录 patch.tjs 设 true，startup.tjs 读取 true，通过 |
| 编译初始化回退 | 启动脚本故意抛异常，回退到原生编译的 System/Initialize.tjs，通过并创建窗口 |
| GUI 文件启动 | 首页打开游戏 → 原生文件选择 → startup.tjs → 关闭首次提示；成功加载同目录补丁并创建游戏窗口，无游戏命令行参数 |
| GUI 目录启动 | 首页打开游戏 → 选择目录 → 只含 data.xp3 的目录；自动挂载归档并成功执行 Startup，无游戏命令行参数 |
| 真实 XP3 | 180 秒存活；中文/空格目录；Startup script ended、ch2004.ks 和剧情返回标记均命中；鼠标、Space、Return、规则转场通过 |

真实 XP3 使用原 CI 固定的外部样例，SHA-256：
`15f008b19fbd10d08002c8419670c0d7c98d03958047f2ded9dc3f1e8c58b02b`。
游戏资源不在源码包或 AppImage 中。原创启动回归脚本位于
`tests/linux/games/`；生成的字节码不纳入交付，可用引擎自己的
`Scripts.compileStorage` 重建。

## 验证边界

用户上传了日志和截图，没有上传对应 39045 文件的游戏归档。
因此本轮修复确认解决上述全局读取错误，不能声称该游戏已完整运行。
Motion/E-mote/D3D 专用能力仍取决于可用插件和游戏补丁，原补丁入口保留。
BPG、XP3 重打包和默认关闭的 JPEG XR 等原有未接入功能没有由本轮补齐。

此执行环境不提供 /proc/self/exe 和 FUSE，无法在此直接启动 AppImage
的 ELF/FUSE 入口；打包检查和解包后的 AppRun 使用另行记录的实测。
没有验证其他发行版、GPU 或桌面环境；不能将构建成功等同于全游戏兼容。

## 复现启动回归

```sh
python3 scripts/linux/gui-smoke.py --engine build-linux/bin/kirikinux2 \
  --game tests/linux/games/startup-compat --output startup-test --seconds 7 \
  --require-log KIRIKINUX_STARTUP_COMPAT_TEXT_AND_BYTECODE_OK
python3 scripts/linux/gui-smoke.py --engine build-linux/bin/kirikinux2 \
  --game tests/linux/games/startup-compat-override --output override-test --seconds 7 \
  --require-log KIRIKINUX_STARTUP_COMPAT_PATCH_OVERRIDE_OK
python3 scripts/linux/gui-smoke.py --engine build-linux/bin/kirikinux2 \
  --game tests/linux/games/startup-compat-fallback --output fallback-test --seconds 7 \
  --require-log KIRIKINUX_STARTUP_COMPAT_COMPILED_FALLBACK_OK
```

需要本机 Xvfb、libX11 和 libXtst；如果安装了 `glxinfo`，脚本会额外记录
OpenGL 探测结果，缺少它不会阻止 smoke 回归。这些命令仅用于开发回归，
用户启动游戏使用图形界面。

## 后续修复：检查点 1

用户新版日志中的 `kirikiriz` 是独立的运行时全局成员，已在补丁执行前
注册可覆盖的 `false` 默认值（本引擎为 krkr2 API）。原创编译初始化回退
已输出 `KIRIKINUX_STARTUP_COMPAT_COMPILED_FALLBACK_OK` 并正常创建窗口。
未取得用户游戏脚本，不能据此确认整部游戏可运行。

用户地址 `0x5089f4` 对应 `TVPListForm::initFromInfo` 的空列表项访问。
`ListItem.csb` 根节点是 Layer，原动态转换为 Widget 得到空指针；改为
Widget 包装。320×180 实际点击路径栏弹出三个目录，进程存活。

外部 URL 使用独立 argv 调用 `xdg-open`，恢复启动前的主机库/字体环境，
测试已确认原始 URL 含引号、空格和 shell 字符时仍作为单个参数传递。
Launcher 使用自含 Fontconfig 并隔离主机 GTK 模块。短弹窗使用紧凑可选标签，
长文本保留滚动和复制；停止向终端输出弹窗正文。滚轮、可拖动滚动条、
动态窗口尺寸及 XP3 脚本提取工具正在下一检查点实现。安装包仍为上一版。

## 后续修复：检查点 2

Linux 设置、文件列表、最近列表和菜单加入常驻可拖动滚动条，支持鼠标滚轮。
控件保持可读的物理像素尺寸，文件/设置内容为滚动条保留右侧空隙。
窗口设为可调整大小；resize 更新设计分辨率、场景、UI 和游戏显示区域，
游戏自身逻辑尺寸保留。320×180→800×500→600×400 与路径弹层存活。
真实 X11 文件列表与设置页的滚轮到底、拖动回顶已抓帧验证。

GTK 实际行为测试增加“确定退出吗？”尺寸小于 600×240，无滚动区/复制按钮，
原响应索引保持；3000 行设备信息的滚动、完整复制、取消/文件选择仍通过。
关于增加独立 GitHub 按钮。中文 XP3/OpenGL 等界面混排补齐空格。

新增本地 XP3 工具：浏览器选文件导出脚本 ZIP，Python 默认图形界面，
可全量流式解包。标准原始/zlib 索引、连续索引、双分段、UTF-16 文件名、
二进制字节码保留、路径穿越拒绝、校验失败报告及 Python/浏览器结果一致
三项测试通过。固定样例导出 48 个脚本/配置文件，不分发样例游戏内容。

最终复核：菜单在 320×180 与两次 resize 中保持可读且能滚动，进程存活；
原创文本与编译字节码启动通过。固定 XP3 在 320×180→800×500→600×400
运行 40 秒，启动结束、鼠标/Space/Return、窗口缩放后存活，未出现缺成员或
terminate 日志。3/3 组件测试通过。用户游戏本体尚未取得；若依赖 Motion/D3D
或自定义加密，仍需脚本诊断和对应兼容处理。

## 当前检查点的修正

本文件以上为此前版本的历史验证记录。新的启动回退策略、布局和提取工具
以 [当前检查点](startup-ui-xp3-checkpoint.md) 为准，不能沿用历史 AppImage 的
实测结果证明新版源码。故意抛异常后强行执行 Initialize 已取消。
