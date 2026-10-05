# PSB / e-mote 数据结构说明

> **文档索引：** [`README.md`](README.md)  
> **位姿实现：** [`MOTIONPLAYER_TEXTURE_WORLD_COORDS.md`](MOTIONPLAYER_TEXTURE_WORLD_COORDS.md)  
> **架构概览：** [`MOTIONPLAYER_ARCHITECTURE.md`](MOTIONPLAYER_ARCHITECTURE.md) §5

> 文件：`tests/test_files/emote/e-mote3.0バニラパジャマa.json`  
> 依据：
> - JSON 实例本身（该文件）
> - 当前解析实现：`cpp/plugins/motionplayer/EmoteFileCore.cpp`、`EmoteMetadata.cpp`、`EmoteMotion.cpp`、`EmoteNode.cpp`、`EmoteFrame.cpp`、`EmoteTimeline.cpp`、`EmotePhysics.cpp`
>
> 说明规则：
> - `✅`：代码中明确读取/使用，含义较确定
> - `⚠️`：可推断但不完全确定
> - 作用留空：当前无法确定（或当前实现未使用）

---

## 1. 根节点字段

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `id` | string |  | 未见当前 C++ 解析使用 |
| `label` | string |  | 未见当前 C++ 解析使用 |
| `metadata` | object | 角色/控制器/变量/导入信息总表 | ✅ 由 `emotemetadata` 解析 |
| `object` | object | 角色对象集合（每个对象含 motion 树） | ✅ |
| `screenSize` | object | 绘制逻辑画布大小与原点 | ✅ |
| `source` | object | 图层贴图资源定义（icon/纹理） | ✅ |
| `spec` | string | 资源规格：`krkr` / `win` / `common` | ✅ 用于决定解析分支 |
| `stereovisionProfile` | object | 立体视觉参数 | ✅ 读取但当前渲染路径几乎未使用 |
| `version` | number |  | 未见当前 C++ 解析使用 |

---

## 2. `screenSize`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `originX` | int | 逻辑坐标原点 X | ✅ |
| `originY` | int | 逻辑坐标原点 Y | ✅ |
| `width` | int | 逻辑画布宽 | ✅ |
| `height` | int | 逻辑画布高 | ✅ |

---

## 3. `stereovisionProfile`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `dist_e2d` | number |  | 已读取，当前路径用途不明确 |
| `dist_eye` | number |  | 已读取，当前路径用途不明确 |
| `eye_angle_ltd` | number |  | 已读取，当前路径用途不明确 |
| `f_level` | number |  | 已读取，当前路径用途不明确 |
| `fov` | number |  | 已读取，当前路径用途不明确 |
| `len_disp` | number |  | 已读取，当前路径用途不明确 |

---

## 4. `metadata`（顶层）

### 4.1 已被当前实现直接读取的字段

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `base` | object | 默认角色与默认动作 | ✅ |
| `base.chara` | string | 默认角色对象名 | ✅ |
| `base.motion` | string | 默认动作名 | ✅ |
| `mirror` | int | 是否镜像开关（`1` 为镜像） | ✅ |
| `timelineControl` | array | 时间线变量控制集合 | ✅ |
| `variableList` | array | 可控变量定义（至少标签） | ✅ |
| `eyeControl` | array | 眨眼控制参数 | ✅ |
| `eyebrowControl` | array | 眉毛控制参数 | ✅ |
| `bustControl` | array | 胸部物理控制参数 | ✅ |
| `hairControl` | array | 头发物理控制参数 | ✅ |
| `partsControl` | array | 其他部件物理控制参数 | ✅ |
| `selectorControl` | array | 选项切换控制 | ✅（本样例为空） |
| `attrcomp` | array | 属性移除规则 | ✅（字段可能不存在） |

