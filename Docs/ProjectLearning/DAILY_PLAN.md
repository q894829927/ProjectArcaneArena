# ProjectArcaneArena 28 天学习计划

> 目标：不是“把代码看完”，而是能够把任意核心功能从 **功能 → 类 → 调用链 → 网络角色 → UE/GAS 原理 → 设计原因 → 缺陷/边界 → 修改入口** 完整讲清楚。
>
> 本文件是 `Docs/ProjectLearning/` 的每日学习调度源。以后用户只说“第 N 天”时，AI 应读取本文件并执行对应 Day 的完整学习流程。

---

## 1. 使用方式

### 最简命令

用户只需要说：

```text
第 1 天
第 6 天
第 18 天
```

或：

```text
开始第 6 天
继续第 12 天
复习第 21 天
```

AI 必须先读取：

- `AGENTS.md`
- `IMPLEMENTED_FEATURES.md`
- `PENDING_VERIFICATION.md`
- `Docs/ProjectLearning/LEARNING_RULES.md`
- `Docs/ProjectLearning/LEARNING_PROGRESS.md`
- 本文件 `Docs/ProjectLearning/DAILY_PLAN.md`
- 当天主题对应的已有学习笔记

然后确认：

```bash
git branch --show-current
git rev-parse HEAD
git status --short
```

以当前 checkout 分支和当前源码为准，不默认 `main` 或 `develop` 一定是最新实现。

---

## 2. 每天固定学习流程

每一天统一执行下面 6 步：

1. **理论概念（约 20 分钟）**
   - 解释当天必须掌握的 UE5 / GAS / Multiplayer 概念。
   - 只讲与 ProjectArcaneArena 当天主题直接相关的理论。

2. **对照真实源码（约 60 分钟）**
   - 找核心 `.h/.cpp`、类、函数和必要资产关联。
   - 禁止逐文件、逐行翻译。
   - 每个关键结论尽量给出文件路径、类名、函数名。

3. **完整调用链（约 30 分钟）**
   - 至少形成一条端到端 Flow。
   - 涉及网络时标注 `[Local]`、`[Predicted]`、`[Server]`、`[Replicated]`、`[Presentation]`。

4. **脱离源码口述（约 30 分钟）**
   - AI 给出 3～5 个理解检查问题。
   - 用户先回答，AI 再判断“正确 / 部分正确 / 错误”。

5. **为什么不用另一种方案（约 20 分钟）**
   - 至少分析 3 个设计取舍。
   - 例如：为什么用 PlayerState、为什么 Damage Server-authoritative、为什么用 DataAsset。

6. **面试复盘（约 20 分钟）**
   - 生成当天高频面试题、追问题和一个可口述的总结。

如果当天只有约 1.5 小时，优先保留：

```text
源码 → 调用链 → 口述
```

---

## 3. 每个模块必须回答的 8 个问题

任何一天结束前，都要能回答：

1. 这个模块解决什么问题？
2. 核心类和文件有哪些？
3. 完整调用链怎么走？
4. Client / Server / Replication 如何分工？
5. 使用了哪些 UE / GAS 原理？
6. 为什么这样设计，而不是另一种方案？
7. 当前实现有什么缺陷、边界或待验证项？
8. 如果让我修改，应该从哪里开始？

---

## 4. 学习笔记目录

按知识主题长期维护，不按聊天记录堆内容：

