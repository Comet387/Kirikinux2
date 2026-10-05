# MotionPlayer：TVP 与 SDL3 对照

> **SDL3 真源：** `docs/sdl3/emoteplayerclass.cpp`  
> **TVP：** `emoteplayerclass.cpp`

---

## SDL3 实际流程

```text
_limitArea = Layer
updateTransMat → matTrans = _affineTrans * model（SDL3 负号矩阵，GPU tess）
progress → 建树（根矩阵来自 progress 时刻，早于本帧 setDrawAffine）
setDrawAffine → 更新 _affineTrans，下一帧 progress 才进 tess
draw → GPU FBO(Layer 尺寸) → glReadPixels → MainImage（1:1 拷贝，无二次仿射）
```

## TVP CPU 为何不能照抄 tess

TVP `evaluatePatchPosition` + `clipToLayerPoint` 在根栈乘 SDL3 的 `_affineTrans×model` 时 `tri` 会飞出 drawLim（日志 `inOffscreen=0`）。这是 **CPU tess 与 GPU shader 的差异**，不是少写 rebuild 能修的。

## TVP 等价分工（同一脚本契约）

| 阶段 | SDL3 | TVP |
|------|------|-----|
| progress / tess 根 | `_affineTrans×model`（GPU） | **仅 `model`** |
| 中间缓冲 | GPU FBO | `m_BmpBits` scratch（= FBO） |
| 贴到 Layer | `memcpy`（仿射已在 tess） | `AffineBlt`：`composite = _affineTrans × inv(model)` |
| draw 内 rebuild | 无 | **无**（与 SDL3 相同） |

`composite` 的 `inv(model)` 用于抵消 `AffineSourceMotion` 未清空 `calcImageMatrix` 导致的 imagex/scale 与 `setCoord` 重复。

---

## 脚本顺序（勿改）

```text
setCoord / setScale → progress → setDrawAffine → draw
```

---

*调试：`EMOTE_DRAW_DEBUG_CENTER=1` 直绘 MainImage，跳过 scratch/composite。*
