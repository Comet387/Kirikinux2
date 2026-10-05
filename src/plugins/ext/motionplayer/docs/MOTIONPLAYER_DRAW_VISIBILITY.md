# MotionPlayer 立绘不可见问题排查

> **关联代码：** `EmoteDrawDebug.h` / `EmoteDrawDebug.cpp`  
> **架构说明：** [`MOTIONPLAYER_ARCHITECTURE.md`](MOTIONPLAYER_ARCHITECTURE.md)  
> **文档索引：** [`README.md`](README.md)  
> **API：** [`MOTIONPLAYER_API_GUIDE.md`](MOTIONPLAYER_API_GUIDE.md)  
> **位置/仿射偏差（TVP vs OpenGL）：** [`MOTIONPLAYER_TVP_COORDINATES.md`](MOTIONPLAYER_TVP_COORDINATES.md)  
> **编译 / 启动 / 日志 / 单测：** [`MOTIONPLAYER_RENDER_TEST.md`](MOTIONPLAYER_RENDER_TEST.md)

**状态（2026-06-08）：** TVP **路线 A** — `_limitArea`=Layer 尺寸、tess 根 `_affineTrans*model`、离屏=Layer、`compositeBitmapToLayer` identity 1:1（对齐 SDL3 FBO 路径）。`EMOTE_DRAW_DEBUG_CENTER=1` 直绘 Layer 跳过 composite。

---

## 1. 现象分层

| 现象 | 说明 |
|------|------|
| **红块/调试矩形可见** | `draw()` → Layer `MainImage` → 屏幕合成正常 |
| **立绘不可见** | 问题在 Emote **离屏位图内容**、**贴回 Layer 后仍透明**（含 `hda` 误用 §8）、或 **在屏外** |
| **日志无 error** | `readIconTobuffer`、`_currmotion`、`progress` 均正常时，常见原因为 **静默裁剪** 或 **目标 alpha 未写入**（无 SDL_Log） |

绘制路径：

```
progress()  →  emotenode 构建 renderMethod（矩阵 + opa）
draw()      →  offscreen Fill(0)
            →  drawToBitmap(offscreen, _limitArea)   // 固定 screenSize
            →  compositeBitmapToLayer(layer)
```

红块若画在 **Layer 上**（绕过离屏），则会出现「有红块、无人」。

---

## 2. 已知的「无日志」原因

### 2.1 progress / draw 的 lim 不一致（高优先级）

| 阶段 | `lim` 来源 |
|------|------------|
| `progress` | 沿树传递；`blank/1830:1800:…` 等可使子节点 `lim` 远大于 `screenSize` |
| `drawToBitmap` | 始终使用 `_limitArea`（PSB `screenSize`，如 800×1080） |

建树时 `glm::ortho` 可能按 **大 lim** 写入 `renderMethod.matTrans`；绘制时 `clipToLayerPoint` 按 **drawLim** 映射像素。三角形可能落在离屏外，`EmoteTVPRenderer` 中 `triClip` 为空时 **直接 return，不打日志**。

**调试日志：** `EmoteDrawDbg summary` 中 `limMismatch=1`、`progressLimMax` > `drawLim`。

### 2.2 贴片被 clip / 非有限坐标跳过

`EmoteTVPRenderer::drawPatchToBitmap` 在以下情况 return（开启调试后会记入 `patchSkip` 并可能打一条 `patch clip empty`）：

- `opa <= 0` → `patchSkip[opa]`
- 三角形非有限 → `patchSkip[nonFinite]`
- 与 `targetClip` 无交 → `patchSkip[emptyClip]`
- `renderMethod.size() > 64` → `patchSkip[surf]` + 原有 `render failed` 日志

### 2.3 `renderMethod` 为空（未 progress 或 checkDrawStatus 失败）

`drawToBitmap` 要求 `renderMethod.size() >= 1`。若本帧未 `progress`，或节点 `isNeedDraw == false`，记入 `skip[noMethod]` / `skip[needDraw]`。

### 2.4 父层 `type == 2` 的 demux 分支无 projection

`EmoteNode.cpp` 在上一栈项为 `type == 2` 时使用 `demuxMat * model`，**不**再乘 `projection`。与 `lim` 不匹配时矩阵可能错误，三角形飞出视口（仍可能无 error 日志）。

### 2.5 透明度连乘为 0

