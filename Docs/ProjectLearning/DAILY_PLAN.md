# ProjectArcaneArena 28 天学习计划

> 本文件是 `Docs/ProjectLearning/` 的每日学习调度源。以后用户只说“第 N 天”时，AI 应按这里的 Day N 计划执行完整学习流程，并在学习结束后维护对应笔记。

## 1. 最终目标

不是把所有代码看一遍，而是做到：

```text
功能是什么
→ 哪些类负责
→ 调用链怎么走
→ 数据怎么流
→ Client / Server 怎么分工
→ 使用了哪些 UE / GAS 机制
→ 为什么这样设计
→ 当前有什么边界/缺陷
→ 如果让我修改应该从哪里开始
```

任何模块结束前必须能回答：

1. 它解决什么问题？
2. 核心类和源码文件有哪些？
3. 完整调用链是什么？
4. Local / Client / Server / Replication 如何分工？
5. 用到了哪些 UE / GAS 原理？
6. 为什么这样设计？
7. 当前有哪些边界、Bug 或待验证项？
8. 修改入口在哪里？

---

## 2. 用户只说“第 N 天”时怎么执行

例如：

```text
第 1 天
第 6 天
第 18 天
```

AI 必须先读取：

- `AGENTS.md`
- `IMPLEMENTED_FEATURES.md`
- `PENDING_VERIFICATION.md`
- `Docs/ProjectLearning/LEARNING_RULES.md`
- `Docs/ProjectLearning/LEARNING_PROGRESS.md`
- `Docs/ProjectLearning/DAILY_PLAN.md`
- 当天主题已有笔记

然后确认当前 branch、HEAD commit 和工作区状态，以当前 checkout 源码为准。

### 当天完整输出必须包含

1. **今日目标**：今天到底要学会什么。
2. **先修概念**：只讲当天必要理论。
3. **源码导航**：文件路径、类、函数、推荐阅读顺序。
4. **源码学习正文**：功能、核心类、生命周期、调用链、数据流、网络角色、UE/GAS 原理。
5. **完整调用链**：至少 1 条真实 Flow。
6. **设计取舍**：至少 3 个“为什么不用另一种方式”。
7. **边界与验证**：区分 Implemented / Partial / Verified / Pending Verification。
8. **面试题**：至少 5 题，从基础到追问。
9. **用户复述**：AI 必须停下来让用户回答，不直接宣布学完。
10. **笔记收尾**：用户完成复述后再更新学习笔记。

涉及网络时标记：

```text
[Local]
[Owning Client]
[Predicted]
[Server]
[Replicated]
[Simulated Client]
[Presentation]
```

涉及 GAS 时说明：

```text
ASC
AbilitySpec
GameplayAbility
GameplayEffect
AttributeSet
GameplayTag
GameplayEvent
GameplayCue
TargetActor
TargetData
PredictionKey
```

在当天流程中的真实作用。

---

## 3. 每天固定节奏

```text
理论概念
→ 对照 ProjectArcaneArena 真实源码
→ 自己画调用链
→ 不看源码口述
→ 为什么不用另一种方式
→ 面试复盘
```

推荐时间：

- 理论：20 min
- 源码：60 min
- 调用链：30 min
- 口述：30 min
- 设计取舍：20 min
- 面试复盘：20 min

如果只有约 1.5 小时，优先保留：

```text
源码 → 调用链 → 口述
```

---

## 4. 笔记目录

主题笔记保存长期知识，Daily 只记录当天过程。

