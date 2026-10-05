# 启动兼容、桌面文件列表与 XP3 诊断检查点

日期：2026-10-05。基线是本次提供的 `Kirikinux2-source-7f32d89c.zip`，
增量补丁逐字节对照该包生成，不假定远端仓库 HEAD。

## 连续缺成员的共同原因

原 `TVPExecuteStartupScript` 先用文本流读取 `startup.tjs`，再调用原本
已经支持文本与字节码的 `TVPExecuteStorage`；它还捕获启动过程中的任何异常，
只要 `System/Initialize.tjs` 存在就继续执行。这会跳过游戏自己的启动上下文，
或带着未完成的初始化进入 KAGEX，掩盖首个插件/脚本异常。

本版直接使用原存储执行器加载启动入口。**只有启动文件不存在**时才保留
initializer-only 回退；文本解码、字节码、插件和脚本执行异常全部保留。
`patch.tjs` 失败停止启动，`AfterStartup.tjs` 的失败也不再被静默丢弃。
成功日志在启动和后置补丁都结束后才输出。

`StartupCompatibility.h` 集中管理以下上游补丁中同组出现的框架上下文。
在实际存储和过滤器准备好之后、游戏补丁之前初始化，已有值不被覆盖。

| 成员 | 默认值与依据 |
| --- | --- |
| `kirikiriz` | false；当前播放器使用 krkr2 API；预处理器原行为保留 |
| `debugWindowEnabled` | false；框架调试窗的默认状态，独立于 VM 的 `-debug` |
| `inXP3archivePacked` | 实际解析到的启动存储含归档分隔符时为 true，散文件入口为 false；入口缺失则使用已建立的项目存储 |
| `convertMode` | false；播放器启动模式，游戏/补丁仍能覆盖 |

这些是框架约定，不是 Windows 原引擎的四个原生 API。没有给未知名字统一返回
false。Linux 对 Android 补丁最常见的 Motion 探测提供显式兼容命名空间，但 `D3DAdaptor`
仍缺失，方法是空操作；没有伪造 D3D、HTTP 或其他插件能力。游戏完整执行自己的 startup
仍是初始化完整性的前提。现有原生类和插件实现继续使用项目原代码。

来源：
- [上游 RIDDLE JOKER 启动补丁](https://github.com/zeas2/Kirikiroid2_patch/blob/master/patch/%E3%82%86%E3%81%9A%E3%82%BD%E3%83%95%E3%83%88/RIDDLE%20JOKER%20Ver%201.14a/patch.tjs)
- [上游喫茶ステラ启动补丁](https://github.com/zeas2/Kirikiroid2_patch/blob/master/patch/%E3%82%86%E3%81%9A%E3%82%BD%E3%83%95%E3%83%88/%E5%96%AB%E8%8C%B6%E3%82%B9%E3%83%86%E3%83%A9%E3%81%A8%E6%AD%BB%E7%A5%9E%E3%81%AE%E8%9D%B6/patch.tjs)

这些来源证明上下文约定，不用于判定用户提供的游戏是哪一款，也没有把完整
游戏专用补丁作为所有游戏的通用能力。

## Android 插件探测兼容

Linux 原先在 `startup.tjs` 读取 `Motion.D3DAdaptor` 时连顶层 `Motion` 都不存在，TJS 会在 `typeof` 求值前抛出成员异常。启动上下文现在建立 `Motion.Player`、`Motion.EmotePlayer` 和 `Motion.ResourceManager`，提供关闭 D3D 所需的字段与空操作回调；`Motion.D3DAdaptor` 保持未定义，让脚本明确走非 D3D 分支。真正的 E-mote 动画渲染、PSB 解密和游戏专用插件仍需对应 Linux 实现或补丁，不能由这个启动兼容层冒充。

## 文件浏览器

Linux 目录行改为直接构造的 Cocos 控件：固定行高，名字单行显示，按字体实际
测量宽度保留前缀和 `...`；目录 `>` 和选择框使用同一右侧列。每次复用和 resize
都重新计算宽度，滚动条保留独立空隙，行内容和滚动视口启用裁剪。

显示层替换控制字符和无效 UTF-8，截断只发生在 Unicode 码点边界；磁盘路径、
回调和打开操作始终使用完整原名。顶部路径和快捷目录列表采用同一测量规则。

最近游戏使用固定名称行、完整路径摘要行与独立操作行，移除按文字长度扩展的
横向滚动容器；所有列随当前列表宽度更新，首页打开游戏入口也跟随 resize。
回收行会取消未完成的长按回调，点击/长按入口拒绝已失效的文件索引。

游戏架构、Cocos UI 框架和渲染器保留。

## XP3 工具

去掉单脚本 32 MiB、脚本包 128 MiB、全量包 512 MiB 的人为上限。
Python 按分段流式处理，输出 ZIP64；支持文件保存接口的浏览器逐块读取、先
校验后直接写入 ZIP64，不把每个资源或全部输出放入一个大数组。
其他浏览器仍有内存 ZIP 的实际内存约束，建议大输出使用 Python。

Adler-32 不一致不能证明密文。本版将预期/实际校验值、flags 和前 32 字节
写入诊断报告，可将该内容保留到独立的 `unverified/`；它不被放入正常的
`files/`，也不标记为已解密。可选择只导出校验一致的内容。
原 XP3 不修改；路径穿越拒绝，字节码原样保留，默认仍过滤图片/声音/视频。
游戏专用过滤器解密没有在缺少样本的情况下凭空实现。

## 本检查点实际验证

- 用项目原 TJS2 VM 编译并执行启动字节码，验证统一上下文、散文件/归档状态、
  游戏覆盖值、缺入口回退、首个异常不被吞掉、其他缺成员仍正常抛出。
- XP3 原始/zlib 连续索引、分段、字节码保留、异常路径、校验异常诊断保留、
  仅校验一致模式、浏览器 Blob 与直接写文件 ZIP64 及超过旧 32 MiB 门槛的大脚本共六项回归通过。
- 文件名 UTF-8、控制字符、无效字节和极窄宽度的截断检查通过。

```sh
python3 scripts/linux/check-startup.py
python3 tests/linux/xp3_tool_regressions.py
```

这不是完整引擎和用户游戏的运行证明。当前环境没有 Cocos2d-x / 私有 FFmpeg
构建依赖，本版完整引擎和新 AppImage **尚未构建**，新 Cocos 布局 **尚未实机验证**。
用户 `data.xp3` 的下载页面显示 2.15 GB，下载时提示分享流量不足、需要购买流量，
因此真实游戏样本 **尚未取得**，加密方式和具体插件需求仍未验证。

上一轮记录中的故意抛异常再进入 Initialize 的测试只适用于旧回退策略；本版
应保留原异常，不能再以那个测试的旧成功标记作为合格条件。
