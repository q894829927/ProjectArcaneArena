# Gameplay Framework（框架层）

> Day 1 建立。核心：一局竞技场战斗如何拆成"规则 / 复制事实 / 本地输入 / 长期玩家状态 / 身体表现"五层。
> 证据标签：【源码确认】= 已核对 C++；【项目文档】= 来自 IMPLEMENTED_FEATURES.md 等；【资产关联】= 通过引用关系确认；【推测】；【待编辑器验证】。

## 1. 它解决什么问题

把"一局 Roguelike 竞技场战斗"的职责切分到互不重叠的框架对象上：

- 谁决定规则（波次、升级资格、胜负、晚加入拒绝）→ 服务器独占
- 谁同步比赛事实（阶段、波数、剩余敌人、Boss、升级种子）→ 复制到所有端
- 谁接收本地输入并提交意图 → Owning Client 的 Controller / Character
- 谁长期持有玩家的 GAS 状态（属性、技能、升级、背包）→ PlayerState
- 谁承载身体与表现（移动、相机、蒙太奇、受击反馈）→ Character

## 2. 核心类与职责【源码确认】

| 类 | 权威位置 | 核心职责 |
|---|---|---|
| `AArenaGameMode` | 仅服务器，不复制 | 装配框架类；波次启动；升级候选生成/验证/授予；胜负；Victory 重开 |
| `AArenaWaveManager` | 仅服务器 | 由 GameMode 生成；刷怪、波次完成检测、Boss Intro/Outro、掉落 |
| `AArenaGameState` | 服务器写、全端复制 | `EArenaGamePhase`、波次索引、剩余敌人数、升级种子、ActiveBoss、Boss 时序、Victory 计数；挂非复制 `UArenaBalanceTelemetryComponent` |
| `AArenaPlayerController` | 本地输入/UI；服务器收 RPC | HUD/升级/背包/ESC Widget；Boss Intro/Outro 本地镜头；UI 意图转 Server RPC |
| `AArenaPlayerState` | 服务器写、复制到端 | 拥有 ASC、AttributeSet、InventoryComponent；升级候选/拥有/堆叠；Victory Ready |
| `AArenaCharacterBase` | 复制 | `bReplicates + SetReplicateMovement(true)`；挂 `UArenaHitReactionComponent` |
| `AArenaPlayerCharacter` | 移动服务器权威 | Enhanced Input 采集、双视角相机、输入转 GAS InputTag、绑定 ASC 委托 |
| `UArenaAbilitySystemComponent` | 随 Owner（PlayerState） | 输入 Tag 路由、NotifyAbilityCommit 事件、权威伤害事件路由与 Cue 批次 |
| `UArenaAttributeSet` | 服务器结算 | 10 个复制属性 + Damage/Healing 元属性；clamp；死亡 Tag；伤害反馈入队 |

`EArenaGamePhase` 七个阶段（ArenaGameState.h:10）：
`Waiting / Combat / Upgrade / Victory / Defeat / BossIntro / BossOutro`

## 3. 核心源码位置

- 装配清单：`Private/Core/ArenaGameMode.cpp:26` `AArenaGameMode::AArenaGameMode()`
- 服务器启动：`ArenaGameMode.cpp:68` `BeginPlay()`（随机流 + SpawnActor<WaveManager> + 绑定阶段委托 + 初始玩家等待闸门）
- 晚加入拒绝：`ArenaGameMode.cpp:37` `InitGame()`、`:54` `PreLogin()`
- 阶段枚举与复制属性：`Public/Core/ArenaGameState.h:10`、`:215` 起
- 玩家 GAS 创建：`Private/Core/ArenaPlayerState.cpp:13`（ASC + AttributeSet + InventoryComponent）
- GAS 与 Avatar 接合：`Private/Character/ArenaPlayerCharacter.cpp:127` `InitializeAbilityActorInfo()`
- 输入路由入口：`Private/Character/ArenaPlayerCharacter.cpp:686` `Input_AbilityInputTagPressed()` → `Private/GAS/ArenaAbilitySystemComponent.cpp:150` `AbilityInputTagPressed()`