`drawPatchCommon` 中 `totalOpa` 为整条 `renderMethod` 的 `opa` 连乘。任一层接近 0 则 `patchSkip[opa]`。

### 2.6 脚本仿射 / 缩放

`AffineSourceMotion.tjs` 每帧：

- `setScale(_emotescale * … * (100/resolutionx))` — `resolutionx` 过大 → 缩到不可见  
- `setDrawAffineTranslateMatrix(a,c,b,d,tx,ty)` — 2×2 行列式 ≈ 0 → 网格退化  

**调试日志：** `playerScale`、`affineDet2x2`；`HINT` 在 det/scale 异常时打印。

**位置「差很多」：** 优先查 [`MOTIONPLAYER_TVP_COORDINATES.md`](MOTIONPLAYER_TVP_COORDINATES.md) — TVP 上常见为 **`_affineTrans` 误乘入 `_renderMethod`**（Layer 坐标与 screenSize ortho 混用，`tri` 飞出数千像素），或 composite 误用 SDL3 负号矩阵。修复后：tess 仅 `model`，composite 用 t2DAffine 形式 `_affineTrans`。

### 2.7 仅 layout / motion 节点入栈，无 icon 绘制

`drawToBitmap` 只绘制 `isIcon == true` 的叶子；`motion/…` 引用走子 `emotemotion`。若子树未展开到 icon，计数上 `iconTry=0`。

### 2.8 其它（解析层已排除后）

- `transformOrder` 未实现 → 姿态错误，不一定出屏  
- `zcc`/`ccc` 曲线未参与插值 → 变形错误，少见整人消失  
- Stencil 依赖 `maskLayer`，当前 `drawWithTVP` 传 `nullptr`，个别资源可能异常  

---

## 3. 调试日志用法

### 3.1 开关

默认 **开启**（`EmoteDrawDebug.h`）：

```cpp
#ifndef EMOTE_DRAW_DEBUG
#define EMOTE_DRAW_DEBUG 1
#endif
```

编译前改为 `EMOTE_DRAW_DEBUG=0` 可关闭。或在 CMake 中加：

```cmake
target_compile_definitions(motionplayer PRIVATE EMOTE_DRAW_DEBUG=0)
```

**居中贴片（矩阵隔离，默认关）：** `EMOTE_DRAW_DEBUG_CENTER=1`（须同时 `EMOTE_DRAW_DEBUG=1`）。开启后 `drawWithTVP` **直绘 Layer `MainImage`**（跳过离屏 + composite），在 Layer 中心网格排 icon（与人物矩阵无关）；顶部红条探针仍绘制。**须对 `AffineBlt` 使用 `triClip`**。仅排查用。开启：

```cmake
target_compile_definitions(motionplayer PRIVATE EMOTE_DRAW_DEBUG_CENTER=0)
```

**顶部红条探针：** CENTER 模式下在屏幕顶部中央画不透明 `Fill(0xFFFF0000)` 矩形。若红条可见、icon 不可见 → 贴图或 `readIconTobuffer`；若红条也不可见 → Layer 未参与屏幕合成。

**目标 alpha（2026-06-01）：** 离屏/MainImage 先 `Fill(0)` 全透明后，`AffineBlt` 须 **`hda=false`**（与 `tTJSNI_BaseLayer::AffineCopy` 在 `dfAlpha` 下一致）。`hda=true` 且 `opa=255` 走 `TVPCopyColor`，**保留目标 alpha**，透明底上贴图后仍全透明。日志 `iconDrawn>0`、`texAlpha~>0` 但 `alphaSamples~` 很少时优先查此项。

**纯色块（无贴图）：** `EMOTE_DRAW_DEBUG_SOLID=1` 时用 `Fill` 画彩色方块代替 `AffineBlt`，验证 blit 以外路径。

### 3.2 每帧输出（`Emote draw` 一次）

标签 **`EmoteDrawDbg summary`**，字段含义：

