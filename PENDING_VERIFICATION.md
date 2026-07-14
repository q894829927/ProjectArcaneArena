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

## 构筑资产生成器幂等性

### 测试方法

1. 打开 `BP_ArenaGameMode`，记录 UpgradePool 中 `DA_Upgrade_Overload` 的数量。
2. 打开 `DA_Waves_Prototype`，记录 Waves 数量和第四波配置。
3. 在编辑器控制台再次执行：

```text
py "../../../../ProjectArcaneArena/Content/Python/setup_build_assets.py"
```

4. 等待脚本成功完成，关闭并重新打开上述两个资产。
5. 检查 Content Browser 中是否出现名称带 `_1`、`_2` 的重复 Overload 资产。

### 通过标准

- UpgradePool 中只有一份 `DA_Upgrade_Overload`。
- Waves 仍只有四项，第四波仍为 `BP_ArenaEnemyCharacter × 9`、间隔 `0.5`、非 Boss。
- `GA_Overload`、`GE_Status_OverloadLockout`、`GCN_Overload_Explosion`、`DA_Upgrade_Overload` 和图标均没有重复资产。
- 第二次执行没有 Python 异常。

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
   - Niagara：`NS_Hit_Lightning_once`
   - Attach Policy：不附着
   - Attachment Rule：Keep World
   - Scale Override：`2.5 / 2.5 / 2.5`

### 通过标准

所有字段与上述配置一致，资产可正常打开、编译和保存，没有失效 Class、None 引用或 GameplayTag 警告。

## Overload 单人完整测试环境

正式四波只有三次升级选择，单人从零开始同时获得 Burning 与 Overload 需要四次选择。因此完整单人测试使用独立测试资产。

### 测试方法

1. 复制 `DA_Waves_Prototype` 为临时 `DA_Waves_OverloadTest`。
2. 配置五波；前四波各使用一名低血量敌人，第五波使用至少三名敌人。
3. 复制或新建临时 GameMode，WaveData 指向测试 WaveData。
4. 临时 UpgradePool 只保留：
   - `DA_Upgrade_FireballDamage`
   - `DA_Upgrade_FireballBurning`
   - `DA_Upgrade_LightningStormDamage`
   - `DA_Upgrade_Overload`
5. 依次完成选择：

```text
Wave 1 后：Fireball Damage
Wave 2 后：Fireball Burning
Wave 3 后：LightningStorm Damage
Wave 4 后：Overload
Wave 5：执行战斗测试
```

6. 第五波让至少两名敌人的中心距离小于 `300`。可以调整 EnemySpawn TargetPoint，或在专用测试关卡中集中放置敌人。
7. 测试完成后切回正式 GameMode，不提交临时测试资产。

### 通过标准

- 四次升级均可按顺序取得。
- Overload 只在同时拥有 `Build.Fire` 和 `Build.Lightning` 后进入候选。
- 玩家 ASC 最终包含 `Upgrade.Combo.Overload`，并拥有 `Ability.Passive.Overload`。

## Overload 基础触发与范围

### 测试方法

1. 使用上述单人测试环境进入第五波。
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

- 四波分别生成 `3 / 5 / 7 / 9` 名敌人。
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
