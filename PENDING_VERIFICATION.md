# Project Arcane Arena 待验证清单

本文档只记录当前尚未完成验证的事项，功能事实仍以 `IMPLEMENTED_FEATURES.md` 为准。

## 维护规则

1. 不使用勾选、叉号或“已完成”标记。
2. 某个条目完成测试并达到通过标准后，直接从本文档删除整个条目。
3. 测试失败或仅部分通过时保留条目，并在条目末尾补充实际现象、日志和待修复内容。
4. 出现新的待测试功能、回归风险或网络场景时，立即添加到本文档。
5. 一个功能完成完整验收后，同步更新 `IMPLEMENTED_FEATURES.md` 中的 `Partial` / `Verified` 状态。
6. 临时测试 GameMode、WaveData、UpgradePool、属性和网络参数不要覆盖正式资产。

## 测试记录格式

每次执行测试时，在需要保留的失败条目下追加以下信息：

```text
日期：
提交/版本：
模式：Standalone / Listen Server / Dedicated Server
玩家数：
视角：Top-down / Third-person
结果：失败 / 部分通过
实际现象：
关键日志：
待处理：
```


## 掉落随机、唯一性与波次回归

### 测试方法

1. 在 `BP_ArenaGameMode` 设置固定 `UpgradeRandomSeedOverride`，记录一局中每名敌人的死亡顺序、是否掉落以及掉落类型。
2. 使用同一种子和同一敌人死亡顺序重新开局，对比掉落序列和每次 Upgrade 候选顺序。
3. 使用足够多的击杀统计总掉落比例和 Health/Energy 类型比例。
4. 临时将 Drop Chance 设为 `1.0`，对同一敌人的死亡处理设断点或观察 World Outliner，确认一次死亡最多出现一个 Pickup Actor。
5. 依次测试 `PickupDropTable = None`、Entries 为空、全部 Weight 为零、Pickup Class 为空和故意配置无法生成的 Class，完整清理波次。
6. 恢复正式掉落表配置。

### 通过标准

- 相同种子与死亡顺序产生相同掉落序列，启用掉落前后的 Upgrade 候选顺序不变。
- 大量样本中总掉率长期接近 `25%`，Health/Energy 长期接近 `1:1`。
- 每名受 WaveManager 管理的敌人每次死亡最多生成一个拾取物，手工摆放且未注册的敌人不掉落。
- 无表、空表、零权重和生成失败均只记录日志，`RemainingEnemyCount`、Upgrade、Victory 和 Defeat 流程仍正常。

## 2-player Listen Server 共享拾取

### 测试方法

1. PIE 设置为 `2 Players` 和 `Play As Listen Server`，使用独立窗口，临时把 Drop Chance 设为 `1.0`。
2. 击杀一名 WaveManager 管理的敌人，确认 Host 和 Client 在同一世界位置看到同一个拾取物。
3. 让两名都缺少对应资源的玩家同时接近，记录最先重叠的玩家和双方属性变化。
4. 让资源已满的玩家先穿过拾取物，再让缺少资源的另一名玩家穿过。
5. 生成拾取物后进入 Upgrade 或 Victory，观察其是否继续存在到被拾取或超时。
6. 在两个窗口分别切换顶视角/第三人称视角后重复。
7. 测试完成后恢复 Drop Chance 为 `0.25`。

### 通过标准

- 只有服务器生成一个复制 Pickup Actor，Host 和 Client 看到同一个生成/销毁结果。
- 首个符合条件的玩家获得恢复，另一名玩家不变；客户端不自行修改属性或销毁 Actor。
- 资源已满玩家不会抢走拾取物，另一名缺少资源的玩家仍可拾取。
- 进入 Upgrade/Victory 不会立即清除掉落物，最终由成功拾取或服务器生命期销毁。

## 构筑资产生成器幂等性

### 测试方法