```text
Docs/ProjectLearning/
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

不要提前创建大量空文件。第一次学习对应主题时再创建。

`MY_UNDERSTANDING.md` 只能记录用户本人明确表达过的理解，AI 不得把自己的总结冒充成用户理解。

---

# 第 1 周：项目骨架 + Multiplayer/GAS 主干

## Day 1 — 项目全局架构

学习：

- `AArenaGameMode`
- `AArenaGameState`
- `AArenaPlayerController`
- `AArenaPlayerState`
- `AArenaPlayerCharacter`
- `UArenaAbilitySystemComponent`
- `UArenaAttributeSet`
- GameplayAbility / GameplayEffect / GameplayTag / GameplayCue 在项目中的位置

当天目标：画出总体架构图，并能脱稿讲 2 分钟。

更新：

- `00_Index.md`
- `Framework/Gameplay_Framework.md`
- `GAS/GAS_Overview.md`
- `Flows/Gameplay_Flow_Atlas.md`
- `Interview/Project_Introduction.md`
- `Daily/Day01.md`

## Day 2 — UE 多人网络基础

学习：Authority、Autonomous Proxy、Simulated Proxy、`HasAuthority()`、`IsLocallyControlled()`、Server/Client/Multicast RPC、Replicated、RepNotify、Listen Server Host。

项目练习：重点追 Sprint / movement network flow。

必答：为什么 Client 需要 Server RPC，而 Host 某些路径不需要？

更新：`Network/Network_Basics.md`、`Network/Authority_RPC_Replication.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day02.md`。

## Day 3 — ASC 为什么放 PlayerState

核心：

```text
OwnerActor = PlayerState
AvatarActor = Character
```

重点阅读：

- `AArenaPlayerCharacter::PossessedBy`
- `AArenaPlayerCharacter::OnRep_PlayerState`
- `AArenaPlayerCharacter::InitializeAbilityActorInfo`
- PlayerState 中 ASC / AttributeSet 创建与访问

必须讲清：

```text
Character Death / Replace
→ PlayerState 保留
→ ASC / Attribute / Upgrade 等长期状态保留
→ 新 Character 重新 InitAbilityActorInfo
```

更新：`Framework/Player_Lifecycle.md`、`GAS/ASC_PlayerState.md`、`Network/Multiplayer_Flow.md`、`Interview/Questions.md`、`Daily/Day03.md`。

## Day 4 — AttributeSet + GameplayEffect + Damage Pipeline

学习 Health、Shield、Energy、AttackPower、Defense、MoveSpeed、CritChance、CritDamage，以及 Damage/Healing Meta Attribute。

核心：

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

必答：为什么不直接 `Health -= Damage`？

更新：`GAS/AttributeSet.md`、`GAS/GameplayEffect.md`、`Gameplay/Damage.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day04.md`。

## Day 5 — GameplayAbility / Tag / Cost / Cooldown

学习 GameplayAbility、AbilitySpec、GameplayTag、`CanActivateAbility`、`ActivateAbility`、`CommitAbility`、Cost、Cooldown、ActivationBlockedTags、`EndAbility`。

必须区分：

```text
CanActivate ≠ Commit
Activate ≠ 造成伤害
Commit ≠ EndAbility
```

更新：`GAS/GAS_Overview.md`、`GAS/GameplayTag.md`、`GAS/GameplayEffect.md`、`Interview/Questions.md`、`Daily/Day05.md`。

## Day 6 — BasicAttack

把 BasicAttack 作为 GAS 网络预测第一条完整样例。

必学：LocalPredicted、PredictionKey、TargetData、WaitTargetData、TargetActor、CommitAbility、Montage、GameplayCue、Server-authoritative Sweep、Damage GE、EndAbility。

目标调用链：

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

完成标准：不看代码连续讲 10～15 分钟。

更新：`Abilities/BasicAttack.md`、`GAS/TargetData.md`、`Network/Prediction.md`、`Gameplay/Damage.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day06.md`。

## Day 7 — 第一轮模拟面试

不学新模块。脱稿回答：项目介绍、为什么 GAS、为什么 ASC 在 PlayerState、BasicAttack 完整流程、LocalPredicted、TargetData、PredictionKey、为什么 Damage 必须服务器权威。

出现“大概”“GAS 自动处理”“UE 应该会处理”时，记录为待查问题。

更新：`Interview/Questions.md`、`Interview/Project_Introduction.md`、`MY_UNDERSTANDING.md`、`Daily/Day07.md`。

---

# 第 2 周：GAS + 网络预测

## Day 8 — Fireball

比较：

```text
BasicAttack → Server Sweep
Fireball    → Server Spawn Replicated Projectile
```

重点：为什么 LocalPredicted Ability 仍然只能由 Authority 生成真实 Damage Projectile。

更新：`Abilities/Fireball.md`、`Network/Prediction.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day08.md`。

## Day 9 — Projectile 网络架构

核心：

```text
Server Spawn Projectile
→ Replicate Actor / Movement
→ Client Presentation

