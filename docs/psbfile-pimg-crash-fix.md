# psbfile.dll：PIMG 载入崩溃修复（2026-10-07）

## 现象

千恋＊万花（ゆずソフト）在 `UILoader.tjs を読み込みました` 之后立即 SIGSEGV，
systemd-coredump 只留下主程序内的一帧（AppImage 的 FUSE 挂载已卸载，无法回溯）。

## 原因

`system/psdlayer.tjs` 的 `loadPIMG` 使用新式调用（反汇编 TJS2 字节码确认）：

```
var psb = new PSBFile();
psb.load(storage);
layerFilename = "psb://" + Storages.extractStorageName(storage).toLowerCase()
              + "/" + layer_id + ".tlg";
```

`src/plugins/ext/psbfile/main.cpp` 的 `load()` 使用文件内 `static PSBMedia *psbMedia`，
该指针从未赋值（真正的 psb:// 媒体在 `PSBMediaRegistry.cpp`），
调用其虚函数 `NormalizeDomainName` 即空指针崩溃。上游 KrKr2 同样存在此问题。

## 修复

- `main.cpp`：删除未初始化指针；`load()` 与旧式 `new PSBFile(path)` 都经
  `PSB::registerRootResources` 注册资源，容器名同时登记“原样路径”和
  `TVPExtractStorageName` 得到的纯文件名；`self`/`objs` 判空。
- `PSBMedia`：资源名按 ASCII 不区分大小写；`<id>.tlg` 找不到时回退 `<id>`；
  不存在的资源 `Open` 抛 TJS 异常而不是插入空资源；`GetLocallyAccessibleName` 返回空。
- 回归：`tests/plugins/psbmedia_harness`，经真实 ncbind 注册执行
  `new PSBFile().load()`、旧式构造、psb:// 查找。用 `yuzulogo.mtn` 验证
  `yuzulogo.mtn/source/yuzu/icon/yuzu_logo/pixel` 读出 129895 字节（与解析器一致）。

## 同时修复

- `TVPGetLocallyAccessibleName("")` 抛 `Not supported media type ""`，导致
  `safeSaveStruct` → `saveSystemVariables失敗`。空名现在返回空；fstat
  `deleteFile`/`moveFile` 对空路径直接返回 false。

## 崩溃诊断

`platform/linux/LinuxCrashHandler.cpp`：SIGSEGV/SIGBUS/SIGFPE/SIGILL/SIGABRT 时在进程内
回溯，输出“模块+偏移”、主程序 `.symtab` 函数名，若主机有 `addr2line` 再给出源码行；
写入 stderr 与 `~/.local/share/kirikinux/crash.log`，随后恢复默认处理并重新触发信号，
core dump 照常生成。`KIRIKINUX_NO_CRASH_HANDLER=1` 关闭。引擎代码以 `-g1`
（`KR2_DEBUG_LINE_TABLES`，默认 ON）编译并带 `--build-id`。

## 未验证

本环境无法构建完整引擎；Logo 动画（motionplayer）的实际绘制和 PIMG 图层显示
尚未在真实游戏中验证。