| 字段 | 含义 |
|------|------|
| `offscreen` / `layer` | 离屏与 Layer 尺寸 |
| `drawLim` | `drawToBitmap` 使用的 `_limitArea` |
| `progressLimMax` | 本帧 `progress` 见到的最大 lim |
| `drawViewport` / `drawVpActive` | 本帧记录的 drawLim（≈ screenSize）；**仅** `clipToLayerPoint` 参考；icon `ortho` 用 progress `lim` |
| `limMismatch` | progress lim 大于 draw lim |
| `sampleIcon` `src` / `triBBox` / `scale` | 源 icon 像素 vs 目标三角包围盒；`scale` 远小于 1 为矩阵缩放塌缩 |
| `worstScaleIcon` | 本帧 `min(scaleX,scaleY)` 最小的 icon |
| `alphaSamples~` | 离屏上非透明采样数（`iTVPBaseBitmap::GetScanLine` 步进扫描，近似） |
| `playerScale` | `currZx` / `currZy` |
| `affineDet2x2` | 脚本仿射 2×2 行列式 |
| `iconTry` / `iconDrawn` | 尝试贴片次数 / 实际进入 blit 次数 |
| `skip[...]` | 节点级跳过统计 |
| `patchSkip[...]` | 贴片级跳过统计 |

**`EmoteDrawDbg sampleIcon[...]`**：本帧 **`texAlpha~` 最大** 的 icon 样本（非遍历顺序第一个）的 `totalOpa`、三角形顶点、`dataAlpha~` / `texAlpha~`、`colorType`、`inOffscreen`。

**`EmoteDrawDbg HINT`**：根据统计给出的简短结论。

### 3.3 如何读结果

| 观察 | 可能结论 |
|------|----------|
| `alphaSamples~=0` 且 `iconTry>0` | 三角在屏外、opa=0、lim 不一致，或 **`hda=true` 保留透明底 alpha**（§8） |
| `texAlpha~>0` 且 `alphaSamples~` 很少 | **优先查 `AffineBlt` 的 `hda=false`**（§8） |
| `limMismatch=1` | 优先查 §2.1 |
| `patchSkip[clip]` 很高 | 坐标/矩阵问题 |
| `patchSkip[opa]` 很高 | 检查帧 opa 与 renderMethod 连乘 |
| `skip[noMethod]` 很高 | 未调 `progress` 或顺序错误 |
| `iconTry=0` | 树未走到 icon（play/chara/motion 或子 motion 路径） |
| `affineDet2x2≈0` 或 `playerScale` 极小 | 查脚本 AffineSourceMotion |

---

## 4. 实机日志样例（2026-06-01）

典型「有人尝试画、离屏仍空」（**2026-06-08 前**：Layer 仿射误入 tess）：

```
EmoteDrawDbg summary: offscreen=800x1080 layer=1280x720 drawLim=800x1080 limMismatch=1
  alphaSamples~=0 iconTry=31 iconDrawn=31
EmoteDrawDbg sampleIcon[■後髪]: surf=8 totalOpa=1.0
  tri=(-9311.7,8318.4)(-9212.9,8305.2)(-9312.1,8200.1) inOffscreen=0
```

| 线索 | 解读 |
|------|------|
| `surf=14` | `renderMethod` 约 14 层；`evaluatePatchPosition` 若叠乘全栈会把顶点甩出 800×1080 |
| `tri` 大量负坐标或 `x>800` | 矩阵重复变换或 ortho 按子树大 `lim` 建树 |
| `totalOpa=1` 仍 `alphaSamples~=0` | 多半在离屏外被 clip；或 GPU 离屏 `GetScanLine` 读不到 |
| `progressLimMax=0`（旧版） | `draw` 开始时 `reset()` 抹掉同帧 `progress` 统计；已改为跨 `progress`→`draw` 保留 |

**与 `docs/sdl3/emotefile.cpp` 对齐的修复：**

1. **progress**：`glm::ortho(-lim.originX, lim.width - lim.originX, …)`，用当前 progress 的 `lim`，**不要** `limToScreen` / `screenLim` 乘到矩阵上。  
2. **demux（父 type==2）**：`emt.matTrans = demuxMat * model`，**不加** `projection`（SDL3 第 1696 行）。  
3. **draw**：全栈 `surfaceCount` + `evaluatePatchPosition` 层间 UV 交换，**不要**折叠成 `surf=1`。  
4. **clipToLayerPoint**：`lim` = 离屏视口 `_limitArea`（800×1080），对应 SDL3 `glViewport(0,0,lim.width,lim.height)`。  
5. **drawPatchCommon 矩阵顺序**（TVP，2026-06-01 修正）：`patchTransforms[i]=renderMethod[surfaceCount-1-i]`，与 SDL3 逆序一致。