```text
Docs/ProjectLearning/
│
├── 00_Index.md
├── DAILY_PLAN.md
├── LEARNING_PROGRESS.md
├── MY_UNDERSTANDING.md
│
├── Framework/
│   ├── Gameplay_Framework.md
│   ├── Player_Lifecycle.md
│   └── GamePhase_Wave.md
│
├── Network/
│   ├── Network_Basics.md
│   ├── Authority_RPC_Replication.md
│   ├── Prediction.md
│   └── Multiplayer_Flow.md
│
├── GAS/
│   ├── GAS_Overview.md
│   ├── ASC_PlayerState.md
│   ├── AttributeSet.md
│   ├── GameplayEffect.md
│   ├── GameplayTag.md
│   ├── TargetData.md
│   └── GameplayCue.md
│
├── Abilities/
│   ├── BasicAttack.md
│   ├── Fireball.md
│   ├── Dash.md
│   ├── Shield.md
│   └── LightningStorm.md
│
├── Gameplay/
│   ├── Damage.md
│   ├── Upgrade.md
│   ├── Trigger_System.md
│   ├── AI.md
│   ├── Boss.md
│   └── Inventory.md
│
├── Flows/
│   └── Gameplay_Flow_Atlas.md
│
├── Interview/
│   ├── Questions.md
│   ├── Bugs_and_Tradeoffs.md
│   └── Project_Introduction.md
│
└── Daily/
    ├── Day01.md
    ├── Day02.md
    └── ...
```

规则：

- 主题笔记保存长期知识。
- `Daily/DayXX.md` 只保存当天学习过程、错误理解、待确认点和下一步。
- 不提前创建大量空白文件；第一次需要时再生成。
- `MY_UNDERSTANDING.md` 只能记录用户本人明确表达过的理解，AI 不得把自己的总结冒充成用户理解。

---

# 第 1 周：项目骨架 + Multiplayer/GAS 主干

## Day 1 — 项目全局架构

### 核心目标

建立 ProjectArcaneArena 的“地图”，先不钻具体技能实现。

### 必学内容

- `AArenaGameMode`
- `AArenaGameState`
- `AArenaPlayerController`
- `AArenaPlayerState`
- `AArenaPlayerCharacter`
- `UArenaAbilitySystemComponent`
- `UArenaAttributeSet`
- GameplayAbility / GameplayEffect / GameplayTag / GameplayCue 的位置

### 当天必须产出

- ProjectArcaneArena 总体架构图
- 2 分钟脱稿项目架构介绍

### 更新笔记

- `00_Index.md`
- `Framework/Gameplay_Framework.md`
- `GAS/GAS_Overview.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Project_Introduction.md`
- `Daily/Day01.md`

---

## Day 2 — UE 多人网络基础

### 核心目标

真正区分 Authority、Local Control、RPC 和 Replication。

### 必学概念

- Authority
- Autonomous Proxy
- Simulated Proxy
- `HasAuthority()`
- `IsLocallyControlled()`
- Server RPC
- Client RPC
- Multicast
- Replicated
- RepNotify
- Listen Server Host 同时 Local + Authority

### 项目练习入口

重点用 Sprint / movement-related network flow 对照源码。

### 必答问题

为什么 Client 需要发 Server RPC，而 Listen Server Host 某些路径不需要？

### 更新笔记

- `Network/Network_Basics.md`
- `Network/Authority_RPC_Replication.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day02.md`

---

## Day 3 — ASC 为什么放 PlayerState

### 核心目标

理解：

```text
OwnerActor = PlayerState
AvatarActor = Character
```

### 重点阅读

- `AArenaPlayerCharacter::PossessedBy`
- `AArenaPlayerCharacter::OnRep_PlayerState`
- `AArenaPlayerCharacter::InitializeAbilityActorInfo`
- PlayerState 中 ASC / AttributeSet 的创建与访问

### 必须讲清

```text
Character Death / Replace
→ PlayerState 保留
→ ASC / Attribute / Upgrade 等长期状态保留
→ 新 Character 重新 InitAbilityActorInfo
```

### 更新笔记

- `Framework/Player_Lifecycle.md`
- `GAS/ASC_PlayerState.md`
- `Network/Multiplayer_Flow.md`
- `Interview/Questions.md`
- `Daily/Day03.md`

---

## Day 4 — AttributeSet + GameplayEffect + Damage Pipeline

### 必学属性

