# Day 1 — 项目全局架构

- 日期：2026-08-15
- Branch：`develop`
- Commit：`53ecf9ad380d1dd1bd3ead48705905476592a879`
- Working Tree：clean

## 今日目标

画出总体架构图（GameMode / GameState / PlayerController / PlayerState / Character / ASC / AttributeSet 七者关系），并脱稿讲 2 分钟"玩家进入关卡 → 能放技能"。

## 今天阅读的源码

- `Source/ProjectArcaneArena/Public/Core/ArenaGameMode.h`、`ArenaGameState.h`、`ArenaPlayerState.h`、`ArenaPlayerController.h`
- `Source/ProjectArcaneArena/Private/Core/ArenaGameMode.cpp`（构造/InitGame/PreLogin/BeginPlay/升级授予/SubmitUpgradeSelection）、`ArenaPlayerState.cpp`、`ArenaPlayerController.cpp`（局部）
- `Source/ProjectArcaneArena/Private/Character/ArenaPlayerCharacter.cpp`（PossessedBy/OnRep_PlayerState/InitializeAbilityActorInfo/ApplyDefaultAttributes/GrantStartupAbilities/输入绑定/Input_AbilityInputTagPressed）、`ArenaCharacterBase.cpp`
- `Source/ProjectArcaneArena/Private/GAS/ArenaAbilitySystemComponent.cpp`、`Public/GAS/ArenaAttributeSet.h`、`Public/GAS/ArenaGameplayTags.h`、`Public/GAS/ArenaGameplayAbility.h`
- `Source/ProjectArcaneArena/ProjectArcaneArena.Build.cs`、`ProjectArcaneArena.uproject`、`Config/DefaultEngine.ini`、`Config/DefaultGame.ini`

## 今天真正弄懂的内容

- 七个框架类 + WaveManager 的职责边界与"谁创建谁"。
- 玩家 ASC/AttributeSet 挂在 PlayerState（Owner），Character 只是 Avatar；`PossessedBy` 与 `OnRep_PlayerState` 两端 Init。
- 输入 → GameplayTag → AbilitySpec 动态源标签 → TryActivateAbility 的路由。
- GameState RepNotify 委托是客户端唯一事实来源。
- 升级选择：客户端只交 `FName`，服务器重验阶段/资格/堆叠后 GE + GiveAbility + Tag 授予。

## 今日核心调用链

见 `Flows/Gameplay_Flow_Atlas.md`：
- Flow A：玩家进入 → GAS 就绪 → 技能输入入口
- Flow B：波次 → 升级选择 → 下一波

## 我原来的错误理解

- Character 是"依赖 Pawn 的对象" → 实际 Character 本身就是 Pawn（`ACharacter : APawn`）。
- PlayerController 只有客户端有 → 服务器端也各有一个 PC（收 RPC 校验），本地 UI/输入只在 Owning Client。
- GE 是"回调函数集合" → 实际是数据载体（Modifiers/Executions/Tags），执行在 ASC/AttributeSet。
- "ASC 放 Character，重生时保留上下文再初始化" → 实际 ASC 一直活着在 PlayerState 上，重生只换 Avatar，无需保存再恢复。
- 不清楚防重复授予机制 → 守卫位 `bGrantedStartupAbilities` / `bAppliedDefaultAttributes` + `HasAuthority()` 分支。

## 修正后的理解

- InitAbilityActorInfo 参数：`(OwnerActor=PlayerState, AvatarActor=Character)`；服务器在 PossessedBy、客户端在 OnRep_PlayerState。
- 授予（GiveAbility/ApplyDefaultAttributes）只在服务器 `HasAuthority()` 分支执行一次，客户端只做 Avatar 接合。
- 升级候选/拥有 OwnerOnly（私有卡池），bHasSelectedUpgrade 全端复制（供 UI 显示等待状态）。
- 默认属性走 GE 是为了统一 clamp/复制/委托/后处理管线 + 可配置初始值。

## 设计取舍

1. ASC 放 PlayerState：长命状态跨 Pawn 保留，代价是两端 Init 时机。
2. 规则在 GameMode、事实复制在 GameState：晚加入自动同步，UI 不拥有状态。
3. 输入经 Tag 路由：加技能不改 Character/ASC；代价多一层间接。
4. C++ 运行时建 Enhanced Input 资产：旅行不丢映射；代价处理序列化时机。

详见 `Interview/Bugs_and_Tradeoffs.md`。

## 面试问题

见 `Interview/Questions.md` Day 1 六连问（功能 → 调用链 → 网络 → GAS 原理 → 设计取舍 → 边界/重构）。

## 我自己的复述

- Q1 部分正确（补充：Character 就是 Pawn；服务器端也有 PC；客户端没有 GameMode/WaveManager）。
- Q2 部分正确（知道两端各 Init 一次，补齐步骤与参数）。
- Q3 未答（OwnerOnly vs 全端复制的用途与反例）。
- Q4 部分正确（PreAttributeChange clamp、PostGameplayEffectExecute 后处理正确；补 GE 数据载体本质 + 为什么走 GE）。
- Q5 部分正确（方向对，补守卫位机制；不需要"保存恢复"）。
- Q6 未答（Host 不重复授予原因 + 新增技能的改/不改清单）。

## 仍未搞懂

- Prediction 具体路径（PredictionKey / TargetData / Server Reject）→ Day 5/6/12。
- 伤害公式细节（BaseDamage / SkillMultiplier / Crit / Defense / Shield-first）→ Day 4。
- Enhanced Input 资产运行时重建与资产化的迁移方案细节 → 后续按需。

## 待编辑器 / PIE / 网络验证

- 【待验证】`BP_ArenaGameMode` 资产内 WaveData / UpgradePool / StartupAbilities / DefaultAttributeEffect 实际配置。
- 【项目文档】框架核心完整构建（含最终 GameMode 修复）pending。
- 【项目文档】双人 Listen Server 独立视角 + 升级选择双端展示未记录验证。

## 下一步

- Day 2：UE 多人网络基础（Authority / Autonomous / Simulated Proxy、RPC、Replication、RepNotify），重点追 Sprint / movement 网络流。