### 4.2 该样例中存在但当前实现未直接消费/不完整消费

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `catalog` | array | ⚠️ 变量预设（样例里是变量赋值脚本串） | 推断 |
| `clampControl` | array | ⚠️ 变量夹紧/限制规则（`min/max`） | 推断；当前代码未直接执行 |
| `customPartsOrder` | array |  | 未见当前代码使用 |
| `editLock` | int |  | 未见当前代码使用 |
| `format` | string |  | 未见当前代码使用 |
| `instantVariableList` | array | ⚠️ 即时变量列表 | 推断 |
| `loopControl` | array |  | 本样例为空，未见当前代码使用 |
| `mirrorControl` | object | ⚠️ 镜像变量匹配规则（`variableMatchList`） | 代码有注释，未真正应用 |
| `mouthControl` | array | ⚠️ 口型/说话控制（`talkLabel`） | 推断；当前解析未接入 |
| `psdGlobalImportInfo` | object | PSD 导入元信息 | 未见当前代码使用 |
| `psdLayoutImportInfoMap` | object | PSD 图层布局导入映射 | 未见当前代码使用 |
| `psdScrapbookImportInfoMap` | object | PSD 贴纸/部件导入映射 | 未见当前代码使用 |
| `psdTextureImportInfoMap` | object | PSD 纹理导入映射 | 未见当前代码使用 |
| `scale` | int |  | 未见当前代码使用 |
| `stereovisionControl` | object | ⚠️ 双目变量匹配规则 | 推断；当前解析未接入 |
| `transitionControl` | array | ⚠️ 过渡开关列表 | 推断；当前解析未接入 |
| `version` | int |  | 未见当前代码使用 |

---

## 5. `metadata` 子结构字段

### 5.1 `timelineControl[]`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 时间线名 | ✅ |
| `lastTime` | int | 时间线长度/末帧时间 | ✅ |
| `loopBegin` | int | 循环起始 | ✅ |
| `loopEnd` | int | 循环结束 | ✅ |
| `diff` | int | ⚠️ 差分时间线标记 | 推断（代码读取为 `int8_t`） |
| `variableList` | array | 时间线上变量轨道 | ✅ |

`timelineControl[].variableList[]`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 变量名 | ✅ |
| `frameList` | array | 变量关键帧 | ✅ |

`timelineControl[].variableList[].frameList[]`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `time` | number | 帧时间 | ✅ |
| `type` | int | 帧类型 | ✅ |
| `content.easing` | number | 插值/缓动参数 | ✅ |
| `content.value` | number | 变量值 | ✅ |

### 5.2 `variableList[]`（metadata 级）

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 变量名 | ✅ |
| `frameList` | array |  | 当前实现未消费内部帧数据 |
| `frameList[].label` | string |  | 未在当前实现读取 |
| `frameList[].frame` | number |  | 未在当前实现读取 |

### 5.3 `eyeControl[]`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 受控变量名 | ✅ |
| `beginFrame` | int | 起始帧 | ✅ |
| `endFrame` | int | 结束帧 | ✅ |
| `blinkFrameCount` | int | 单次眨眼帧数 | ✅ |
| `blinkIntervalMin` | int | 眨眼最小间隔 | ✅ |
| `blinkIntervalMax` | int | 眨眼最大间隔 | ✅ |
| `blinkEnabled` | int/bool | ⚠️ 是否启用眨眼 | 推断；当前代码未直接读取该键 |
| `enabled` | int/bool |  | 当前代码未直接读取该键 |
| `edge` | number |  | 当前代码未直接读取该键 |
| `node` | string |  | 当前代码未直接读取该键 |
| `parameter` | string |  | 当前代码未直接读取该键 |

### 5.4 `eyebrowControl[]`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 受控变量名 | ✅ |
| `beginFrame` | int | 起始帧 | ✅ |
| `endFrame` | int | 结束帧（缺省时等于 begin） | ✅ |
| `enabled` | int/bool |  | 当前代码未直接读取该键 |
| `edge` | number |  | 当前代码未直接读取该键 |
| `node` | string |  | 当前代码未直接读取该键 |