- Health / MaxHealth
- Shield
- Energy / MaxEnergy
- AttackPower
- Defense
- MoveSpeed
- CritChance / CritDamage
- Damage / Healing Meta Attribute

### 核心调用链

```text
GameplayAbility
→ Damage GameplayEffect
→ ExecCalc_Damage
→ Damage Meta Attribute
→ PostGameplayEffectExecute
→ Shield
→ Health
→ Death / Event / Cue / Replication
```

### 必答问题

为什么不直接 `Health -= Damage`？

### 更新笔记

- `GAS/AttributeSet.md`
- `GAS/GameplayEffect.md`
- `Gameplay/Damage.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day04.md`

---

## Day 5 — GameplayAbility / Tag / Cost / Cooldown

### 必学概念

- GameplayAbility
- AbilitySpec
- GameplayTag
- `CanActivateAbility`
- `ActivateAbility`
- `CommitAbility`
- Cost
- Cooldown
- ActivationBlockedTags
- `EndAbility`

### 必须区分

```text
CanActivate ≠ Commit
Activate ≠ 造成伤害
Commit ≠ EndAbility
```

### 更新笔记

- `GAS/GAS_Overview.md`
- `GAS/GameplayTag.md`
- `GAS/GameplayEffect.md`
- `Interview/Questions.md`
- `Daily/Day05.md`

---

## Day 6 — BasicAttack

### 核心目标

把 BasicAttack 作为 GAS 网络预测的第一条完整样例。

### 必学内容

- LocalPredicted
- PredictionKey
- TargetData
- WaitTargetData
- TargetActor
- CommitAbility
- Montage
- GameplayCue
- Server-authoritative Sweep
- Damage GE
- EndAbility

### 目标调用链

```text
LMB
→ Enhanced Input
→ InputTag
→ ASC AbilitySpec
→ TryActivateAbility
→ LocalPredicted
→ TargetData
→ Client Prediction
→ Server Sweep
→ GE_Damage
→ ExecCalc_Damage
→ AttributeSet
→ Shield / Health
→ Cue / UI / Replication
```

### 当天完成标准

不看代码，能连续讲 10～15 分钟 BasicAttack。

### 更新笔记

- `Abilities/BasicAttack.md`
- `GAS/TargetData.md`
- `Network/Prediction.md`
- `Gameplay/Damage.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day06.md`

---

## Day 7 — 第一轮模拟面试

### 不学习新模块

必须脱稿回答：

- 2 分钟介绍项目
- 为什么用 GAS
- 为什么 ASC 放 PlayerState
- 一次 BasicAttack 完整流程
- LocalPredicted 是什么
- TargetData 是什么
- PredictionKey 有什么用
- 为什么 Damage 必须 Server-authoritative

### 规则

出现“大概”“GAS 自动处理”“UE 应该会处理”时，记录成待查问题。

### 更新笔记

- `Interview/Questions.md`
- `Interview/Project_Introduction.md`
- `MY_UNDERSTANDING.md`（仅记录用户真实口述）
- `Daily/Day07.md`

---

# 第 2 周：GAS + 网络预测

## Day 8 — Fireball

### 重点比较

```text
BasicAttack → Server Sweep
Fireball    → Server Spawn Replicated Projectile
```

理解为什么 LocalPredicted Ability 仍然只能由 Authority 生成真实 Damage Projectile。

### 更新笔记

- `Abilities/Fireball.md`
- `Network/Prediction.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day08.md`

---

## Day 9 — Projectile 网络架构

### 核心流程

```text
Server Spawn Projectile
→ Replicate Actor / Movement
→ Client Presentation

Server Collision
→ Validate Target
→ Apply Damage
```

### 必答问题

为什么客户端不能独立 Spawn 一个造成真实 Damage 的 Projectile？

### 更新笔记

- `Abilities/Fireball.md`
- `Network/Authority_RPC_Replication.md`
- `Network/Multiplayer_Flow.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Daily/Day09.md`