Server Collision
→ Validate Target
→ Apply Damage
```

必答：为什么客户端不能独立 Spawn 一个真实伤害 Projectile？

更新：`Abilities/Fireball.md`、`Network/Authority_RPC_Replication.md`、`Network/Multiplayer_Flow.md`、`Flows/Gameplay_Flow_Atlas.md`、`Daily/Day09.md`。

## Day 10 — Dash

学习 LocalPredicted、Direction TargetData、Root Motion、`State.Dashing`、`State.Invincible`、Server confirmation/correction、Collision / movement consistency。

必答：为什么 Dash 的预测比 Fireball 更难？

更新：`Abilities/Dash.md`、`Network/Prediction.md`、`GAS/TargetData.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day10.md`。

## Day 11 — Shield + LightningStorm

比较：

```text
Shield → Attribute / GE 型持续状态
LightningStorm → Server Spawn Area Actor → Area 自己维护生命周期
```

设计题：为什么 Ability 结束/取消，不等于之前 Spawn 的 Area Actor 一定销毁？

更新：`Abilities/Shield.md`、`Abilities/LightningStorm.md`、`GAS/GameplayEffect.md`、`Flows/Gameplay_Flow_Atlas.md`、`Daily/Day11.md`。

## Day 12 — Prediction Rollback

核心：

```text
Client Predict
→ PredictionKey
→ Server Accept / Reject
→ Reconcile / Rollback
```

结合项目已有 Ability Reject / Audit 调试能力验证 predicted Montage、Cost/Cooldown、Server rejection、Projectile/Area 不权威生成、Damage 不发生。

更新：`Network/Prediction.md`、`Interview/Bugs_and_Tradeoffs.md`、`Interview/Questions.md`、`Daily/Day12.md`。

## Day 13 — GameplayCue + 表现层

学习 Execute/Add/Remove GameplayCue、Instant Cue、Persistent Cue、预测表现与权威结果的区别。

必答：为什么挥击动画/声音可以预测，而 Damage Hit Feedback 更适合服务器确认？

更新：`GAS/GameplayCue.md`、`Network/Prediction.md`、`Interview/Questions.md`、`Daily/Day13.md`。

## Day 14 — GAS 高压模拟

不学新模块。连续解释 ASC、AbilitySpec、GameplayAbility、GameplayEffect、AttributeSet、GameplayTag、GameplayCue、AbilityTask、TargetActor、TargetData、PredictionKey、CommitAbility、LocalPredicted/ServerOnly、OwnerActor/AvatarActor。

更新：`GAS/GAS_Overview.md`、`Interview/Questions.md`、`MY_UNDERSTANDING.md`、`Daily/Day14.md`。

---

# 第 3 周：完整游戏架构

## Day 15 — GamePhase + Wave

学习 Waiting、Combat、Upgrade、BossIntro、BossOutro、Victory、Defeat，以及 GameMode / GameState / Wave Manager 的职责。

核心：

```text
GameMode 决定规则
GameState 复制结果
```

更新：`Framework/GamePhase_Wave.md`、`Network/Multiplayer_Flow.md`、`Flows/Gameplay_Flow_Atlas.md`、`Daily/Day15.md`。

## Day 16 — Upgrade / Roguelike Build

学习 DataAsset、UpgradePool、GameplayTag、RequiredTags/BlockedTags、MaxStacks、随机选择策略、PlayerState upgrade ownership。

必答：为什么使用 DataAsset + Tag，而不是 `if (UpgradeId == "...")`？

更新：`Gameplay/Upgrade.md`、`GAS/GameplayTag.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day16.md`。

## Day 17 — Damage Event + Trigger / Passive

追踪当前源码真实存在的 OnDamageDealt、OnCrit、OnKill、OnAbilityCast、OnDashEnd、OnShieldBreak 等事件。

核心：

```text
技能产生 Gameplay Event
→ 被动/升级监听
→ Tag/DataAsset 决定条件
→ Gameplay Result
```

必答：为什么不把所有 Upgrade 判断写进 Fireball.cpp？

更新：`Gameplay/Trigger_System.md`、`Gameplay/Upgrade.md`、`GAS/GameplayTag.md`、`Flows/Gameplay_Flow_Atlas.md`、`Daily/Day17.md`。

## Day 18 — Enemy AI

学习 EnemyCharacter、EnemyAIController、Target Selection、NavMesh/Chase、Attack、GameplayAbility、Death，以及当前源码真实使用的 Behavior Tree / Blackboard。

必答：为什么 Enemy AI 决策、攻击和 Damage 必须由服务器主导？

更新：`Gameplay/AI.md`、`Network/Multiplayer_Flow.md`、`Flows/Gameplay_Flow_Atlas.md`、`Daily/Day18.md`。

## Day 19 — Boss

学习 BossCharacter、BossAIController、BehaviorTree、Blackboard、BTService/Decorator/Task、GroundSlam、Charge、FireZone、Summon、Phase/Enrage、Boss Death/Outro/Victory。

核心：

```text
Behavior Tree 决策“用哪个技能”
GameplayAbility 执行“技能怎么做”
```

必须区分 Implemented / Partial / Verified。

更新：`Gameplay/Boss.md`、`Flows/Gameplay_Flow_Atlas.md`、`Interview/Questions.md`、`Daily/Day19.md`。

## Day 20 — Inventory

学习 FastArraySerializer、OwnerOnly、StackId、Item DataAsset、Server RPC Validation、Use / Drop / Pickup、事务式修改。

必答：为什么背包不简单使用整个 `TArray<Item>` 每次完整 Replicate？

更新：`Gameplay/Inventory.md`、`Network/Authority_RPC_Replication.md`、`Network/Multiplayer_Flow.md`、`Interview/Questions.md`、`Daily/Day20.md`。

## Day 21 — 第二轮完整模拟面试

连续讲：

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

更新：`Interview/Project_Introduction.md`、`Interview/Questions.md`、`MY_UNDERSTANDING.md`、`Daily/Day21.md`。

---

# 第 4 周：设计取舍 + 高频追问

## Day 22 — 设计取舍

准备：为什么 GAS、为什么 ASC 在 PlayerState、为什么 Server-authoritative Damage、为什么 LocalPredicted、为什么 TargetData、为什么 DataAsset、为什么 GameplayTag、为什么 FastArray、为什么 Behavior Tree + GAS。

更新：`Interview/Questions.md`、`Interview/Bugs_and_Tradeoffs.md`、`Daily/Day22.md`。

## Day 23 — 反向设计题

分析：不用 GAS、ASC 放 Character、Damage 客户端计算、Upgrade 用字符串 ID、Inventory 整体数组 Replicate、AI 在客户端运行分别会怎样。

更新：`Interview/Bugs_and_Tradeoffs.md`、`Interview/Questions.md`、`Daily/Day23.md`。

## Day 24 — Bug 与边界条件

重点：Upgrade 阶段技能输入、Projectile/Area 跨阶段残留、Prediction Reject、UI InputMode/Focus、Seamless Travel、Respawn ASC、Host vs Client、Ability duplicate execution、Invalid TargetData。

每个 Bug：

```text
现象
→ 根因
→ 为什么危险
→ 修复
→ 为什么这样修
→ 学到了什么
```

更新：`Interview/Bugs_and_Tradeoffs.md`、`Interview/Questions.md`、`Daily/Day24.md`。

## Day 25 — 多人联网专题

学习 Listen Server、Dedicated Server、RPC、Replication、OwnerOnly、Seamless Travel、Prediction、Authority、Late Join、PlayerState、Direct IP Lobby/Ready/Host Start（以当前源码为准）。

原则：不要把 Pending 写成 Verified。

更新：`Network/Multiplayer_Flow.md`、`Network/Authority_RPC_Replication.md`、`Interview/Questions.md`、`Daily/Day25.md`。

## Day 26 — 性能与工程化

学习 RPC 数量与边界、FastArray 增量复制、Damage feedback batching、Telemetry、Python asset generation、Idempotent scripts、Git LFS、Automation / verification strategy。

更新：`Interview/Bugs_and_Tradeoffs.md`、`Interview/Questions.md`、`Daily/Day26.md`。

## Day 27 — 三个版本项目介绍

准备：

- 30 秒：一句话定位 + 技术关键词
- 2 分钟：背景 → 核心玩法 → 技术架构 → 技术难点 → 当前结果
- 5 分钟：重点讲 `GAS + Multiplayer + Prediction`

更新：`Interview/Project_Introduction.md`、`Daily/Day27.md`。

## Day 28 — 终极模拟面试

规则：不看代码、不开项目、禁止说“大概”“GAS 自动做的”“UE 应该会处理”。

每个答案至少明确：

```text
哪个类
哪个函数
Client / Server
什么数据
为什么
```

更新：`Interview/Questions.md`、`MY_UNDERSTANDING.md`、`LEARNING_PROGRESS.md`、`Daily/Day28.md`。

---

## 5. Daily 文件模板

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

## 6. 主题笔记统一模板

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

“我的理解”只能来自用户本人真实表达。

---

## 7. 笔记证据规则

使用：

- `【源码确认】`
- `【项目文档】`
- `【资产关联】`
- `【推测】`
- `【待编辑器验证】`

禁止：

- 把 `AGENTS.md` 规划当成已实现事实。
- 把 Implemented 自动等同于 Verified。
- 根据 `.uasset` 名称猜 Blueprint Graph。
- 编造不存在的类、函数和系统。

---

## 8. 学习优先级

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

## 9. 最终掌握标准

28 天后，面对“鼠标左键按下之后发生了什么？”，应该能从：

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
- 哪些只是 Implemented，哪些已经 Verified？
