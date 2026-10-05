# XP3 提取工具

[浏览器版](https://kirikinux2-progress.lively-plum-4208.chatgpt.site/tools/xp3-tool.html)：
选择本地 XP3，点击 **导出脚本诊断包**。默认过滤图片、声音、视频，保留 TJS
字节码、`.scn` 和常见配置；不用删除资源或重新封包，原 XP3 不会被修改。
文件只在你的浏览器中处理，不上传服务器。

- `files/`：内容的 Adler-32 与归档记录一致。
- `unverified/`：校验不一致的诊断字节，**不代表已解密**。可能原因包括游戏
  自定义哈希、加密或数据损坏。默认保留，也可取消勾选。
- `kirikinux-diagnostic.json`：预期/实际校验、flags、异常内容前 32 字节及失败原因。

已取消固定 MiB 限制，不再按脚本大小默默丢弃文件。支持文件保存接口的浏览器
直接分段写入 ZIP64；其他浏览器生成内存 ZIP，较大的输出建议用 Python。

离线浏览器版：把 `xp3-tool.html` 和 `xp3-tool.js` 放在同一目录，打开 HTML。
Python 版无参数时打开图形界面，需要 Python 3.9+ 与 Tk；图形界面可选择脚本
或全部资源，以及是否保留校验异常字节。

```sh
python3 tools/xp3-extract.py data.xp3 --output scripts.zip
python3 tools/xp3-extract.py data.xp3 --all --output unpacked.zip
python3 tools/xp3-extract.py data.xp3 --output verified.zip --verified-only
```

标准原始/zlib 索引、连续索引和多分段文件均支持；已有输出不覆盖，异常路径
不会进入 ZIP。游戏专用索引或内容解密仍需对应实现/过滤器。本版没有声称用户
游戏的加密已破解；详细验证范围见 [检查点记录](../docs/startup-ui-xp3-checkpoint.md)。

本目录新增代码采用 AGPL-3.0-only。格式依据本仓库原 XP3Archive.cpp。
