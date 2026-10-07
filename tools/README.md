# XP3 提取工具

这里有两个功能相同的小工具，用来从 XP3 封包中导出脚本或解包全部资源。遇到游戏启动报错时，可以先用它导出脚本诊断包，再附在问题反馈中。工具只读取原 XP3，不会修改它，也不需要重新封包。

## 浏览器版

把 `xp3-tool.html` 和 `xp3-tool.js` 放在同一目录，用浏览器打开 HTML，选择本地 XP3 后点击 **导出脚本诊断包** 或 **解包全部资源**。文件只在本地浏览器中处理，不会上传到任何服务器。

支持文件保存接口的浏览器（如 Chromium 系）会分段读取并直接写入 ZIP64，文件大小不受限制；其他浏览器会在内存中生成 ZIP，输出较大时建议改用 Python 版。

## Python 版

需要 Python 3.9 或更新版本。不带参数运行时会打开 Tk 图形界面，可以选择只导出脚本还是全部资源，以及是否保留校验异常的数据。

```sh
python3 tools/xp3-extract.py data.xp3 --output scripts.zip
python3 tools/xp3-extract.py data.xp3 --all --output unpacked.zip
python3 tools/xp3-extract.py data.xp3 --output verified.zip --verified-only
```

## 输出内容

脚本模式默认跳过图片、声音和视频，保留 TJS 字节码、`.scn` 和常见配置文件。生成的 ZIP 包含：

- `files/`：Adler-32 校验与封包记录一致的文件。
- `unverified/`：校验不一致的原始字节，仅供诊断，**不代表已经解密**。不一致可能来自游戏自定义的哈希、加密或数据损坏。默认保留，可以取消勾选或使用 `--verified-only`。
- `kirikinux-diagnostic.json`：每个异常文件的预期与实际校验值、flags、前 32 字节以及失败原因。

工具支持标准的原始/zlib 索引、连续索引和多分段文件。已存在的输出文件不会被覆盖，包含路径穿越的条目会被拒绝。使用游戏专用加密的封包需要对应的解密实现，本工具不处理。

格式实现参照本仓库的 `src/core/base/XP3Archive.cpp`。本目录代码采用 AGPL-3.0-only。