### 5.5 `bustControl[]`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 控制器标签 | ✅ |
| `var_lr` | string | 左右控制变量 | ✅ |
| `var_ud` | string | 上下控制变量 | ✅ |
| `friction` | number | 摩擦参数 | ✅ |
| `gravity` | number | 重力参数 | ✅ |
| `spring` | number | 弹性参数 | ✅ |
| `scale_x` | number | 尺度参数 X | ✅ |
| `scale_y` | number | 尺度参数 Y | ✅ |
| `param` | object | 物理初始状态 | ✅ |
| `param.ofs` | number | 偏移 | ✅ |
| `param.op.{x,y,z}` | number |  | 代码读取，具体物理语义未完全确认 |
| `param.p.{x,y,z}` | number |  | 同上 |
| `param.pv.{x,y,z}` | number |  | 同上 |
| `baseLayer` | string | ⚠️ 作用层名 | 推断 |
| `enabled` | int/bool | ⚠️ 启用开关 | 推断 |
| `parameter` | string |  | 未确认 |

### 5.6 `hairControl[]` / `partsControl[]`

两者结构一致（由 `uniControl` 解析）：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 控制器标签 | ✅ |
| `var_lr` | string | 左右变量 | ✅ |
| `var_lrm` | string |  | 代码读取，语义不完全确定 |
| `var_ud` | string | 上下变量 | ✅ |
| `b_rate` | number |  | 代码读取，具体含义未完全确认 |
| `bend_spd` | number |  | 同上 |
| `bend_vol` | number |  | 同上 |
| `friction_x` | number |  | 同上 |
| `friction_y` | number |  | 同上 |
| `gravity` | number | 重力参数 | ✅（被读取） |
| `ud_eft` | number |  | 代码读取，语义不完全确定 |
| `v_bound` | number |  | 代码读取，语义不完全确定 |
| `scale_x` | array[number] | 分段尺度 X | ✅ |
| `scale_y` | array[number] | 分段尺度 Y | ✅ |
| `length` | array[number] | 分段长度 | ✅ |
| `param` | object | 分段物理参数 | ✅ |
| `param.ofs` | number | 偏移 | ✅ |
| `param.bendR` | number |  | 代码读取，语义不完全确定 |
| `param.bendS` | number |  | 代码读取，语义不完全确定 |
| `param.op` | object |  | 代码读取，语义不完全确定 |
| `param.bp[]` | array[object] | 基准点链 | ⚠️ 推断（代码用于段初始化） |
| `param.p[]` | array[object] | 当前位置链 | ⚠️ 推断 |
| `param.pv[]` | array[object] | 速度链 | ⚠️ 推断 |
| `baseLayer` | string | ⚠️ 作用层名 | 推断 |
| `enabled` | int/bool | ⚠️ 启用开关 | 推断 |
| `parameter` | string |  | 未确认 |

---

## 6. `object` 结构

`object` 是一个 map（键是对象名，如 `all_parts`、`head_parts`）。  
每个 `object.<name>` 结构：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `type` | int | 对象类型 | ✅ |
| `metadata` | object/null |  | 当前实现未使用 |
| `motion` | object | 动作集合（键为动作名） | ✅ |

---

## 7. `object.<name>.motion.<motionName>`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `type` | int | 动作类型 | ✅ |
| `lastTime` | int | 动作末时间 | ✅ |
| `loopTime` | int | 循环周期（`-1` 常表示不循环） | ⚠️ 推断 |
| `layer` | array | 根节点列表 | ✅ |
| `priority` | array | 渲染优先级列表 | ✅（读取 `content`） |
| `parameter` | array | 参数定义 | ✅ |
| `parameterize` | int/null | 参数化标记 | ✅ |
| `variable` | array |  | 当前实现未使用 |
| `metadata` | object/null |  | 当前实现未使用 |
| `referenceModelFileList` | array |  | 当前实现未使用 |
| `referenceProjectFileList` | array |  | 当前实现未使用 |
| `tag` | array |  | 当前实现未使用 |

`motion.parameter[]`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `id` | string | 参数名 | ✅ |
| `rangeBegin` | number | 范围起点 | ✅ |
| `rangeEnd` | number | 范围终点 | ✅ |
| `division` | int | 离散分段数 | ✅ |
| `enabled` | int/bool | ⚠️ 启用 | 推断 |
| `discretization` | int |  | 代码未使用该值 |

`motion.priority[]`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `content` | array[int] | 节点索引排序列表 | ✅ |
| `time` | number |  | 当前实现未使用 |
| `type` | int |  | 当前实现未使用 |

---

## 8. `layer` 节点（`motion.layer[]` 及其 `children[]`）

