# GAS 总览（GAS Overview）

> Day 1 建立。主题：GAS 在 ProjectArcaneArena 中的位置——哪些类负责什么、挂在哪里、如何被创建与调用。属性/伤害细节留给 Day 4，Ability 激活细节留给 Day 5/6。
> 证据标签：【源码确认】【项目文档】【资产关联】【推测】【待编辑器验证】。

## 1. 它解决什么问题

GAS（GameplayAbilitySystem）为项目提供统一的能力、属性、状态与表现通知框架。本项目用它实现：技能激活/冷却/消耗、服务器权威伤害、属性复制、GameplayTag 状态控制、GameplayCue 表现、升级（Roguelike）的授予与叠加。

## 2. 核心类与职责

| 类 | 位置 | 职责 |
|---|---|---|
| `UArenaAbilitySystemComponent` | `Public/GAS/ArenaAbilitySystemComponent.h` | ASC 扩展：输入 Tag 路由（`AbilityInputTagPressed`）、`NotifyAbilityCommit` 路由 `Trigger.OnAbilityCast`、权威伤害事件路由（`RouteAuthoritativeDamageEvent`）、Cue 批次广播 |
| `UArenaAttributeSet` | `Public/GAS/ArenaAttributeSet.h` | 10 个复制属性 + Damage/Healing 元属性；clamp；死亡 Tag；伤害反馈入队 |
| `UArenaGameplayAbility` | `Public/GAS/ArenaGameplayAbility.h` | 玩家技能基类：`InputTag`、`CanActivateAbility` 扩展（BossIntro 门控 + 开发期网络拒绝） |
| `UArenaGameplayTags` | `Public/GAS/ArenaGameplayTags.h` | 原生 Tag 集中声明（State / Ability / Damage / Cooldown / GameplayCue / Phase / Build / Upgrade / Trigger / Status / SetByCaller） |
| `UExecCalc_Damage` | `Public/GAS/ExecCalc_Damage.h` | GE_Damage 的 ExecutionCalculation，服务器结算伤害公式（Day 4 细讲） |

## 3. 谁创建它 / 谁调用它

```text
创建：AArenaPlayerState::AArenaPlayerState()（ArenaPlayerState.cpp:13）
  ├ CreateDefaultSubobject<UArenaAbilitySystemComponent>()
  ├ SetIsReplicated(true) + SetReplicationMode(Mixed)
  ├ CreateDefaultSubobject<UArenaAttributeSet>()
  └ AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet)
调用：AArenaPlayerCharacter::InitializeAbilityActorInfo()（ArenaPlayerCharacter.cpp:127）
  └ ASC->InitAbilityActorInfo(PlayerState, this)  // Owner=PS, Avatar=Character
    ├ [Server] ApplyDefaultAttributes（GE）
    ├ [Server] GrantStartupAbilities（GiveAbility，InputTag 写入 Spec 动态源标签）
    └ BindAbilitySystemDelegates（Dead/Stunned Tag 事件、MoveSpeed 属性变化）
输入：Input_AbilityInputTagPressed（Character）→ ASC::AbilityInputTagPressed（ArenaAbilitySystemComponent.cpp:150）
  └ 遍历 GetActivatableAbilities()，匹配 AbilitySpec.GetDynamicSpecSourceTags()
  └ TryActivateAbility(Handle)
```

## 4. 生命周期

- ASC / AttributeSet 随 PlayerState 存在（跨 Pawn），角色重生只换 Avatar。
- 技能授予在服务器 `GrantStartupAbilities` 一次，由 `bGrantedStartupAbilities` 守卫（ArenaPlayerCharacter.cpp:458）。
- 属性初始化在服务器 `ApplyDefaultAttributes` 一次，由 `bAppliedDefaultAttributes` 守卫（ArenaPlayerCharacter.cpp:438）。
- Character 销毁时 `UnbindAbilitySystemDelegates`（ArenaPlayerCharacter.cpp:179），防旧 Avatar 悬挂回调。

## 5. 完整调用链

见 `Flows/Gameplay_Flow_Atlas.md` Flow A 后半段：
```text
[Local] LMB → Input_BasicAttack → Input_AbilityInputTagPressed(InputTag)
  [阶段锁/UI锁/Dead/Stunned 检查]
  → ASC::AbilityInputTagPressed → 遍历 Spec 动态源标签精确匹配
  → UAbilitySystemComponent::TryActivateAbility(Handle)
  → （Day 5/6 的 CanActivate/Commit/预测流程）
```

## 6. 数据流（GAS 部分）