1. 打开 `BP_ArenaGameMode`，记录 UpgradePool 中 `DA_Upgrade_Overload`、`DA_Upgrade_EnergyOnKill`、`DA_Upgrade_CritChance` 与 `DA_Upgrade_EnergyOnCrit` 的数量。
2. 打开 `DA_Waves_Prototype`，记录 Waves 数量和第四波配置。
3. 在编辑器控制台再次执行：

```text
py "../../../../ProjectArcaneArena/Content/Python/setup_build_assets.py"
```

4. 等待脚本成功完成，关闭并重新打开上述两个资产。
5. 检查 Content Browser 中是否出现名称带 `_1`、`_2` 的重复 Overload、EnergyOnKill 或 Crit 资产。

### 通过标准

- UpgradePool 中四项记录资产都各只有一份。
- Waves 仍只有四项，第四波仍为 `BP_ArenaEnemyCharacter × 9`、间隔 `0.5`、非 Boss。
- `GA_Overload`、`GE_Status_OverloadLockout`、`GCN_Overload_Explosion`、`DA_Upgrade_Overload` 和图标均没有重复资产。
- `GA_EnergyOnKill`、`GE_Trigger_EnergyOnKill`、`DA_Upgrade_EnergyOnKill` 和图标均没有重复资产。
- `GE_Upgrade_CritChance`、`GA_EnergyOnCrit`、`GE_Trigger_EnergyOnCrit`、两个 Crit DataAsset、两个图标与两个伤害数字 Cue 均没有重复资产。
- 第二次执行没有 Python 异常。

## Crit 资产字段与候选资格

### 测试方法

1. 编译 C++ 并重启编辑器后运行 `py "../../../../ProjectArcaneArena/Content/Python/setup_build_assets.py"`。
2. 打开 `DA_Upgrade_CritChance`，确认 ID 为 `Upgrade.Crit.Chance`，数值为 `0.05`，Common、可叠加、MaxStacks 为 `5`，Tags 包含 `Build.Crit` 与 `Upgrade.Crit.Chance`，Granted GE 为 `GE_Upgrade_CritChance`，Target Ability 为空。
3. 打开 `DA_Upgrade_EnergyOnCrit`，确认 ID 为 `Upgrade.Trigger.EnergyOnCrit`，数值为 `5`，Rare、可叠加、MaxStacks 为 `3`，Required Tags 包含 `Build.Crit`，Target/Trigger 分别为 `Ability.Passive.EnergyOnCrit` 与 `Trigger.OnCrit`，Granted Ability 为 `GA_EnergyOnCrit`。
4. 打开 `GA_EnergyOnCrit`，确认 Energy Restore Effect Class 为 `GE_Trigger_EnergyOnCrit`。
5. 打开 `GCN_DamageNumber` 与 `GCN_DamageCritical`，确认 Cue Tag 分别正确，且只有后者启用 Critical Style。
6. 使用临时 GameMode 在没有 `Build.Crit` 时多次生成候选，确认 `EnergyOnCrit` 不出现；取得一次 CritChance 后确认它可以出现。

### 通过标准

- 所有字段、Class 和 GameplayTag 均与上述配置一致，没有 None 引用、失效父类或 Tag 警告。
- CritChance 能作为构筑起点出现，EnergyOnCrit 只在已有 `Build.Crit` 后出现。
- 达到各自 MaxStacks 后对应升级不再进入候选池，且被动 AbilitySpec 只有一份。

## OnCrit 权威路由与升级数值

### 测试方法

