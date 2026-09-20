# ProjectArcaneArena 学习笔记索引

> 本文件是 `Docs/ProjectLearning/` 的总索引。每个主题第一次真正学习时才创建，禁止提前创建空文件。
> 状态标签：`Implemented` / `Partial` / `Verified` / `Pending Verification`，以及证据标签【源码确认】【项目文档】【资产关联】【推测】【待编辑器验证】。

## 主题笔记

### Framework（框架层）
- [Gameplay_Framework.md](Framework/Gameplay_Framework.md) — Day 1 已建：GameMode / GameState / PlayerController / PlayerState / Character 职责与装配。
- Player_Lifecycle.md — 规划中（Day 3）。
- GamePhase_Wave.md — 规划中（Day 15）。

### GAS
- [GAS_Overview.md](GAS/GAS_Overview.md) — Day 1 已建：ASC / AttributeSet / Ability / GE / Tag / Cue 在项目中的位置。
- ASC_PlayerState.md、AttributeSet.md、GameplayEffect.md、GameplayTag.md、TargetData.md、GameplayCue.md — 规划中（Day 3-6、11、13）。

### Abilities
- BasicAttack.md、Fireball.md、Dash.md、Shield.md、LightningStorm.md — 规划中（Day 6、8、10、11）。

### Gameplay
- Damage.md、Upgrade.md、Trigger_System.md、AI.md、Boss.md、Inventory.md — 规划中（Day 4、16、17、18、19、20）。

### Network
- Network_Basics.md、Authority_RPC_Replication.md、Prediction.md、Multiplayer_Flow.md — 规划中（Day 2、6、8、9、10、20、25）。

### Flows（最重要）
- [Gameplay_Flow_Atlas.md](Flows/Gameplay_Flow_Atlas.md) — Day 1 已建：玩家进入 → GAS 就绪 → 技能输入入口；波次 → 升级 → 下一波。

### Interview
- [Project_Introduction.md](Interview/Project_Introduction.md) — Day 1 已建：30 秒 / 2 分钟 / 5 分钟版本项目介绍。
- [Questions.md](Interview/Questions.md) — Day 1 已建：Day 1 六连问 + 关键解答。
- [Bugs_and_Tradeoffs.md](Interview/Bugs_and_Tradeoffs.md) — Day 1 已建：首轮架构取舍。

### Daily（每日过程）
- [Day01.md](Daily/Day01.md) — Day 1 已建。

### 用户自己的理解（只能来自用户本人表达）
- [MY_UNDERSTANDING.md](MY_UNDERSTANDING.md)

## 学习进度
- [LEARNING_PROGRESS.md](LEARNING_PROGRESS.md)

## 状态总览（截至 Day 1）
- 框架七大类的职责与装配：`Implemented`（源码确认）
- 玩家进入 → GAS 初始化 → 技能输入入口调用链：`Implemented`（已从源码追踪）
- 波次 → 升级 → 下一波循环：`Implemented`（已从源码追踪）
- 框架核心 PIE 验证：`Partial`（部分冒烟通过，完整构建待验证）
- `BP_ArenaGameMode` 资产内的具体数值（WaveData / UpgradePool / StartupAbilities）：【待编辑器验证】