> **位姿与世界坐标如何实现：** 见 [`MOTIONPLAYER_TEXTURE_WORLD_COORDS.md`](MOTIONPLAYER_TEXTURE_WORLD_COORDS.md)（`coord` / `blank` / `source.origin` / `progress` 矩阵栈）。

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `label` | string | 节点名（可用于定位） | ✅ |
| `type` | int | 节点类型 | ✅ |
| `frameList` | array | 节点关键帧 | ✅ |
| `children` | array | 子节点 | ✅ |
| `inheritMask` | int | 继承/变换掩码 | ✅（用于行为分支） |
| `meshCombine` | int | 网格组合策略 | ⚠️ 推断（字段被读取） |
| `meshDivision` | int | 网格细分等级 | ✅（用于 CPU mesh 细分） |
| `meshTransform` | int | 网格变换模式 | ⚠️ 推断（字段被读取） |
| `meshSyncChildMask` | int | 子节点网格同步掩码 | ⚠️ 推断 |
| `parameterize` | int/null | 参数化索引 | ✅（有则参与变量驱动） |
| `stencilCompositeMaskLayerList` | array[string] | 模板合成关联层名 | ✅ |
| `objTriPriority` | int |  | 当前实现未使用 |
| `stencilType` | int |  | 当前实现未使用 |
| `coordinate` | int |  | 字段存在，当前实现未直接读取 |
| `groundCorrection` | int |  | 当前实现未直接读取 |
| `joinTarget` | string/null |  | 当前实现未直接读取 |
| `metadata` | object/null |  | 当前实现未直接读取 |
| `transformOrder` | int |  | 当前实现未直接读取 |
| `exportSelf` | int/bool |  | 当前实现未直接读取 |

---

## 9. `frameList[]` 与 `content`

`frameList[]`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `time` | number | 帧时间 | ✅ |
| `type` | int | 帧类型 | ✅ |
| `content` | object | 帧内容（可能为空） | ✅ |

`content`（本样例出现字段）：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `src` | string | 源引用（icon 或子 motion/layout） | ✅ |
| `mask` | int | 模板/遮罩标记 | ✅（读取） |
| `bm` | int | 混合模式 | ✅（映射到 blt 模式） |
| `ox` | number | 偏移 X | ✅ |
| `oy` | number | 偏移 Y | ✅ |
| `coord` | array[number] | 三维坐标（x/y/z） | ✅ |
| `angle` | number | 旋转角 | ✅（字段支持，样例中未广泛出现） |
| `sx` | number | 剪切/斜切 X | ✅（字段支持） |
| `sy` | number | 剪切/斜切 Y | ✅（字段支持） |
| `zx` | number | 缩放 X | ✅（字段支持） |
| `zy` | number | 缩放 Y | ✅（字段支持） |
| `opa` | number | 透明度 | ✅（字段支持） |
| `zcc` | object | Z 相关曲线控制 | ⚠️ 推断（字段被读取） |
| `ccc` | object | 颜色/曲线控制 | ⚠️ 推断（字段被读取） |
| `mesh` | object | 网格形变数据 | ✅ |
| `motion` | object |  | 旧格式分支支持，当前样例少见 |
| `icon` | string |  | 旧格式分支支持，当前样例少见 |

`content.mesh`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `bp` | array[32]/null | 网格控制偏移（32 点） | ✅ |
| `cc` | object/null | 曲线控制参数 | ⚠️ 推断 |
| `cc.c` | array[2] |  | 已读取，语义未完全确认 |
| `cc.x` | array[4] |  | 已读取，语义未完全确认 |
| `cc.y` | array[4] |  | 已读取，语义未完全确认 |

`content.zcc` / `content.ccc`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `c` | array[2] |  | 已读取，语义未完全确认 |
| `x` | array[4] |  | 已读取，语义未完全确认 |
| `y` | array[4] |  | 已读取，语义未完全确认 |

---

## 10. `source` 结构

`source` 是一个 map（键是资源组名，如 `face_mouth`）。

每个 `source.<name>`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `type` | int | 资源类型 | ✅ |
| `icon` | object | 图像变体集合 | ✅ |
| `metadata` | object/null |  | 当前实现未使用 |