1. 使用临时测试属性把玩家 `CritChance` 设为 `0`，分别攻击有 Health 和只有 Shield 的敌人，观察伤害事件或在 `RouteAuthoritativeDamageEvent` 设断点。
2. 把 `CritChance` 设为 `1`，重复 BasicAttack、Fireball、LightningStorm 和 Overload Secondary；对多名敌人同时命中时逐目标记录事件次数。
3. 让一次必定暴击同时击杀敌人，确认同一结算依次派发 OnDamage、OnCrit、OnKill。
4. 让 Burning Tick 完成多次伤害和击杀，确认它不带 `Damage.Critical` 且不派发 OnCrit。
5. 从默认 `5%` CritChance 开始依次取得一至五层 `DA_Upgrade_CritChance`，每层记录 AttributeSet 数值。
6. 消耗足够 Energy 后依次取得一至三层 `DA_Upgrade_EnergyOnCrit`，每层用必定暴击命中一名存活敌人并记录 Energy 变化。
7. 在 Energy 接近 MaxEnergy 和 MaxEnergy 为 `0` 时重复暴击。

### 通过标准

- CritChance 为 `0` 时永不暴击，为 `1` 时每次实际伤害都暴击，每个目标每次结算最多一个 OnCrit。
- BasicAttack、Fireball、LightningStorm、Overload Secondary 和纯 Shield 承伤都可触发；Burning 永不触发。
- 暴击击杀同时且各一次触发 OnCrit/OnKill，死亡后的重复伤害不产生新事件。
- 五层 CritChance 后属性依次为默认 `10/15/20/25/30%`，且不会超过 AttributeSet 上限。
- EnergyOnCrit 一至三层分别恢复 `5/10/15 Energy`，不超过 MaxEnergy；MaxEnergy 为 `0` 时保持 `0`。

## 暴击伤害数字与多人归属

### 测试方法

1. 单人以 CritChance `0` 命中敌人，记录每次命中的数字数量、颜色和数值。
2. 以 CritChance `1` 重复测试，确认数字为金色、约普通字号的 `1.35` 倍且没有裁切。
3. 给敌人足够 Shield，分别造成未打穿和打穿 Shield 的伤害，对比数字与 Shield/Health 实际总消耗。
4. 启动 2-player Listen Server，让 Host 与 Client 分别取得 EnergyOnCrit，并交替造成必定暴击。
5. 两个窗口分别切换顶视角和第三人称，再使用 BasicAttack、Fireball 与 LightningStorm，观察伤害、Projectile、Area Actor 和数字 Cue 数量。

### 通过标准

- 普通与暴击每次都只显示一个数字，数值等于服务器实际消耗的 Shield 加 Health；纯 Shield 伤害不再漏数字。
- 暴击数字更大且为金色，DrawSize 足够容纳文本；普通数字保持白色常规样式。
- 只有造成暴击的来源玩家恢复 Energy，另一名玩家不变化；属性由服务器修改并复制到各自 HUD。
- Host 和 Client 都只看到同一次权威数字 Cue，切换视角不会重复 Ability、Projectile、Area Actor、伤害或数字。
   
## OnKill 单人触发、堆叠与防重复

### 测试方法

1. 使用临时 GameMode，把 UpgradePool 缩减为 `DA_Upgrade_EnergyOnKill`，不要覆盖正式资产。
2. 分别取得一、二、三层升级；每次选择后用 `showdebug abilitysystem` 确认 `Upgrade.Trigger.EnergyOnKill` 和 `Ability.Passive.EnergyOnKill` 存在。
3. 每个层级先消耗足够 Energy，再用 BasicAttack 完成一次击杀，记录击杀前后 Energy。
4. 第三层后再进入升级阶段，检查该升级不再进入候选。
5. 将 Energy 调整到接近 MaxEnergy 后击杀敌人，再把测试 MaxEnergy 设置为 `0` 重复一次。
6. 准备高血量敌人，使其先承受周期或范围伤害；在敌人死亡后继续观察剩余 Burning Tick、Storm Tick 或其他范围命中。

### 通过标准

- 一、二、三层每次击杀分别恢复 `10/20/30 Energy`。
- Energy 不超过 MaxEnergy；MaxEnergy 为 `0` 时保持 `0`。
- 达到三层后升级不再出现，且 AbilitySpec 仍只有一份被动 Ability。
- 同一敌人从存活变为死亡只恢复一次，死亡后的周期、范围或重复命中不追加恢复。

