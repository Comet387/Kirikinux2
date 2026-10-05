# Kirikinux2 的许可范围

Kirikinux2 复用的代码继续按各自原协议授权。根目录 `LICENSE` 保持原内容，
源文件内的上游版权和许可声明继续有效；本项目不将第三方代码追溯改为 AGPLv3。

Kirikinux2 新增代码采用 GNU Affero General Public License version 3 only
（SPDX: AGPL-3.0-only），完整文本见 `LICENSE-AGPL-3.0`。
本轮原创新增文件包括 `tools/` 中的 XP3 提取工具、
`platform/linux/LinuxHost.cpp`、`src/core/environ/ui/DesktopScroll.*`、
 `src/core/base/StartupCompatibility.h`、
`src/core/environ/ui/FileNameLayout.h`、`DesktopFileLayout.h`、
`scripts/linux/check-startup.py`、`scripts/linux/generate-cursor.py`、
`platform/linux/tools/kr2_check_resources.cpp`、新增 UI 回归测试和其生成的
`cocos/kr2/Resources/default.cur`、`fonts.conf` 和 `tests/linux/games/` 的
原创启动回归脚本。现有文件中的 Kirikinux2 新增实现，
仅其新增版权部分依此授权；复用内容保留原许可及义务。

构建脚本下载的 Cocos2d-x、FFmpeg、7-Zip、UnRAR 与其他库各有原许可。
打包脚本收集其许可文件和发行版版权声明，不能以新增代码许可覆盖它们。

`cocos/kr2/Resources/DroidSansFallback.ttf` 实际为未修改字形和元数据的
Noto Sans CJK SC Regular OTF，以该资源名保存以兼容上游 UI。
其原作者、SIL OFL 1.1 与保留字体名称声明见同目录的
`licenses/NotoSansCJK-OFL.txt`。字体不是 AGPLv3 代码。

历史名称、上游项目 URL、原补丁 URL 和旧设置文件名只为来源声明或兼容保留，
不作为 Kirikinux2 当前产品名称。
