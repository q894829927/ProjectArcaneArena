# ProjectArcaneArena 学习命令

这些命令通过仓库级 Codex Skills 实现。Codex 会从 `.agents/skills/` 自动发现它们。

> 推荐显式调用形式：`$skill-name`。也可以通过 `/skills` 打开技能列表选择。

---

## 1. `$project-learn` — 日常总命令

用途：按照当前学习进度自动选择最值得继续深入的一个模块，分析真实源码，并更新学习笔记。

示例：

```text
$project-learn
```

也可以附加方向：

```text
$project-learn 今天优先把 GAS 初始化和 Player 生命周期串起来
```

默认学习顺序：

```text
Project Map
→ Gameplay Framework
→ Player Lifecycle
→ GAS Core
→ Attribute / Damage
→ Ability
→ Input / Targeting / View
→ Enemy AI
→ Wave / GamePhase
→ Roguelike Upgrade
→ Build / Status / Trigger
→ Multiplayer
→ Boss
→ UI / Inventory / Pickup
→ GameplayCue / VFX
→ DataAsset / Config
→ Python Asset Automation
→ Debug / Verification
→ Gameplay Flow Atlas
→ Final Summary
```

---

## 2. `$project-trace` — 追完整调用链

用途：从一个玩家行为或 Gameplay 结果出发，追踪真实调用路径、数据流和网络角色。

示例：

```text
$project-trace Fireball 从按 Q 到敌人扣血
```

```text
$project-trace 敌人死亡后如何推进 Wave
```

```text
$project-trace Overload 是如何被 Lightning + Burning 触发的
```

结果应优先补充到 `19_Gameplay_Flow_Atlas.md`。

---

## 3. `$project-explain` — 深入理解一个类/模块

用途：解释某个类、组件、DataAsset 或系统在整个项目里的真实位置。

示例：

```text
$project-explain AArenaPlayerState
```

```text
$project-explain UArenaAbilitySystemComponent
```

```text
$project-explain Wave System
```

重点回答：

```text
它是什么
→ 为什么存在
→ 谁创建
→ 生命周期
→ 核心成员
→ 核心函数
→ 谁调用它
→ 它调用谁
→ 数据怎么流
→ 网络角色
→ GAS角色
→ 修改入口
```

---

## 4. `$project-why` — 理解设计原因

用途：针对“为什么这样设计”进行源码驱动分析，而不是只解释代码行为。

示例：

```text
$project-why 为什么 Player ASC 放在 PlayerState，而不是 PlayerCharacter？
```

```text
$project-why 为什么 Damage 要经过 Meta Attribute？
```

```text
$project-why 为什么 Targeting 本地采集但最终结果由 Server 决定？
```

要求区分：源码明确设计、UE/GAS 通用原因、基于当前架构的合理推断。

---

## 5. `$project-change-plan` — 改功能前先做架构分析

用途：不直接写业务代码，先分析一个需求会影响哪些系统和文件。

示例：

```text
$project-change-plan 给 Fireball 增加分裂升级
```

```text
$project-change-plan 新增一个远程精英敌人
```

输出应包含：

```text
现有 Flow
→ 需求插入点
→ 涉及类/文件/资产
→ GAS 影响
→ Multiplayer 影响
→ DataAsset/Blueprint/GameplayCue 影响
→ 潜在风险
→ 验证方案
```

默认学习模式下不修改 `Source/`、`Config/`、`Content/`。

---

## 6. `$project-review-notes` — 复查并修正旧笔记

用途：源码持续变化后，重新检查学习笔记是否过期、错误或缺少证据。

示例：

```text
$project-review-notes
```

或者：

```text
$project-review-notes 重点检查最近 Boss 和 Inventory 的变化
```

检查项：

- 当前 branch / HEAD 是否改变；
- 已记录类名、函数名、路径是否仍存在；
- 调用链是否因重构失效；
- 是否把 `Implemented` 错写成 `Verified`；
- 是否把项目规划误写成已经实现；
- 是否缺少 Client / Server / Replication 信息；
- 是否存在无源码依据的结论；
- 是否有重复笔记可以合并。

---

# 推荐日常工作流

正常学习：

```text
$project-learn
```

开发时遇到一条不懂的链路：

```text
$project-trace <功能>
```

看到一个重要类：

```text
$project-explain <类名>
```

想搞懂架构原因：

```text
$project-why <为什么问题>
```

准备自己增加功能：

```text
$project-change-plan <需求>
```

项目开发一段时间后：

```text
$project-review-notes
```

# 三个状态必须严格区分

```text
源码看懂了 ≠ 功能已经实现 ≠ 功能已经验证通过
```

- “源码看懂”写入学习笔记；
- “实现状态”参考并验证 `IMPLEMENTED_FEATURES.md`；
- “验证状态”参考 `PENDING_VERIFICATION.md` 以及真实 PIE / Standalone / Listen Server / Dedicated Server / Packaged Build 结果。