## OnKill 全伤害来源与多人归属

### 测试方法

1. 单人分别使用 BasicAttack、Fireball 直接伤害、Burning 最后一跳、LightningStorm 和 Overload Secondary 完成击杀。
2. 使用一次 LightningStorm 或 Overload 同时击杀至少两名敌人，记录总 Energy 恢复。
3. 启动 2-player Listen Server，让两名玩家共同攻击同一敌人，并分别安排 Host 与 Client 完成最后一击。
4. 在来源玩家死亡后，让其先前施加的 Burning 完成击杀。
5. 开启 `arena.Net.AbilityAudit 1` 或在 `RouteAuthoritativeDamageEvent` 设置断点，检查 `Trigger.OnKill` 的 Target、EventMagnitude、TargetTags 和 EffectContext。

### 通过标准

- 每种经过 Damage Meta Attribute 的伤害都能在首次致死时派发一次 `Trigger.OnKill`。
- 一次范围伤害击杀多名敌人时，每个死亡目标分别恢复一次 Energy。
- 2-player 中只有实际最后一击来源玩家恢复，另一个玩家不获得共享奖励。
- 来源玩家已拥有 `State.Dead` 时，OnKill 仍可被路由，但 EnergyOnKill 被动不激活。
- 客户端不自行应用恢复 GE，Energy 由服务器修改并通过属性复制更新 HUD。

## Overload 资产配置

### 测试方法

逐个打开生成资产并检查 Details：

1. `DA_Upgrade_Overload`
   - Upgrade ID：`Upgrade.Combo.Overload`
   - Rarity：`Legendary`
   - Required Tags：`Build.Fire`、`Build.Lightning`
   - Upgrade Tags：`Build.Fire`、`Build.Lightning`、`Upgrade.Combo.Overload`
   - Target Ability：`Ability.Passive.Overload`
   - Trigger Event：`Trigger.OnDamageDealt.Lightning`
   - Damage Type：`Damage.Lightning`
   - NumericValue：`20`
   - Stackable：false，MaxStacks：`1`
   - Granted Ability：`GA_Overload`
2. `GA_Overload`
   - Damage Effect Class：`GE_Damage`
   - Overload Lockout Effect Class：`GE_Status_OverloadLockout`
   - Explosion Radius：`300`
3. `GE_Status_OverloadLockout`
   - Duration：`1s`
   - Stacking：`AggregateBySource`
   - Stack Limit：`1`
   - Granted Tag：`Status.Overload.Lockout`
4. `GCN_Overload_Explosion`
   - Cue Tag：`GameplayCue.Combo.Overload`
   - Niagara：`NS_Hit_Lightning_once` 与 `NS_Hit_Fire_Once`
   - Attach Policy：不附着
   - Attachment Rule：Keep World
   - Scale Override：`3.0 / 3.0 / 3.0`

### 通过标准

所有字段与上述配置一致，资产可正常打开、编译和保存，没有失效 Class、None 引用或 GameplayTag 警告。

## Overload 基础触发与范围

验证状态：**Partial Verified（2026-07-15）**。已确认 Fireball 赋予 `Status.Burning`、LightningStorm 在主木桩位置触发火焰 + 闪电爆发、250 距离木桩受伤且 350 距离木桩不受伤。

### 测试方法

1. 使用上述独立测试环境直接开始 PIE；该测试 GameMode 已关闭正式 WaveData，不需要进入指定波次。
2. 打开控制台输入 `slomo 0.2`，便于观察 Storm Tick 和爆炸 Cue。
3. 先用 Fireball 命中目标，使用 `showdebug abilitysystem` 确认目标具有 `Status.Burning`。
4. 对 Burning 目标释放 LightningStorm。
5. 观察爆炸 Cue、伤害数字、敌人 HealthBar 和周围敌人的 Health。
6. 分别把第二名敌人放在约 `250` 和 `350` 距离重复测试。
7. 在爆炸发生后再次查看原目标的 ActiveGE 和 `Status.Burning`。