## 4. 生命周期 / 入口

```text
引擎 World 创建 GameMode（GlobalDefaultGameMode = BP_ArenaGameMode，蓝图子类【资产关联】）
  └ 构造函数指定 GameState/PlayerController/PlayerState/DefaultPawn 类
     └ 引擎按类创建实例
        └ GameMode::BeginPlay [Server] 生成 WaveManager
           └ WaveManager 写 GameState → RepNotify → 各端委托
GameMode::HandleStartingNewPlayer → RestartPlayer → 生成 Pawn → Possess
  └ Character::PossessedBy [Server] → InitializeAbilityActorInfo
     └ ASC->InitAbilityActorInfo(PlayerState, this)
        ├ [Server] ApplyDefaultAttributes + GrantStartupAbilities
        └ BindAbilitySystemDelegates
  客户端：OnRep_PlayerState → 同一 InitializeAbilityActorInfo（不授予）
```

生命周期规则：
- GameMode / GameState / WaveManager：随整局存在，关卡重载（Victory 重开）时重建。
- PlayerController / PlayerState：随连接存在，Pawn 死亡不销毁。
- Character：可替换的 Avatar；`EndPlay` 解绑 ASC 委托（ArenaPlayerCharacter.cpp:119），新 Pawn 重新 Init。
- 防重复：`bGrantedStartupAbilities` / `bAppliedDefaultAttributes` 在 PlayerState 上。

## 5. 完整调用链

详见 `Flows/Gameplay_Flow_Atlas.md`。两条主链：
- Flow A：玩家进入 → GAS 就绪 → 左键进入技能激活入口
- Flow B：波次 → 升级选择（RPC + 服务器重验）→ 下一波

## 6. 数据流

| 数据 | 产生 | 类型 | 传递 | 验证 | 消费 | 复制 |
|---|---|---|---|---|---|---|
| 阶段/波次/敌人数/种子/ActiveBoss | WaveManager/GameMode [Server] | enum/int32/指针/Timing | GameState Setter | Setter 仅服务器语义 | 各端 Controller/HUD/Character 委托 | ReplicatedUsing 全端 |
| 玩家属性 | 服务器 GE 初始化 | FGameplayAttributeData | ASC/AttributeSet | clamp + 元属性结算 | HUD 属性委托；Character MoveSpeed | RepNotify |
| 升级候选/拥有 | GameMode 服务器随机流 | TArray<DataAsset>/FArenaOwnedUpgrade | PlayerState | SubmitUpgradeSelection 重验 | Owning Client UI | OwnerOnly |
| bHasSelectedUpgrade | 服务器 CompleteUpgradeSelection | bool | PlayerState | HasAuthority | 各端 UI 显示等待 | 全端复制 |

## 7. 网络角色

- Local / Owning Client：采集输入、转 Tag、运行全部 UI（HUD/升级/背包/ESC/Boss 演出本地镜头）；通过 Server RPC 提交意图（`ServerSelectUpgrade`、`ServerSetVictoryRestartReady`、`ServerUseInventoryItem` 等）；不决定伤害/升级/阶段。
- Server：GameMode/WaveManager 独占规则（开波、刷怪、候选生成、胜负、晚加入拒绝）；所有 RPC 实现里重复验证阶段与资格。
- Simulated Client：看不到 GameMode；通过 GameState 复制看比赛事实；通过 PlayerState 复制看别人的 `bHasSelectedUpgrade`（但看不到别人的候选，OwnerOnly）。
- Listen Server Host：同时是 Authority 和 Local Control；`PossessedBy` 与 `OnRep_PlayerState` 都会走 `InitializeAbilityActorInfo`，但授予只在 `HasAuthority()` 分支执行一次。

## 8. UE / GAS 机制