6. **projection**（2026-06-08）：icon 须 `ortho(progress lim)`，**勿** `projectionLimForDraw` 强制 `ortho(screenSize)`；clip 仍用 `_limitArea`。后者会使 layout 世界坐标与投影尺子不一致，`triBBox << src` 且 `tri` 飞出 ±5000。

`limMismatch=1` 在 SDL3 下也正常；NDC 与 `clipToLayerPoint(drawLim)` 独立，与 progress `lim` 可不同。

**错误尝试（已回退）：** `limToScreen` 叠在矩阵上；`projectionLimForDraw` 强制 drawViewport 作 ortho。

复测期望：`tri` 进入 `[0,800)×[0,1080)`，`alphaSamples~` > 0；`surf` 可能 >1（与 SDL3 相同）。

---

## 5. 建议修复方向（其它仍不可见场景）

1. **脚本隔离：** `setScale(1)`、固定 `setCoord`、暂时禁用 `setDrawAffineTranslateMatrix`。  
2. **矩阵 / lim：** §2.1、`drawPatchCommon` 正序、`progress` ortho 与 SDL3 一致（§4）。  
3. **合成路径：** `alphaSamples~>0` 后查 `compositeBitmapToLayer`、Layer 透明度。  
4. **TODO API：** `setTimelineBlendRatio` / `startWind` 未实现，影响表情/风，一般不是整人消失主因。

---

## 6. 相关文件

| 文件 | 作用 |
|------|------|
| `emoteplayerclass.cpp` | `drawWithTVP`、离屏、`compositeBitmapToLayer`、调试帧首尾 |
| `EmoteNode.cpp` | `progress` / `drawPatchCommon` / 节点 skip 统计 |
| `EmoteTVPRenderer.cpp` | 三角形 clip、opa、sampleIcon |
| `EmoteDrawDebug.cpp` | 汇总日志与 HINT |
| `data/system/AffineSourceMotion.tjs` | `setScale` / 仿射 / `progress`+`draw` 调用顺序 |

---

## 7. 维护

- 问题类型或日志字段变化时，同步更新本文与 `MOTIONPLAYER_ARCHITECTURE.md` §8。  
- 生产构建建议：`EMOTE_DRAW_DEBUG=0`、`EMOTE_DRAW_DEBUG_CENTER=0`。

---

## 8. 结案总结（NEKOPARA macOS，2026-06-01）

### 8.1 最终根因（一行代码语义）

`EmoteTVPRenderer` / `compositeBitmapToLayer` 在透明底（`Fill(0)`）上调用 `AffineBlt` 时使用了 **`hda=true`**。

TVP 中 `hda` = **hold destination alpha**（保留目标 alpha）。`bmCopy` + `opa=255` + `hda=true` 走 `TVPCopyColor`，只拷贝 RGB，**不覆盖目标 alpha**（`cpp/core/visual/gl/blend_functor_c.h` 的 `color_copy_functor`：`d & 0xff000000` + `s & 0x00ffffff`）。

因此：解码与 GPU 贴图均正常，仿射也执行了，但 MainImage/离屏在 `Fill(0)` 后 alpha 仍为 0，Layer 合成时整层仍透明。

**正确写法**（与引擎 Layer 一致）：

```cpp
// tTJSNI_BaseLayer::AffineCopy，DrawFace == dfAlpha
MainImage->AffineBlt(..., bmCopy, 255, &updaterect, false, ...);
```

修复：所有 Emote 路径 `AffineBlt` 及缩放合成改为 **`hda=false`**。

**透明叠盖（2026-06-01）：** icon 默认须 **`bmAlpha` + `hda=false`**（按源像素 alpha 合成）。勿把 `bmAlpha` 改成 `bmCopy`：`ConstAlphaBlend` 只用恒定 `opa`，透明区 RGB 仍会盖住下层。SDL 用 `GL_BLEND`+深度缓冲，TVP 靠 **Z 排序 + `bmAlpha`** 近似。

### 8.2 决定性日志（修复前）

CENTER 模式 + 红条探针下：

```
sampleIcon[■髪揺れ_後髪]: dataAlpha~=1298 texAlpha~=1298
summary: iconTry=26 iconDrawn=26 alphaSamples~=40
```