### 通过标准

- Lightning 命中 Burning 目标时，在目标位置出现一次 Overload Burst Cue。
- 距离约 `250` 的敌人受到爆炸伤害，距离约 `350` 的敌人不受影响。
- 玩家和其他非敌人 Actor 不受爆炸伤害。
- 原目标的 Burning 不被消耗，持续时间和周期行为保持原规则。
- 爆炸只产生一层可观察伤害，不出现无限爆炸或瞬间递归清场。

## Overload 每来源每目标限频

### 测试方法

1. 保持 LightningStorm 的 DamageTickInterval 为 `0.5s`，目标保持 Burning 且有足够高的生命值。
2. 输入 `slomo 0.2`。
3. 观察四秒 Storm 内同一目标位置的 Overload Cue 和爆炸伤害数字。
4. 使用 `showdebug abilitysystem` 观察目标短暂持有的 `Status.Overload.Lockout`。
5. 再准备两个同时 Burning 的目标，确保两者都处于 Storm 范围内，重复测试。

### 通过标准

- 同一来源对同一目标约在 `0s、1s、2s、3s` 触发，而不是每个 `0.5s` Tick 都触发。
- `Status.Overload.Lockout` 约一秒后消失或被同来源刷新。
- 两个 Burning 目标分别维护自己的限频，任一目标的 Lockout 不阻止另一目标触发。
- Secondary 爆炸伤害不会再次激活 Overload。

## Overload 致死触发

### 测试方法

1. 准备一个 Burning 原目标和一个位于其 `300` 范围内的高血量敌人。
2. 将原目标 Health 调整到低于下一次 LightningStorm 直接伤害。
3. 释放 LightningStorm，让直接 Lightning 伤害击杀原目标。
4. 观察死亡位置的 Cue 和附近敌人的 Health 变化。

### 通过标准

- 原目标被直接 Lightning 击杀后仍产生 Overload 爆炸。
- 已死亡原目标不再承受爆炸伤害或重复死亡。
- 附近存活敌人受到爆炸伤害。
- 死亡流程、Burning/Shocked 清理、碰撞禁用和延迟销毁保持正常。

## Overload 伤害公式

### 测试方法

使用临时测试 GameplayEffect 或测试版初始化资产固定数值，不修改 C++：

1. Source：AttackPower `10`、CritChance `0`、CritDamage 保持默认。
2. Target：Defense `0`、Shield `0`、Health 足够高。
3. 触发一次未 Shocked 的 Overload，记录伤害数字和 Health 差值。
4. 让目标先获得 Shocked，再触发 Overload，记录第二组数值。
5. 把 Target Defense 改为一个明确正值，重复测试。
6. 把 Source CritChance 临时改为 `1`，重复测试。
7. 给 Target 添加足够 Shield，重复测试并分别记录 Shield、Health 变化。
8. 给 Target 添加 `State.Invincible`，再次触发。

### 通过标准

- `20 BaseDamage + 10 AttackPower`、Defense `0`、未 Shocked、不暴击时约造成 `30`。
- 目标已有 `20%` Shocked 易伤时约造成 `36`。
- Defense 能按现有公式降低爆炸伤害。
- CritChance 为 `1` 时使用现有 CritDamage 产生暴击。
- Shield 先于 Health 消耗。
- `State.Invincible` 目标不受伤。
- LightningStorm 第一次建立 Shocked 的 Tick 不让同一次首次 Overload 提前享受易伤；后续 Tick 可以享受。

## 通用权威伤害事件载荷

该层目前没有正式调试 UI，使用 Visual Studio Debugger 验证载荷。

### 测试方法