- GameMode 不复制 → 规则天然防篡改；客户端只拿到结果（复制值）。
- PlayerState 生命周期 > Character → 死亡重生不丢 GAS 状态（构造注释 ArenaPlayerState.cpp:18 明确写明）。
- Owner/Avatar 分离 → 服务器 `PossessedBy`、客户端 `OnRep_PlayerState` 各自 Init；漏客户端会症状为"技能按了没反应、Cue 不播"。
- RepNotify 委托是 UI 唯一事实来源 → "UI 不拥有玩法状态"结构性成立。
- AbilitySpec `DynamicSpecSourceTags` 存输入 Tag → 新增技能无需改 Character/ASC 路由。

## 9. 为什么这样设计

1. **ASC 放 PlayerState 而非 Character**
   当前方案 → 重生不丢状态；替代 → ASC 放 Character，死亡重生要重新授予/迁移全部技能属性，且 ActiveGE 生命周期难保留。
2. **规则在 GameMode、事实复制在 GameState**
   当前方案 → GameState RepNotify 保证晚加入玩家自动拿到状态；替代 → GameMode 用 Multicast RPC 通知，晚加入拿不到历史状态，UI 需自建同步。
3. **输入经 GameplayTag 路由而非 Character 直接调 Ability**
   当前方案 → GA 资产配 InputTag 即接线；替代 → Character 持有技能引用，每加一个技能改一次 Character。
4. **C++ 运行时创建 Enhanced Input 资产而非 Blueprint 资产**
   当前方案 → 模板阶段不依赖资产、旅行/重生映射不丢；替代 → 标准资产化，但当前方案要处理序列化时机（`RebuildDefaultInputMappings` 的存在本身就是代价；头注释注明"后续可迁移到项目资产"）。

## 10. 当前问题 / 边界条件

- 框架核心 `Implemented`（源码确认）；"全员死亡判定 + 死亡目标重定向"通过双人 PIE 冒烟，但**最终 GameMode 修复由 Live Coding 加载，完整构建仍 pending**【项目文档】。
- `BP_ArenaGameMode` 资产内的 WaveData / UpgradePool / StartupAbilities / DefaultAttributeEffect 实际值在 Blueprint 资产中【待编辑器验证】。
- Mass 插件已启用但 Day 1 主玩法链路未使用【源码确认】。
- 双视角 2P 独立验证未记录【项目文档】。

## 11. 如果让我修改

- 加一个比赛阶段：`EArenaGamePhase` 加枚举 → WaveManager 推进处写 Setter → GameState 复制 → 各端 HandleGamePhaseChanged 响应。
- 加一个服务器规则：在 GameMode/WaveManager 加函数，从 RPC 入口（Controller）进入，写完再复制。
- 加一个主动技能：新建继承 `UArenaGameplayAbility` 的 GA（配 InputTag）+ 加入 StartupAbilities 配置；Character 输入绑定与 ASC 路由不用动。

## 12. 面试问题

见 `Interview/Questions.md` Day 1 六连问。

## 13. 我的理解

（来自用户 2026-08-15 Day 1 复述，原话要点）
- GameMode 管规则、只存服务器；GameState 服务器写复制给客户端；PlayerController 收本地输入/建 UI、提交意图由服务器校验；PlayerState 存长期数据（含 ASC）服务器写复制；Character 把按键转成移动/技能。
- InitAbilityActorInfo 在客户端和服务端各调用一次，初始化能力系统。
- PreAttributeChange 在属性修改前做上下限限制；PostGameplayEffectExecute 在效果执行后处理扣血、死亡判定、事件派发。
- 把 ASC 绑到 PlayerState 上解决重生时 ASC 上下文的保留。

## 14. 待确认

- 【待验证】框架核心完整构建编译。
- 【待验证】BP_ArenaGameMode 资产内升级池 / 波次数据实际配置。
- 【待验证】双人 Listen Server 独立视角 + 升级选择的双端展示。