| 概念 | 项目中的角色 |
|---|---|
| ASC | 状态中枢，持有 AbilitySpec、Tag、ActiveGE、属性复制；Owner=PlayerState |
| AbilitySpec | 授予的实例（`FGameplayAbilitySpec`），输入 Tag 放 `GetDynamicSpecSourceTags()` |
| GameplayAbility | 技能行为定义；`UArenaGameplayAbility` 基类带 InputTag；玩家/敌人/Boss 被动均有具体子类 |
| GameplayEffect | 数据化修改：默认属性（`DefaultAttributeEffect`）、伤害（`GE_Damage`）、升级授予（`ApplyUpgrade` 注入 SetByCaller）、恢复（`UArenaGameplayEffect_UpgradeRecovery`） |
| AttributeSet | 属性容器（BaseValue + CurrentValue），clamp 在 PreAttributeChange/PreAttributeBaseChange |
| GameplayTag | 状态/冷却/构筑/触发全部 Tag 化，原生集中声明于 ArenaGameplayTags.h |
| GameplayCue | 表现通知，Tag 在 `GameplayCue_*` 命名空间；资产扫描路径 `DefaultGame.ini` = /Game/GAS、/Game/Boss |
| Damage/Healing | 元属性，不复制；只在 `PostGameplayEffectExecute` 消费（ArenaAttributeSet.cpp:192） |

Day 1 尚未展开：TargetData / PredictionKey / CommitAbility / GameplayEvent 细节（Day 5/6/17）。

## 7. 网络角色

- Server：授予技能、应用属性、结算伤害、路由权威事件、写属性值。
- Owning Client：本地激活输入（预测路径，Day 6）、观察属性复制与 Tag 变化驱动 UI。
- Simulated Client：观察其他玩家的 ASC 复制内容（Mixed 模式允许的部分）。
- 复制模式：玩家 ASC 用 `EGameplayEffectReplicationMode::Mixed`（ArenaPlayerState.cpp:21）。

## 8. UE / GAS 原理

- 属性走 `FGameplayAttributeData`（Base/Current 双值），GE 只改 Modifier，clamp 由 AttributeSet 钩子统一做。
- `PreAttributeChange`：修改 CurrentValue 前 clamp；`PreAttributeBaseChange`：修改 BaseValue 前 clamp（Instant GE 等）。
- `PostGameplayEffectExecute`：GE 结算完成后处理元属性（Damage/Healing）、Shield 优先、死亡 Tag、反馈入队与事件路由。
- 默认属性也走 GE（ArenaPlayerCharacter.cpp:446 注释：避免绕过 AttributeSet/GAS 的统一流程）。

## 9. 为什么这样设计

1. 属性修改只允许 GE → 所有变化都经过 clamp/复制/委托/后处理钩子，杜绝 `Health -= X` 的散弹式写入。
2. Tag 化状态（State.Dead 等）→ 多个系统（移动、UI、技能门控）监听同一 Tag 计数，替代一堆 bool。
3. 输入 Tag 放 AbilitySpec → 技能"可授予、可撤回、可升级变体"，不用在 Character 里维护按键→技能映射。

## 10. 当前问题 / 边界条件

- OnAbilityCast Trigger 为 `Partial`：客户端预测、失败 commit、被动与敌人 ASC 不产生权威事件；双人 PIE 待验证【项目文档】。
- 预测网络为 `Partial`：LocalPredicted 五种技能已实现，150ms/丢包/强制回滚/双客户端 DS 验证 pending【项目文档】。
- GameplayCue 路由为 `Partial`：打包版扫描路径已配置，打包版呈现待验证【项目文档】。
- 玩家 ASC 属性复制、启动技能授予、输入 Tag 路由为 `Implemented`（源码确认）。

## 11. 如果让我修改

- 加属性：AttributeSet 加 `ARENA_ATTRIBUTE_ACCESSORS` 属性 + RepNotify + `GetLifetimeReplicatedProps`；需要时在 `ClampAttribute` 加边界。
- 加状态 Tag：`ArenaGameplayTags.h` 加 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` + cpp 定义，业务用 `AddReplicatedLooseGameplayTags` 授予。
- 改输入路由：ASC `AbilityInputTagPressed` 是通用循环，改绑定/改按任意键在 Character/Input 层。

## 12. 面试问题

见 `Interview/Questions.md`（Q4：为什么默认属性走 GE 不直接 SetHealth）。

## 13. 我的理解

（来自用户 2026-08-15 Day 1 复述，原话要点）
- GameplayEffect 提供初始化与回调；PreAttributeChange 在属性修改前做上下限限制；PostGameplayEffectExecute 在效果执行后处理扣血、死亡判定、事件派发。
- （修正：GE 是数据载体而非回调集合；真正执行在 ASC/AttributeSet；走 GE 是为统一管线 + 可配置初始值。）

## 14. 待确认

- 【待验证】OnAbilityCast 双人 PIE。
- 【待验证】预测拒绝 / 回滚（Day 12 主题）。
- 【待验证】打包版 Cue 呈现。