1. 使用 Development Editor 启动项目，并让 Visual Studio 附加到 `UnrealEditor.exe`。
2. 在 `UArenaAbilitySystemComponent::RouteAuthoritativeDamageEvent` 调用 `HandleGameplayEvent` 前设置断点。
3. 分别使用 BasicAttack、Fireball、Burning Tick、LightningStorm 和 Overload 爆炸造成伤害。
4. 每次命中检查：
   - `DamageEventTag`
   - `EventPayload.EventMagnitude`
   - `EventPayload.Instigator` / `Target`
   - `InstigatorTags`
   - `TargetTags`
   - `OptionalObject` / `OptionalObject2`
   - `ContextHandle`
5. 给目标设置 `Shield = 10`、`Health = 5`，施加明显大于 `15` 的伤害，检查 EventMagnitude。
6. 让 Burning 目标被致死 Lightning 命中，检查断点中的命中前 TargetTags。
7. 分别测试 `State.Invincible` 和无剩余 Shield/Health 的目标，确认不会产生实际伤害事件。

### 通过标准

- BasicAttack 路由 `Trigger.OnDamageDealt.Physical`。
- Fireball 和 Burning 路由 `Trigger.OnDamageDealt.Fire`。
- LightningStorm 与 Overload 爆炸路由 `Trigger.OnDamageDealt.Lightning`。
- Overload 载荷的 InstigatorTags 包含 `Damage.Lightning` 和 `Damage.Secondary`。
- EventMagnitude 等于实际消耗的 Shield + Health；上述过量伤害示例应为约 `15`，不包含溢出量。
- 致死 Lightning 的 TargetTags 仍包含命中前 `Status.Burning`。
- OptionalObject、SourceObject 和 EffectContext 与原 Damage Spec 对应。
- 无敌或零实际资源损失不会产生可触发被动的事件。

## 2-player Listen Server Overload

### 测试方法

1. PIE 设置为 `2 Players`、`Play As Listen Server`，使用独立窗口。
2. 使用五波测试 GameMode，让两名玩家都依次取得 Fireball Damage、Fireball Burning、LightningStorm Damage、Overload。
3. 由两名玩家先后在小于一秒的时间内，用 LightningStorm 命中同一个 Burning 高血量目标。
4. 分别观察 Host、Client 的 Cue、伤害数字和目标 Health。
5. 在每个窗口独立切换顶视角/第三人称后重复一次。
6. 使用 World Outliner 或网络审计确认每次施法只有一个权威 LightningStorm Area。

### 通过标准

- 每个玩家只有拥有自己的 Overload 后才能由自己的 Lightning 触发。
- 两个来源能在一秒内分别触发一次，不共享同一个来源 Lockout。
- 伤害只由服务端结算，不出现双倍扣血或重复伤害数字。
- Host 和 Client 都看到同一次权威 Burst Cue。
- 爆炸不伤害任一玩家。
- 本地视角切换不复制输入，也不增加 Area、事件或爆炸数量。

## 远程敌人最小战斗闭环

### 测试方法

1. 编译并重启编辑器后运行 `Content/Python/ranged_enemy/setup_ranged_enemy.py` 两次，确认资产创建与重复执行都成功。
2. 单人 PIE 观察远程敌人在约 `950` 距离停步、转向、播放 Montage，并在约 `0.45s` 后生成一个 Projectile；确认冷却约 `1.6s`。
3. 在前摇中离开射程、躲到墙后、击杀或眩晕敌人，确认攻击打空且没有迟到 Projectile。
4. 发射后横向移动躲避，并让 Projectile 分别命中墙、Shield 玩家和 Dash 无敌玩家。
5. 2-player Listen Server 让未被锁定的玩家走入弹道，观察双方 Montage、Projectile、属性和伤害数字。
6. 分别用顶视角与第三人称检查弹道高度、可读性和躲避空间。

### 通过标准