---

## Day 10 — Dash

### 必学内容

- LocalPredicted
- Direction TargetData
- Root Motion
- `State.Dashing`
- `State.Invincible`
- Server confirmation / correction
- Collision / movement consistency

### 必答问题

为什么 Dash 的预测比 Fireball 更难？

### 更新笔记

- `Abilities/Dash.md`
- `Network/Prediction.md`
- `GAS/TargetData.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day10.md`

---

## Day 11 — Shield + LightningStorm

### 对比模型

```text
Shield
→ Attribute / GE 型持续状态

LightningStorm
→ Server Spawn Area Actor
→ Area Actor 自己维护持续生命周期
```

### 设计题

为什么 Ability 结束/取消，不等于之前 Spawn 的 Area Actor 一定销毁？

### 更新笔记

- `Abilities/Shield.md`
- `Abilities/LightningStorm.md`
- `GAS/GameplayEffect.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Daily/Day11.md`

---

## Day 12 — Prediction Rollback

### 核心流程

```text
Client Predict
→ PredictionKey
→ Server Accept / Reject
→ Reconcile / Rollback
```

### 项目调试重点

使用现有 Ability Reject / Audit 调试能力，验证：

- predicted Montage
- predicted cooldown/cost
- Server rejection
- Projectile / Area 不应权威生成
- Damage 不应发生

### 更新笔记

- `Network/Prediction.md`
- `Interview/Bugs_and_Tradeoffs.md`
- `Interview/Questions.md`
- `Daily/Day12.md`

---

## Day 13 — GameplayCue + 表现层

### 必学内容

- Execute GameplayCue
- Add GameplayCue
- Remove GameplayCue
- Instant Cue
- Persistent Cue
- Predicted presentation vs authoritative gameplay result

### 必答问题

为什么挥击动画/声音可以预测，而 Damage Hit Feedback 更适合服务器确认？

### 更新笔记

- `GAS/GameplayCue.md`
- `Network/Prediction.md`
- `Interview/Questions.md`
- `Daily/Day13.md`

---

## Day 14 — GAS 高压模拟

### 不学习新模块

至少连续解释 30 分钟：

- ASC
- AbilitySpec
- GameplayAbility
- GameplayEffect
- AttributeSet
- GameplayTag
- GameplayCue
- AbilityTask
- TargetActor
- TargetData
- PredictionKey
- CommitAbility
- LocalPredicted / ServerOnly
- OwnerActor / AvatarActor

### 更新笔记

- `GAS/GAS_Overview.md`
- `Interview/Questions.md`
- `MY_UNDERSTANDING.md`
- `Daily/Day14.md`

---

# 第 3 周：完整游戏架构

## Day 15 — GamePhase + Wave

### 必学内容

- Waiting
- Combat
- Upgrade
- BossIntro
- BossOutro
- Victory
- Defeat
- GameMode / GameState / Wave Manager 职责

### 核心设计

```text
GameMode 决定规则
GameState 复制结果
```

### 更新笔记

- `Framework/GamePhase_Wave.md`
- `Network/Multiplayer_Flow.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Daily/Day15.md`

---

## Day 16 — Upgrade / Roguelike Build

### 必学内容

- DataAsset
- UpgradePool
- GameplayTag
- RequiredTags / BlockedTags
- MaxStacks
- deterministic/random selection strategy
- PlayerState upgrade ownership

### 必答问题

为什么使用 DataAsset + Tag，而不是 `if (UpgradeId == "...")`？

### 更新笔记

- `Gameplay/Upgrade.md`
- `GAS/GameplayTag.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day16.md`

---

## Day 17 — Damage Event + Trigger / Passive 系统

### 必学方向

追踪项目现有 Trigger/Event：

- OnDamageDealt
- OnCrit
- OnKill
- OnAbilityCast
- OnDashEnd
- OnShieldBreak（若当前分支实现）

### 核心设计

