# Bugs 与 Tradeoffs（真实缺陷 / 设计取舍）

> 只记录真实存在（源码 / 文档可考）的 Bug 或明确的架构取舍。格式：现象 → 根因 → 为什么危险 → 修复/权衡 → 学到什么。
> 证据标签：【源码确认】【项目文档】【推测】【待验证】。

## Tradeoff 1 — ASC 放 PlayerState 而非 Character

- 现状：`AArenaPlayerState` 创建 ASC/AttributeSet/Inventory（`ArenaPlayerState.cpp:13`），Character 只是 Avatar。
- 取舍：重生不丢 GAS 状态、升级/技能/背包跨 Pawn 保留。
- 代价：必须处理 Owner/Avatar 初始化时机——服务器 `PossessedBy`、客户端 `OnRep_PlayerState` 各 Init 一次；漏客户端一步会出现"技能按了没反应"的典型症状。
- 学到：多生命期状态放长命对象，短命对象只做 Avatar；"接合"动作要在两端各做一次。

## Tradeoff 2 — 规则在 GameMode、事实复制在 GameState

- 现状：GameMode/WaveManager 服务器写 GameState，GameState RepNotify 全端广播（`ArenaGameState.h:215` 起）。
- 取舍：晚加入玩家自动拿到当前比赛状态；UI 只订阅委托，不拥有状态。
- 替代方案：GameMode 用 Multicast RPC 通知 → 晚加入拿不到历史状态、断线重连丢状态。
- 学到：能复制就不要 RPC；能观察就不要持有。

## Tradeoff 3 — 输入经 GameplayTag 路由

- 现状：输入 → `InputTag` → AbilitySpec `DynamicSpecSourceTags` 匹配（`ArenaAbilitySystemComponent.cpp:150`）。
- 取舍：新增技能只需 GA 配 InputTag + 加入 StartupAbilities，Character/ASC 路由零改动。
- 代价：多一层间接，调试"按键没反应"要多查 Tag 是否进 Spec 动态源标签。
- 学到：数据驱动路由能避免 `if (Ability == ...)` 的硬编码膨胀。

## Tradeoff 4 — C++ 运行时创建 Enhanced Input 资产

- 现状：`CreateDefaultInputMappings` / `RebuildDefaultInputMappings` 在 C++ 构造与序列化后重建 InputAction/MappingContext，`PawnClientRestart` 重新安装（`ArenaPlayerCharacter.cpp:69/558/631`）。
- 根因：LocalPlayer 输入子系统跨关卡持久，Cook/旅行后不重建会丢/重映射。
- 代价：需要处理序列化时机与幂等（Remove 后再 Add）；头注释注明"后续可迁移到项目资产"。
- 学到：模板期不依赖资产可行，但把"资产化"当负债记录比当假设更安全。

## 待验证 / 未确认边界

- 【项目文档】"全员死亡判定 + 死亡目标重定向"通过双人 PIE 冒烟，但最终 GameMode 修复是 Live Coding 加载，完整构建 pending —— 复习时不要说成完整 Verified。
- 【待验证】`BP_ArenaGameMode` 资产内 WaveData/UpgradePool/StartupAbilities 实际配置值。
- 【推测】Mass 插件已启用但 Day 1 主玩法链路未使用。