- 每次有效释放只由服务器生成一个复制 Projectile，Host 与 Client 看到同一个 Actor 和销毁结果。
- Projectile 不追踪、不穿墙、不伤害敌人；任意存活玩家可挡弹，一次最多结算一次物理伤害。
- Shield 优先承伤；Dash 无敌玩家不掉血但 Projectile 仍被消耗。
- 前摇失效不会生成 Projectile，发射后的 Projectile 不因原目标移动而改向。
- 目标死亡后 AI 取消前摇并重新选择最近的存活玩家。

## 四波正式流程

### 测试方法

1. 恢复正式 `BP_ArenaGameMode` 和 `DA_Waves_Prototype`。
2. 使用 Standalone 完整清理四波，记录每波生成数和阶段变化。
3. 检查 Wave 1、2、3 后进入 Upgrade；完成选择后才进入下一波。
4. 检查 Wave 4 清理后的最终阶段。
5. 使用临时缩减 UpgradePool 的四波测试副本，依次选择 Fireball Damage、LightningStorm Damage、Overload，验证第三次升级资格。
6. 2-player Listen Server 再执行一次，故意让一名玩家延迟选择。
7. 使用空 UpgradePool 或让全部候选达到 MaxStacks，清理非最终波并观察错误日志、资源恢复和阶段推进。
8. 在 2-player Upgrade 阶段让迟加入玩家没有符合 RequiredTags 的候选，检查该玩家自动完成后的全员门槛。

### 通过标准

- 四波分别生成 `3M / 3M+2R / 4M+3R / 5M+4R`，总数仍为 `3 / 5 / 7 / 9`。
- 前三波清理后进入 Upgrade，第四波清理后直接进入 Victory。
- Overload 在双构筑条件满足后可以进入第三次候选。
- Overload 达到 MaxStacks 后不再出现。
- 升级、Build Tags 和被动 Ability 跨波次保留。
- 2-player 中必须全员完成选择才进入下一波。
- 无候选玩家不显示选择 UI、不获得升级，但会恢复 Health/Energy 并且不会阻塞下一波。
- Upgrade 阶段迟加入且无候选的玩家完成后，服务器会立即重新检查全员选择状态。

## Fire Build 回归

### 测试方法

1. 使用固定 AttackPower、Defense 和 CritChance，分别测试 Fireball Damage `0 / 1 / 2 / 3` 层。
2. 对高血量目标连续施加 Burning，使用 `showdebug abilitysystem` 查看 StackCount、Duration 和 Period。
3. 给目标添加 Shield，观察四次 Burning Tick。
4. 在 Burning 到期和目标死亡两种情况下观察状态 Tag 与持续 Cue。
5. 在 2-player PIE 中让两名玩家分别对同一目标施加 Burning。
6. 给目标添加 `State.Invincible` 后用已解锁 Burning 的 Fireball 命中，观察投射物和目标 ActiveGE。

### 通过标准

- Fireball 技能倍率为 `1.0 / 1.2 / 1.4 / 1.6`。
- Burning 每来源最多三层，单层每跳 `5`，重复命中刷新持续时间和周期。
- Burning 伤害先消耗 Shield，并尊重 Dead/Invincible。
- Fireball 命中 `State.Invincible` 目标后正常销毁，但不新增或刷新 Burning ActiveGE。
- 到期或死亡后 ActiveGE、`Status.Burning` 和 Cue 全部清理。
- 两名玩家的 Burning 按来源独立维护。

## Lightning Build 回归

### 测试方法

1. 使用固定属性分别测试 LightningStorm Damage `0 / 1 / 2 / 3` 层。
2. 对无 Shocked 目标释放 Storm，记录第一 Tick 和后续 Tick 伤害。
3. 重复命中并使用 `showdebug abilitysystem` 观察 Shocked StackCount 和 Duration。
4. 停止 Storm，确认 Shocked 从最后一次命中继续保留约四秒。
5. 在目标死亡和状态自然到期时观察 Cue 清理。
6. 2-player 中由玩家 A 施加 Shocked，玩家 B 用 LightningStorm 命中同一目标。

