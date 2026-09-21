# ProjectArcaneArena 学习进度

> 本文件由 `$project-learn` 与相关学习 Skills 持续维护。

## 当前源码版本

- Branch: `develop`
- Commit: `53ecf9ad380d1dd1bd3ead48705905476592a879`
- Working Tree: `clean`
- Last Updated: `2026-08-15（Day 1 完成复述收尾）`

## 已完成

- 学习框架、总提示词和 repo-scoped Skills 已建立。
- **Day 1 — 项目全局架构（复述已完成并收尾）**
  - 画出框架架构：GameMode/WaveManager（服务器规则）、GameState（复制事实）、PlayerController（本地输入/UI + Server RPC）、PlayerState（ASC/AttributeSet/Inventory/升级 Owner）、Character（Avatar + 输入采集）。
  - 玩家 ASC/AttributeSet 在 PlayerState，Owner/Avatar 分离，`PossessedBy`/`OnRep_PlayerState` 两端 Init。
  - 输入经 GameplayTag → AbilitySpec 动态源标签 → TryActivateAbility 路由。
  - 升级选择：客户端只提交 `FName`，服务器重验阶段/资格/堆叠后 GE + GiveAbility + Tag 授予。
  - 用户复述判定：Q1/Q2/Q4/Q5 部分正确，Q3/Q6 未答；关键误解已修正（Character 就是 Pawn、服务器端也有 PC、GE 是数据载体、防重复靠守卫位）。

## 正在学习

- 无（Day 1 已收尾，等待进入 Day 2）。

## 待学习

建议顺序：

1. ~~Project Map~~（Day 1 完成：00_Index + Framework/Gameplay_Framework + GAS/GAS_Overview + Flows + Interview + Daily）
2. Gameplay Framework — 已建主题笔记，随 Day 15/20 深化
3. Player Lifecycle — Day 3
4. GAS Core — 已建总览，AttributeSet/GE/Tag 细讲 Day 4/5
5. Attributes and Damage — Day 4
6. Ability System — Day 5/6
7. Input / Targeting / Dual View — Day 6
8. Network Basics / RPC / Replication — **Day 2（下一课）**
9. Wave / GamePhase — Day 15
10. Roguelike Upgrade — Day 16
11. Status / Trigger / Build — Day 17
12. Multiplayer — Day 25
13. Boss System — Day 19
14. UI / Inventory / Pickup — Day 20
15. GameplayCue / VFX — Day 13
16. DataAsset / Asset Config — Day 16
17. Python Asset Automation — Day 26
18. Debug / Verification — Day 26
19. Gameplay Flow Atlas — 已建 Flow A/B，随每日追加
20. Final Summary — Day 28

## 已发现关键调用链

- **Flow A**：玩家进入 → `PossessedBy`/`OnRep_PlayerState` → `InitAbilityActorInfo(PlayerState, Character)` → [Server] 授予属性/技能 → 输入 Tag → `ASC::AbilityInputTagPressed` → `TryActivateAbility`。见 `Flows/Gameplay_Flow_Atlas.md`。
- **Flow B**：WaveManager 推进阶段 → GameState 复制 → 升级候选生成（服务器随机流）→ 客户端 Server RPC 提交 → GameMode 重验 → ApplyUpgrade → CompleteUpgradeSelection → 全员选完下一波。

## 待确认问题

- 【待验证】框架核心完整构建（含最终 GameMode 修复）pending，复习时不得把该部分说成完整 Verified。
- 【待编辑器验证】`BP_ArenaGameMode` 资产内 WaveData / UpgradePool / StartupAbilities / DefaultAttributeEffect 实际配置值。
- 【推测】Mass 插件已启用但 Day 1 主玩法链路未使用。
- 双人 Listen Server 独立视角 + 升级选择双端展示未记录验证。

## 下一步建议

- 进入 **Day 2 — UE 多人网络基础**：Authority / Autonomous Proxy / Simulated Proxy、HasAuthority / IsLocallyControlled、Server/Client/Multicast RPC、Replicated / RepNotify、Listen Server Host 双角色；项目练习重点追 Sprint / movement 网络流（`Input_SprintStarted → ServerSetSprinting`）。
- 复述优先纠错点：Character 是 Pawn 而非依赖 Pawn；PC 两端都有；GE 是数据载体；防重复授予靠守卫位 + HasAuthority 分支。

## 维护规则

每次学习后更新：

- 当前 Branch / Commit / Working Tree；
- 本轮完成的模块；
- 新确认的关键调用链；
- 新发现但尚未解决的问题；
- 下一步最值得学习的内容。

不要把"源码理解完成""功能实现""功能验证通过"混成同一个状态。