| 指标 | 含义 |
|------|------|
| `dataAlpha~` / `texAlpha~` > 0 | `readIconTobuffer` + `uploadSourceTextureFromData` **无问题**（非 psbfile / seed） |
| `iconDrawn` > 0 | `AffineBlt` **被调用** |
| `alphaSamples~` ≈ 40 | 帧缓冲上几乎仍透明（红条 + 极少噪点），指向 **blit 未写目标 alpha** |

改 `hda=false` 后，CENTER 网格立绘与 `alphaSamples~` 均正常。

### 8.3 排查过程中仍有效的改动（非根因，但应对齐 SDL3）

| 改动 | 作用 |
|------|------|
| `patchTransforms[i]=renderMethod[n-1-i]` | 与 SDL3 uniform 逆序一致，三角落在离屏内 |
| 单 icon `AffineBlt` 用 **triClip** 非整层 `targetClip` | 避免每 icon 全屏 `warpPerspective` 卡死 |
| `progress` 内 `readIconTobuffer` 后 **立即** `uploadSourceTextureFromData` | 与 SDL3 `glTexImage2D` 时机一致 |
| macOS `colorType==0` **BGRA 直传** `Update(data)` | 与 SDL3 Apple `GL_BGRA` 一致 |
| `EMOTE_DRAW_DEBUG_CENTER` + 红条 + `dataAlpha~`/`texAlpha~` | **隔离矩阵**，证明贴图链路 OK，把怀疑从 PSB 转到 TVP blit |

**明确未改 / 不应改：** `psbfile`、默认 decrypt seed 重试、调色板 `palbuff + coloridx * 4`（保持与 SDL3 `palbuff + coloridx` 一致）。

### 8.4 弯路与误判（避免重演）

| 误判 | 事实 |
|------|------|
| 「`hda=true` 才能写入 alpha」 | **相反**；`hda=true` 在透明底上会 **保留** alpha=0 |
| 「`else` 分支从未进 → 上传路径错了」 | `colorType` 恒为 0，应走 BGRA 分支；`else` 是 RGBA |
| 「`dataAlpha~=0` → 解码失败」 | 曾按遍历 **第一个** icon 采样；该节点当帧可为空，与全局解码无关 |
| 长时间怀疑 psbfile / seed | 用户约束下已排除；`texAlpha~>0` 后应 **立即** 查 TVP `AffineBlt` 参数 |

根因本质是 **未对照 `LayerIntf.cpp` 的 `AffineCopy(dfAlpha)`**，而不是 PSB 或矩阵 alone。调试框架的价值在于把问题收敛到「贴图有、屏上无」；最后一跳应对照 TVP 文档/引擎既有 Layer 路径。

### 8.5 验证通过后的开关

```cmake
target_compile_definitions(motionplayer PRIVATE
    EMOTE_DRAW_DEBUG=0
    EMOTE_DRAW_DEBUG_CENTER=0
)
```

恢复 **`drawWithTVP` 离屏 + `compositeBitmapToLayer`** 正常人物坐标；若位置仍偏，再查 §2.1 lim / 脚本仿射，与本次透明底 bug 无关。

### 8.6 Bezier 网格与 TVP 软件渲染（卡死 / OpenCV）

**栈：** `meshCopyToBitmap` → `AffineBlt` → `OperateTriangles` → `cv::warpPerspective`（`RenderManager.cpp` ~3700）。

**原因：**

- SDL3 用 **GPU tessellation** 一次绘制；TVP 路径对每个 mesh cell 做 **2 次** `AffineBlt`，非轴对齐四边形走 **OpenCV 透视**。一帧数十 icon × 每 icon 数十 cell → **数千次 warp**，macOS 上表现为卡死或 NEON 内崩溃。
- 与 §8.6 旧述 `triClip`/`destrect` 不一致、退化三角等问题叠加时更易崩。

**当前策略（`EmoteTVPRenderer.cpp`）：**

- 默认 **`kEnableCpuBezierMesh = false`**：有 `controlPts` 的变形仍经 **`evaluatePatchPosition` 全栈** 算三角顶点，但只打 **1 次** `affineTargetTriangle` 仿射（与无 mesh 的 icon 相同次数级）。
- 内部细分网格代码保留，仅当显式改 `kEnableCpuBezierMesh` 为 `true` 时启用（调试用，生产勿开）。

**代价：** 强弯曲部位可能略逊于 SDL3 网格；换稳定与可玩性。若将来接 GPU 或 `InternalAffineBlt` 批量路径，可再开 mesh。