```text
技能产生 Gameplay Event
→ 被动/升级监听
→ Tag/DataAsset 决定条件
→ Gameplay Result
```

### 必答问题

为什么不把所有 Upgrade 判断写进 Fireball.cpp？

### 更新笔记

- `Gameplay/Trigger_System.md`
- `Gameplay/Upgrade.md`
- `GAS/GameplayTag.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Daily/Day17.md`

---

## Day 18 — Enemy AI

### 必学内容

- EnemyCharacter
- EnemyAIController
- Target Selection
- NavMesh / Chase
- Attack
- GameplayAbility
- Death
- Behavior Tree / Blackboard（如果当前分支真实使用）

### 必答问题

为什么 Enemy AI 决策、攻击和 Damage 必须由服务器主导？

### 更新笔记

- `Gameplay/AI.md`
- `Network/Multiplayer_Flow.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Daily/Day18.md`

---

## Day 19 — Boss

### 必学内容

- BossCharacter
- BossAIController
- BehaviorTree / Blackboard
- BTService / Decorator / Task
- GroundSlam
- Charge
- FireZone
- Summon
- Phase / Enrage
- Boss Death / Outro / Victory

### 核心设计

```text
Behavior Tree 决策“用哪个技能”
GameplayAbility 执行“技能怎么做”
```

必须区分 Implemented / Partial / Verified。

### 更新笔记

- `Gameplay/Boss.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Questions.md`
- `Daily/Day19.md`

---

## Day 20 — Inventory

### 必学内容

- FastArraySerializer
- OwnerOnly
- StackId
- Item DataAsset
- Server RPC Validation
- Use / Drop / Pickup
- Transaction-like modification

### 必答问题

为什么背包不简单使用整个 `TArray<Item>` 每次完整 Replicate？

### 更新笔记

- `Gameplay/Inventory.md`
- `Network/Authority_RPC_Replication.md`
- `Network/Multiplayer_Flow.md`
- `Interview/Questions.md`
- `Daily/Day20.md`

---

## Day 21 — 第二轮完整模拟面试

### 目标

连续讲约 45 分钟：

```text
项目介绍
→ 整体架构
→ GAS
→ BasicAttack
→ Prediction
→ Fireball
→ Dash
→ Damage
→ Upgrade
→ AI
→ Boss
→ Inventory
→ Multiplayer
```

### 更新笔记

- `Interview/Project_Introduction.md`
- `Interview/Questions.md`
- `MY_UNDERSTANDING.md`
- `Daily/Day21.md`

---

# 第 4 周：设计取舍 + 高频追问

## Day 22 — 设计取舍

准备并能回答：

- 为什么 GAS？
- 为什么 ASC 在 PlayerState？
- 为什么 Server-authoritative Damage？
- 为什么 LocalPredicted？
- 为什么 TargetData？
- 为什么 DataAsset？
- 为什么 GameplayTag？
- 为什么 FastArray？
- 为什么 Behavior Tree + GAS？

### 更新笔记

- `Interview/Questions.md`
- `Interview/Bugs_and_Tradeoffs.md`
- `Daily/Day22.md`

---

## Day 23 — 反向设计题

针对每个关键架构做反事实分析：

- 如果不用 GAS？
- 如果 ASC 放 Character？
- 如果 Damage 客户端计算？
- 如果 Upgrade 用字符串 ID？
- 如果 Inventory 整个数组 Replicate？
- 如果 AI 在客户端运行？

### 更新笔记

- `Interview/Bugs_and_Tradeoffs.md`
- `Interview/Questions.md`
- `Daily/Day23.md`

---

## Day 24 — Bug 与边界条件

优先整理真实项目问题：

- Upgrade 阶段技能输入
- Projectile / Area 跨阶段残留
- Prediction Reject
- UI InputMode / Focus
- Seamless Travel
- Respawn ASC
- Host vs Client 差异
- Ability duplicate spawn/execution
- Invalid TargetData