### 通过标准

- LightningStorm 技能倍率为 `1.0 / 1.2 / 1.4 / 1.6`。
- 第一 Tick 先按基础伤害结算再建立 Shocked，后续 Lightning 获得约 `20%` 易伤。
- Shocked 不叠层，重复命中只刷新四秒持续时间。
- 死亡或到期后 ActiveGE、`Status.Shocked` 和 Cue 全部清理。
- 玩家 B 可以利用玩家 A 创建的共享 Shocked 易伤。

## 双视角主动技能回归

### 测试方法

1. 在顶视角依次测试 WASD、BasicAttack、Fireball、Dash、Shield、LightningStorm。
2. 按 `0` 或 `NumPad0` 切换第三人称，重复全部技能。
3. 在移动、技能冷却、Storm 存续和 Fireball 飞行期间切换视角。
4. 2-player PIE 中让两名玩家使用不同视角同时战斗。

### 通过标准

- 顶视角使用鼠标地面目标，第三人称使用屏幕中心目标。
- 第三人称射线不会首先命中自己的 Mesh 或 Capsule。
- 切换视角不会重复 Ability、Projectile、Area Actor、Damage 或 GameplayCue。
- 两名本地玩家的视角状态互不影响，战斗结果仍由服务器决定。

## 升级随机种子网络一致性

### 测试方法

1. 启动 2-player Listen Server，记录 Host 和 Client HUD 右上角种子。
2. 结束 PIE 后重新启动一局，记录新种子。
3. 设置固定 `UpgradeRandomSeedOverride`，连续启动两局并记录候选顺序。
4. 使用额外客户端在会话开始后加入，检查 Late Join HUD 种子。

### 通过标准

- 同一会话的 Host、Client 和 Late Join 显示完全相同的种子。
- 默认 Override 为 `0` 时，不同会话通常生成不同种子。
- 固定正数 Override 时，多次会话产生相同种子和确定性候选序列。
- 随机数只由服务器候选流消费，客户端不自行决定升级结果。

## 预测拒绝与高延迟网络

### 测试方法

1. PIE 设置为 2-player Listen Server，启用 Network Emulation。
2. 先使用约 `150ms RTT / 2% Loss`，再提高到 `150ms RTT / 5% Loss`。
3. 对五个主动技能分别快速按键、长按和在冷却边界重复输入。
4. 依次设置 `arena.Net.RejectNextAbility`：

```text
1 BasicAttack
2 Fireball
3 Dash
4 Shield
5 LightningStorm
```

5. 打开 `arena.Net.AbilityAudit 1`，记录服务器执行序列、Projectile 和 Area 数量。
6. 对每个技能检查拒绝后的 Montage、Cooldown、Cost、状态 Tag 和生成 Actor。

### 通过标准

- 每次成功施法只有一次服务端权威伤害、Projectile 或 Area。
- 预测拒绝不会残留 Montage、Cooldown、Cost、Projectile、Area、`State.Dashing` 或 `State.Invincible`。
- 网络抖动下没有永久输入锁、重复伤害或客户端独立决定结果。
- 客户端表现可以延迟或回滚，但最终与服务器一致。

## Dedicated Server 回归

### 测试方法

1. PIE 设置为 Dedicated Server，并启动两个客户端。
2. 完成移动、五个主动技能、敌人攻击、死亡、升级选择和至少两波推进。
3. 使用 Overload 测试配置验证 Burning、Lightning、爆炸和 Lockout。
4. 检查 Dedicated Server 不创建 HUD、本地伤害数字或仅表现用 Actor。

### 通过标准

- Dedicated Server 独占伤害、敌人 AI、波次、升级校验和 Overload 状态应用。
- 两个客户端只显示各自 HUD 和复制表现。
- 不出现重复 Projectile、Storm Area、Overload Damage 或 Cue。
- 玩家属性、敌人死亡、GameState 阶段和全员升级门槛复制一致。
