# 项目介绍（Interview 用）

> Day 1 建立，随学习深入持续更新。目标：30 秒 / 2 分钟 / 5 分钟三个版本都能脱稿。

## 30 秒版

Project Arcane Arena 是一个基于 UE5.6 与 Gameplay Ability System 的俯视角 Roguelike 竞技场战斗 Demo：玩家在竞技场里对抗多波敌人，每波结束后三选一升级构筑流派（火 / 雷 / 暴击 / 护盾 / 冲刺），最终击败 Boss 获胜。核心展示 C++ GAS 架构、服务器权威伤害、多人友好的预测/复制设计，以及顶视角 / 第三人称双视角兼容。

## 2 分钟版

背景 → 核心玩法 → 技术架构 → 当前结果：

- 背景：面向简历的 UE5 C++ 游戏玩法 Demo，先保证单机完整循环，再扩展双人 Listen Server。
- 核心玩法：波次战斗 → 三选一升级 → 构筑成长 → Boss 战 → 胜利。升级不仅是数值，还有技能变体、触发被动与状态协同（Fire+Lightning → Overload 等）。
- 技术架构：规则在 `AArenaGameMode`/`AArenaWaveManager`（服务器独占），比赛事实（阶段/波数/敌人数/Boss）在 `AArenaGameState` 复制广播；玩家 ASC/AttributeSet 放在 `AArenaPlayerState`（跨死亡重生保留），`AArenaPlayerCharacter` 只做 Avatar 与输入采集；输入经 Enhanced Input → GameplayTag → AbilitySpec 动态源标签路由进 GAS。
- 当前结果：五种玩家主动技能（BasicAttack/Fireball/Dash/Shield/LightningStorm）、服务器权威伤害管线（ExecCalc + Shield 优先 + 元属性）、Tag 驱动状态控制、数据驱动升级池、Boss 战与动态演出镜头、双视角输入恢复、Direct IP Lobby 多人流程均已实现；部分多人/打包验证仍在 PENDING_VERIFICATION.md 中。

## 5 分钟版（重点：GAS + Multiplayer + Prediction）

1. **为什么 GAS**：属性修改统一走 GameplayEffect；状态用 GameplayTag 而非 bool；技能激活/冷却/消耗标准化；ExecutionCalculation 承载伤害公式。
2. **ASC 为什么在 PlayerState**：Owner/Avatar 分离（Owner=PlayerState 保长期状态，Avatar=Character 可替换），死亡重生不丢技能/属性/升级；`PossessedBy`（服务器）与 `OnRep_PlayerState`（客户端）两端 InitAbilityActorInfo。
3. **为什么服务器权威**：伤害由 `UExecCalc_Damage` 在服务器结算，客户端只提交意图与 TargetData；升级选择、波次推进、Boss 阶段全部服务器重验；客户端只拥有表现与预测。
4. **Prediction**：玩家技能用 LocalPredicted，配合 PredictionKey、TargetData 与服务器确认/拒绝（项目带 `arena.Net.RejectNextAbility` / `AbilityAudit` 调试工具）；Dash 有方向 TargetData 与服务器规范化校验。
5. **当前边界**：哪些 Implemented / Partial / Verified 一定要分清楚（见 LEARNING_PROGRESS 与各主题笔记）。

## 待完善（后续 Day 更新）
- Day 2 后补 Network 专项表达；Day 6 后补 BasicAttack 完整预测链；Day 21 第二轮完整模拟面试。