每个 Bug 按：

```text
现象
→ 根因
→ 为什么危险
→ 修复
→ 为什么这样修
→ 学到了什么
```

### 更新笔记

- `Interview/Bugs_and_Tradeoffs.md`
- `Interview/Questions.md`
- `Daily/Day24.md`

---

## Day 25 — 多人联网专题

### 必学内容

- Listen Server
- Dedicated Server
- RPC
- Replication
- OwnerOnly
- Seamless Travel
- Prediction
- Authority
- Late Join
- PlayerState
- Direct IP Lobby / Ready / Host Start（以当前源码为准）

### 重点原则

不要把 Pending 写成 Verified。

### 更新笔记

- `Network/Multiplayer_Flow.md`
- `Network/Authority_RPC_Replication.md`
- `Interview/Questions.md`
- `Daily/Day25.md`

---

## Day 26 — 性能与工程化

### 学习方向

- RPC 数量与边界
- FastArray 增量复制
- Damage feedback batching
- Telemetry
- Python asset generation
- Idempotent scripts
- Git LFS
- Automation / verification strategy

### 更新笔记

- `Interview/Bugs_and_Tradeoffs.md`
- `Interview/Questions.md`
- `Daily/Day26.md`

必要时新增专题笔记，但不要为了目录完整而创建空文件。

---

## Day 27 — 三个版本项目介绍

### 30 秒版本

一句话定位 + 技术关键词。

### 2 分钟版本

背景 → 核心玩法 → 技术架构 → 技术难点 → 当前结果。

### 5 分钟版本

重点讲：

```text
GAS + Multiplayer + Prediction
```

不要把主要时间花在玩法介绍。

### 更新笔记

- `Interview/Project_Introduction.md`
- `Daily/Day27.md`

---

## Day 28 — 终极模拟面试

### 规则

- 不看代码
- 不开项目
- 禁止说“大概”
- 禁止说“GAS 自动做的”
- 禁止说“UE 应该会处理”

每个答案至少明确：

```text
哪个类
哪个函数
Client / Server
什么数据
为什么
```

### 更新笔记

- `Interview/Questions.md`
- `MY_UNDERSTANDING.md`
- `LEARNING_PROGRESS.md`
- `Daily/Day28.md`

---

# 5. “第 N 天”自动生成协议

当用户只说“第 N 天”时，AI 必须把它理解为：

> 按 `DAILY_PLAN.md` 执行 Day N 的完整 ProjectArcaneArena 学习任务。

一次完整 Day 输出必须包含下面内容。

## A. 今日目标

- 今天到底要学会什么。
- 学完后能回答哪些问题。

## B. 先修概念

- 只列当天必要理论。
- 每个理论用 ProjectArcaneArena 实际使用场景解释。

## C. 今日源码导航

提供：

- 文件路径
- 类
- 函数
- 阅读顺序
- 为什么先看这里

不能只给“建议看 Source/”。

## D. 源码学习正文

按：

```text
功能
→ 核心类
→ 生命周期
→ 调用链
→ 数据流
→ 网络角色
→ UE/GAS 原理
→ 设计原因
→ 边界问题
→ 修改入口
```

讲解。

## E. 今日完整调用链

至少一条真实 Flow，并写入/更新：

`Flows/Gameplay_Flow_Atlas.md`

## F. “为什么”问题

至少 3 个架构 Tradeoff 问题。

## G. 面试问题

至少 5 个，从基础到追问逐级增加。

## H. 用户复述阶段

AI 必须停下来让用户自己回答，不要直接宣布“今天学完”。

只有用户完成复述/问答后，才进入笔记收尾。

## I. 当天笔记收尾

学习完成后：