`source.<name>.icon` 是一个 map（如 `"1"`, `"3"`... 变体编号）：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `originX` | number | 贴图锚点 X | ✅ |
| `originY` | number | 贴图锚点 Y | ✅ |
| `width` | number | 图块宽 | ✅ |
| `height` | number | 图块高 | ✅ |
| `compress` | string | 压缩类型（样例为 `RL`） | ✅ |
| `pixel` | resource/object | 像素资源引用 | ✅ |
| `pal` | resource/object | 调色板资源（如存在） | ✅（条件读取） |
| `clip` | object | 裁剪矩形 | ✅ |
| `metadata` | object/null |  | 当前实现未使用 |
| `attr` | object |  | 字段存在，当前实现未使用 |

`icon.clip`：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `left` | number | 裁剪左 | ✅ |
| `top` | number | 裁剪上 | ✅ |
| `right` | number | 裁剪右 | ✅ |
| `bottom` | number | 裁剪下 | ✅ |

`pixel`（PSB 资源引用对象，样例结构）：

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `__psb` | string | PSB 资源标识 | ⚠️ 推断 |
| `index` | int | 资源索引 | ✅（被 `parseResource` 使用） |
| `bytes` | int | 资源字节数 | ⚠️ 推断 |
| `extra` | bool |  | 含义未确认 |

---

## 11. PSD 导入相关字段（仅样例元数据）

这些字段在当前 `motionplayer` 运行路径未直接参与渲染/物理更新，属于导入辅助信息。

### 11.1 `psdGlobalImportInfo`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `layout` | int |  | 含义未确认 |
| `layoutZ` | int |  | 含义未确认 |

### 11.2 `psdLayoutImportInfoMap.<key>`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `psdLabel` | string | ⚠️ 对应 PSD 图层名 | 推断 |
| `z` | int | ⚠️ 图层 Z 顺序 | 推断 |

### 11.3 `psdScrapbookImportInfoMap.<key>`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `psdLabel` | string | ⚠️ PSD 图层名 | 推断 |
| `blendMode` | int | ⚠️ 混合模式 | 推断 |
| `mask` | int | ⚠️ 是否掩码 | 推断 |
| `remove` | int | ⚠️ 是否移除 | 推断 |
| `z` | int | ⚠️ Z 顺序 | 推断 |
| `clipRect` | object | ⚠️ 裁剪区域 | 推断 |
| `clipRect.left/top/width/height` | number | ⚠️ 裁剪矩形参数 | 推断 |

### 11.4 `psdTextureImportInfoMap.<key>`

| 字段 | 类型 | 作用 | 确定性/说明 |
|---|---|---|---|
| `blendMode` | int | ⚠️ 混合模式 | 推断 |
| `framePsdLabelList` | array[string] | ⚠️ 帧图层列表 | 推断 |
| `iconPsdLabelMap` | object | ⚠️ icon 名到 PSD 图层名映射 | 推断 |
| `isValid` | int/bool | ⚠️ 是否有效 | 推断 |
| `mask` | int | ⚠️ 掩码标记 | 推断 |
| `remove` | int | ⚠️ 移除标记 | 推断 |
| `z` | int | ⚠️ Z 顺序 | 推断 |

---

## 12. 当前实现“已知未完整支持/未使用”的字段清单

以下字段在 JSON 中存在，但当前代码未完整落地（仅读取、或完全未读取）：

- `metadata.mirrorControl.variableMatchList`（代码有注释未启用）
- `metadata.transitionControl`
- `metadata.clampControl`
- `metadata.mouthControl`
- `metadata.psd*` 导入映射系列
- `layer.transformOrder`、`layer.coordinate`、`layer.exportSelf`、`layer.groundCorrection`、`layer.joinTarget`
- `motion.variable`、`motion.tag`、`motion.metadata`
- `frame.content` 中部分旧格式键（如 `motion`、`icon`）

---

## 13. 备注

- 本文是对**该单个 JSON 样例**与**当前仓库实现**的对齐结果，不等价于 e-mote 全格式规范。
- 若后续需要“严格规范版”，建议再对照上游 e-mote SDK 文档与更多样例（`win/common` 规格）补齐。