1. 创建/更新对应主题笔记。
2. 创建/更新 `Daily/DayXX.md`。
3. 更新 `Flows/Gameplay_Flow_Atlas.md`。
4. 新问题写入 `Interview/Questions.md`。
5. Bug/Tradeoff 写入 `Interview/Bugs_and_Tradeoffs.md`。
6. 更新 `LEARNING_PROGRESS.md`。
7. `MY_UNDERSTANDING.md` 只记录用户真实表达过的理解。

---

# 6. Daily 文件模板

每一天的 `Daily/DayXX.md` 使用：

```markdown
# Day XX — 主题

## 今日目标

## 今天阅读的源码

## 今天真正弄懂的内容

## 今日核心调用链

## 我原来的错误理解

## 修正后的理解

## 设计取舍

## 面试问题

## 我自己的复述

## 仍未搞懂

## 待编辑器 / PIE / 网络验证

## 下一步
```

---

# 7. 主题笔记统一模板

主题文件统一采用：

```markdown
# 模块名称

## 1. 它解决什么问题

## 2. 核心类与职责

## 3. 核心源码位置

## 4. 生命周期 / 入口

## 5. 完整调用链

## 6. 数据流

## 7. 网络角色

## 8. UE / GAS 机制

## 9. 为什么这样设计

## 10. 当前问题 / 边界条件

## 11. 如果让我修改

## 12. 面试问题

## 13. 我的理解

## 14. 待确认
```

其中“我的理解”只能来自用户本人真实表达。

---

# 8. 笔记证据规则

重要结论按以下标签区分：

- `【源码确认】`
- `【项目文档】`
- `【资产关联】`
- `【推测】`
- `【待编辑器验证】`

不得：

- 把 `AGENTS.md` 的规划当成已实现事实。
- 把 `Implemented` 自动等同于 `Verified`。
- 根据 `.uasset` 文件名猜 Blueprint Graph 内部行为。
- 为了笔记完整而编造不存在的类、函数或系统。

---

# 9. 学习优先级

| 优先级 | 模块 | 面试价值 |
|---|---|---|
| S | UE Network Authority / RPC / Replication | ★★★★★ |
| S | GAS 整体架构 | ★★★★★ |
| S | LocalPredicted / PredictionKey / TargetData | ★★★★★ |
| S | Server-authoritative Damage | ★★★★★ |
| S | BasicAttack / Fireball / Dash | ★★★★★ |
| A | PlayerState ASC 架构 | ★★★★★ |
| A | GameplayTag / GE / AttributeSet | ★★★★★ |
| A | Roguelike Upgrade 数据驱动 | ★★★★☆ |
| A | AI + Behavior Tree + GAS | ★★★★☆ |
| A | Boss | ★★★★☆ |
| B | Inventory / FastArray | ★★★★☆ |
| B | GameplayCue / Niagara | ★★★☆☆ |
| B | UI / Input Mode | ★★★☆☆ |
| B | Python 自动化 | ★★★☆☆ |
| C | 美术、具体数值 | ★☆☆☆☆ |

前 10 天优先保证 GAS + Multiplayer 主线完整。

---

# 10. 最终掌握标准

28 天结束后，面对：

> “鼠标左键按下之后发生了什么？”

应该能够从：

```text
Enhanced Input
→ InputTag
→ AbilitySpec
→ TryActivateAbility
→ PredictionKey
→ CanActivateAbility
→ TargetActor
→ TargetData
→ CommitAbility
→ Montage
→ Server Sweep
→ GameplayEffectSpec
→ ExecCalc_Damage
→ Damage Meta Attribute
→ AttributeSet
→ Shield
→ Health
→ GameplayEvent
→ GameplayCue
→ Replication
```

持续讲清楚，并继续回答：

- Server Reject 怎么办？
- 为什么客户端不能决定 Damage？
- 为什么 TargetData 不能直接作为可信命中结果？
- 为什么 ASC 在 PlayerState？
- 当前哪些地方只是 Implemented，哪些已经 Verified？

达到这个程度，才认为 ProjectArcaneArena 的核心 Gameplay / GAS / Multiplayer 主线真正掌握。
