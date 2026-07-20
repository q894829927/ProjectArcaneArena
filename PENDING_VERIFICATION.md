# Project Arcane Arena 待验证清单

本文档只记录当前尚未完成验证的事项，功能事实仍以 `IMPLEMENTED_FEATURES.md` 为准。

## 维护规则

1. 不使用勾选、叉号或“已完成”标记。
2. 某个条目完成测试并达到通过标准后，直接从本文档删除整个条目。
3. 测试失败或仅部分通过时保留条目，并在条目末尾补充实际现象、日志和待修复内容。
4. 出现新的待测试功能、回归风险或网络场景时，立即添加到本文档。
5. 一个功能完成完整验收后，同步更新 `IMPLEMENTED_FEATURES.md` 中的 `Partial` / `Verified` 状态。
6. 临时测试 GameMode、WaveData、UpgradePool、属性和网络参数不要覆盖正式资产。

## Boss Foundation 编译与资产生成

### 测试方法

1. 按 `AGENTS.md` 检查 `ProjectArcaneArena.uproject` 的 EngineAssociation、Live Coding 和现有构建进程。
2. 关闭 Live Coding 后，只编译 `ProjectArcaneArenaEditor Win64 Development` 窄目标并重启编辑器。
3. 执行 `Content/Python/boss/setup_boss_foundation.py`，等待脚本完整成功。
4. 关闭并重新打开 `DA_Waves_Prototype`，确认原有普通波保持原配置，最后一波为 `BP_ArenaBossCharacter × 1`、SpawnInterval `0.5`、BossWave 为 true、RewardCount 为 `0`，且重复执行不会追加第二个 Boss 波。
5. 再次执行同一脚本，检查 `/Game/Boss` 下没有 `_1`、`_2` 资产，最终 Boss 波没有重复追加。
6. 打开 `BP_ArenaBossCharacter`、`GA_BossGroundSlam`、两个 GE 和两个 GameplayCue，确认 Class 与引用均有效。

### 通过标准

- UHT 和 C++ 编译无错误，不触发 Engine 或 ShaderCompileWorker 全量重建。
- Boss 直接引用的 SkeletalMesh、AnimBP、Slam Animation 和 Niagara 位于 `/Game/Boss`，内部依赖可以继续指向已有 Skeleton、材质和贴图。
- `GA_BossGroundSlam` 正确连接 `GE_Damage`、Boss 冷却 GE 和 `AM_BossGroundSlam`。
- 重复执行脚本不会重复创建资产、波次或引用，也不会修改全部原有普通波配置。

## Boss 最终波、HUD 与 Victory 闭环

### 测试方法

1. 单人从正式 `BP_ArenaGameMode` 开始 PIE，依次清理全部普通波并完成升级选择。
2. 进入最终 Boss 波时观察 World Outliner、顶部 Boss HUD、GameState 的 `ActiveBoss` 和 `RemainingEnemyCount`。
3. 攻击 Boss，比较 Boss AttributeSet 的 Health/MaxHealth 与 HUD 文本和进度。
4. 击杀 Boss，观察死亡广播、HUD 隐藏、`ActiveBoss` 清空、普通 Pickup 数量和最终 GamePhase。
5. 临时把最终 Boss 波改成两个 Boss 条目、Count `2` 或普通敌人 Class，重新尝试开始该波；测试后恢复正式资产。

### 通过标准

- 最终 Boss 波只生成一个权威 Boss，`ActiveBoss` 指向该实例，Boss HUD 显示“悟空战将”和复制 Health。
- Boss 头顶普通敌人血条不显示；HUD 不通过 Tick 或本地变量修改 Boss 属性。
- Boss 死亡只扣减一次敌人数，不生成普通 Health/Energy Pickup，并进入 Victory。
- 无效 Boss 波在进入 Combat 前记录明确错误，不生成敌人，也不误判 Victory。

## GroundSlam 权威范围、预警与取消

### 测试方法

1. 单人靠近 Boss 到攻击距离，使用 `slomo 0.25` 观察 GroundSlam Montage、固定预警和 Impact 时机。
2. 前摇开始后让 Boss 或玩家改变位置，确认预警圆心保持在施法开始时的 Boss 脚下；分别停留圈内和走出圈外。
3. 临时在圈内放置两名玩家或使用 2-player PIE，比较每名玩家的 Shield/Health 实际损失和伤害事件数量。
4. 在前摇期间分别给 Boss 添加 `State.Stunned`、击杀 Boss，以及让锁定玩家死亡，观察 Ability/Cue 清理。
5. 在冷却期间持续靠近 Boss，确认不会重复激活；冷却结束后确认可以再次攻击。

### 通过标准

- 预警持续约 `1.2s`，中心不追踪玩家，视觉范围与 `300` 实际命中范围可清楚对应。
- 圈内合法玩家各承受一次服务器 `Damage.Physical`，走出圈外、死亡或 `State.Invincible` 玩家不受伤。
- GroundSlam 使用现有 Shield-first、Defense、Crit 和死亡管线，不直接修改 Health。
- Boss 死亡、眩晕、目标死亡或 Montage 中断后没有迟到伤害、残留预警或永久 `State.Attacking`。

## Boss Behavior Tree 阶段二 A

### 资产配置

原生编译、编辑器重启、三个 `/Game/Boss/AI` 资产、`TargetActor` Key、Blackboard 引用和固定 `GroundSlam / Chase / Wait` 图已经完成并通过运行时使用。当前只需再次执行脚本，确认没有 `_1`、`_2` 资产，且已连接的 Behavior Tree 图不会被覆盖。

### 单人 PIE

2026-07-20 复测结果：核心循环通过。Boss 能获取 `TargetActor` 并进入 Chase/MoveTo，路径朝向稳定且不会持续侧走、倒走或静止；进入范围后 GroundSlam 会中断追击，冷却期间恢复 Chase，墙体遮挡时继续寻路且不会原地停滞。

1. 墙体路径重新满足攻击条件后，确认 Decorator 能及时从 Chase 切回 GroundSlam。
2. GroundSlam 前摇期间分别施加 `State.Stunned`、击杀 Boss、切换到 Victory/Defeat，并中断 Montage。
3. 让当前玩家死亡，确认最多一个 `0.2s` Service 周期内清除或切换目标。

### 双人 Listen Server

1. 两名玩家距离 Boss 的差值小于 `150` 时交替靠近，确认 Boss 保持当前目标。
2. 让另一名玩家比当前目标至少近 `150`，确认 Blackboard 与 `CombatTarget` 同时切换。
3. 击杀当前目标，确认 Boss 选择仍存活玩家；Host/Client 只看到一棵服务器行为树、一次 GroundSlam 和一次权威伤害结算。
4. 回归普通近战和远程敌人，确认它们仍使用原 Tick AI，追击、攻击与弹道行为不变。

### 通过标准

- Boss 不在 `State.Attacking`、`State.Casting`、`State.Stunned` 或 `State.Dead` 中移动或重复激活攻击。
- Ability Task 正常结束后继续决策；Abort、终局或死亡时没有迟到 GroundSlam、残留 Delegate、Focus 或 CombatTarget。
- 墙体遮挡时不会原地反复空转，恢复攻击路径后能及时从 Chase 切回 GroundSlam。
- 两人目标切换符合 `150` 单位滞回，当前目标死亡后可靠追击存活玩家。

## Boss Charge 编译、资产与行为树

### 测试方法

1. 关闭 Live Coding 后编译窄范围 `ProjectArcaneArenaEditor Win64 Development`，重启编辑器并确认新增 Charge 原生类型与标签可见。
2. 执行 `Content/Python/boss/setup_boss_charge.py` 两次，确认没有 `_1`、`_2` 资产，`BP_ArenaBossCharacter.StartupAbilities` 中 GroundSlam 与 Charge 各一份。
3. 按 `Content/Python/boss/README.md` 在 `BT_ArenaBoss` 中连接 `GroundSlam -> Charge -> Chase -> Wait`，保存并重新打开树检查参数。
4. 检查 `AS_BossCharge` 未启用 Root Motion，`GA_BossCharge` 已连接 `AM_BossCharge`、`GE_Damage` 和 `GE_Cooldown_BossCharge`。
5. 打开三个 Charge GameplayCue，确认 Telegraph 使用 Boss 专属直线 Niagara、Active 使用 Boss 专属 Dash Aura、Impact 使用 Boss 专属 Mystic Hit。

首次脚本已成功保存 Charge 资产，但同一编辑器会话中复制模板曾让 Active/Impact 临时以 Shield/Physical 旧 Tag 注册。生成器已改为直接创建原生 Looping/Burst 子类；关闭并重启编辑器后使用修正版重跑，确认 Output Log 不再出现这两条 `AddGameplayCueData_Internal` 冲突。

### 通过标准

- C++/UHT 编译通过，脚本重复执行不会覆盖 Behavior Tree 图、重复 Ability 或创建后缀资产。
- Telegraph Cue 的世界位置不附着 Boss，方向来自 `Normal`，长度来自 `RawMagnitude`；Active Cue 附着 Boss 并与 Ability 成对结束。
- Behavior Tree 近距离优先 GroundSlam，`350-900` 中距离可选 Charge，技能冷却或条件失败时回退 Chase。

## Boss Charge 单人与双人闭环

### 单人 PIE

1. 在 `350-900` 距离触发 Charge，观察默认 `0.8s` 固定预警；前摇中横向移动，确认 Boss 仍沿锁定直线冲过原目标位置约 `150` 单位。
2. 分别测试到达终点与撞墙：到达终点不额外产生 Impact，撞墙在阻挡点产生一次 Impact 并立即停止。
3. 在路径上使用 Shield 和 Dash，确认 Shield-first 与无敌规则；让同一玩家胶囊持续位于 Sweep 中，确认一次 Charge 最多结算一次物理伤害。
4. 在前摇和冲刺期间分别让目标死亡、给 Boss 添加 `State.Stunned`、击杀 Boss、中断 Montage，并切换到 Victory/Defeat。
5. 分别在顶视角和第三人称观察预警长度、方向、速度、Active Cue 和 Impact 可读性。

2026-07-20 首轮单人结果：BT 分支顺序与两项 StartupAbilities 已确认；中距离触发、锁向冲锋、横移躲避、单目标单次命中、撞墙/终点结束、冷却回退和正常表现清理通过。原 `0.6s` 预警不够明显，高差路径会在坡顶提前停止；代码已改为默认 `0.8s` 加宽抬高预警，并忽略可行走地面 Hit，等待编译和脚本重跑后复测。`State.Attacking`/RootMotion 内部清理、异常取消、Victory/Defeat 与双人测试仍未完成；当前孤立测试环境没有自然终局入口。

### 双人 Listen Server

1. 把两名玩家放在同一 Charge 路径上，确认 Boss 贯穿两人且每人最多结算一次。
2. 让非锁定玩家进入路径挡枪，确认同样受伤但不阻挡 Boss；锁定玩家横移离线时可躲避。
3. Host 与 Client 同时观察预警、Montage、Boss 位移、Active/Impact Cue、Shield/Health 和伤害次数。
4. 在 Charge 前摇与冲刺中让当前目标死亡，确认 BT Task 结束后重新选择存活玩家。

### 通过标准

- 只有服务器执行 RootMotion Sweep 和 `GE_Damage`；两端看到同一个 Boss、同一条锁定路径和同一组 Cue。
- 每名存活玩家一次 Charge 最多被处理一次，玩家不会阻挡 Boss，世界阻挡物会终止 Charge。
- 所有正常、撞墙和取消路径均不残留 Timer、RootMotion、速度、Montage、Cue、临时碰撞响应、`State.Attacking` 或 BT Task。
- GroundSlam、Charge 与 Chase 能按距离、冷却和状态切换，不同时激活，也不会永久停滞。

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
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/setup_build_assets.py"
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

1. 编译 C++ 并重启编辑器后运行 `py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/setup_build_assets.py"`。
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
2. 单人 PIE 观察远程敌人在约 `950` 距离且实际发射路径畅通时停步、转向、播放加速 Montage，并在 Montage 进入 BlendOut 的动作结束点立即生成一个 Projectile；确认冷却约 `1.6s`。
3. 激活前用墙挡住实际发射点到角色瞄准点，确认敌人继续沿 NavMesh 绕行；攻击已 Commit 后离开原射程或移动到遮挡后，仍应在动画结束点发射，后续是否命中交给 Projectile 飞行碰撞。
4. 在前摇中击杀目标、击杀敌人或眩晕敌人，确认取消攻击且没有迟到 Projectile。
5. 发射后横向移动躲避，并让 Projectile 分别命中墙、Shield 玩家和 Dash 无敌玩家。
6. 2-player Listen Server 让未被锁定的玩家走入弹道，观察双方 Montage、Projectile、属性和伤害数字。
7. 分别用顶视角与第三人称检查弹道高度、可读性和躲避空间。

### 通过标准

- 每次有效释放只由服务器生成一个复制 Projectile，Host 与 Client 看到同一个 Actor 和销毁结果。
- Projectile 不追踪、不穿墙、不伤害敌人；任意存活玩家可挡弹，一次最多结算一次物理伤害。
- Shield 优先承伤；Dash 无敌玩家不掉血但 Projectile 仍被消耗。
- 实际发射路径被挡时 AI 不会把攻击射程当作寻路完成半径；存在导航通道时会绕到可发射位置。
- 已 Commit 的前摇不会因目标随后离开射程或出现遮挡而取消发射，Projectile 发射后也不因原目标移动而改向。
- 目标死亡、敌人死亡或 Stun 会取消前摇并阻止迟到 Projectile。
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

## Shield Build 编译与资产生成

### 测试方法

1. 关闭 Live Coding 或关闭编辑器，只编译窄目标 `ProjectArcaneArenaEditor Win64 Development`。
2. 重启编辑器后执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/setup_build_assets.py"
```

3. 连续执行生成器两次，检查 Output Log、Content Validation 和 UpgradePool。
4. 检查 `GA_Shield.ShieldEffectClass = GE_Shield_Grant`。
5. 检查 `GA_ShieldBreakBlast.DamageEffectClass = GE_Damage`。
6. 检查 `DA_Upgrade_ShieldAmount`、`DA_Upgrade_ShieldBreakBlast`、两个独立图标和 `GCN_ShieldBreak_Burst` 的字段。

### 通过标准

- UHT 和 C++ 编译通过，没有新增 Warning/Error。
- 第二次运行不重复创建资产，也不重复追加 UpgradePool。
- `GE_Shield_Grant` 的原生父类为 `ArenaGameplayEffect_ShieldGrant`。
- 两个 DataAsset 的 ID、Tags、Required Tags、数值、稀有度和 MaxStacks 与功能设计一致。
- 旧 `GE_Shield` 仍存在，但 `GA_Shield` 不再引用它。

## Shield Amount 升级与预测

### 测试方法

1. 使用 DebugStartingUpgrades 或临时缩减候选池，分别授予 `DA_Upgrade_ShieldAmount` 的 `0 / 1 / 2 / 3` 层。
2. 每次先清空当前 Shield，再按 `F` 释放一次，记录 Shield 增量、Energy 和 Cooldown。
3. 当前已有 Shield 时选择一层升级，确认选择瞬间的 Shield 不变；下一次施放再记录增量。
4. 达到三层后进入后续 Upgrade 阶段，检查候选池。
5. 在 2-player Listen Server 和约 `150ms RTT` 网络模拟下由 Client 释放 Shield，观察本地预测值和服务器确认值。

### 通过标准

- `0 / 1 / 2 / 3` 层分别增加 `30 / 36 / 42 / 48` Shield。
- 选择升级不会立即补充当前 Shield，只影响后续施放。
- 第三层后 `DA_Upgrade_ShieldAmount` 不再出现。
- Cost、Cooldown 和现有 Shield Active Cue 保持正常。
- Client 预测值最终与服务器一致，不发生永久回滚错误或重复增加。

## OnShieldBreak 事件边界

### 测试方法

1. 给玩家设置足够 Health，并分别准备 `Shield = 30`、`Shield = 10`、`Shield = 0`。
2. 分别承受小于 Shield、刚好耗尽 Shield、超过 Shield 但不致死、超过 Shield 且致死的服务器伤害。
3. 对已经为零的 Shield 继续造成伤害。
4. 使用非 Damage Meta Attribute 的 Instant GE 或调试手段把 Shield 从正数改为零。
5. 观察 `Trigger.OnShieldBreak` 次数、EventMagnitude、Target、Instigator、Damage Tags 和命中前 TargetTags。

### 通过标准

- Shield 仍大于零时不触发。
- 正数 Shield 被实际伤害耗尽且玩家存活时只触发一次。
- EventMagnitude 等于本次实际 Shield 损失，不包含 Health 损失或溢出伤害。
- Shield 原本为零、直接属性修改和致死破盾均不触发。
- 重新施放 Shield 后再次被击破，可以产生新的合法事件。

## ShieldBreakBlast 伤害与表现

### 测试方法

1. 先获得 `Build.Shield`，确认此前 `DA_Upgrade_ShieldBreakBlast` 不会出现，之后可以进入候选。
2. 获得破盾升级，在玩家周围放置距离约 `250` 和 `350` 的敌人。
3. 让服务器伤害击破 Shield，同时保证玩家存活。
4. 固定 AttackPower、CritChance 和敌人 Defense，记录爆发伤害；再分别测试 CritChance `1`、敌人 Shield、Defense 和 `State.Invincible`。
5. 让爆发击杀一名敌人，观察 `OnCrit`、`OnKill`、伤害数字和破盾 Cue。
6. 检查爆发 Damage Spec 的 Asset Tags。

### 通过标准

- 只在玩家位置执行一次 `GameplayCue.Ability.Shield.Break`。
- 300 范围内的存活敌人受伤，范围外、死亡和无敌敌人不受伤，玩家不受影响。
- 每个敌人只结算一次 `GE_Damage`，伤害包含 `Damage.Physical` 和 `Damage.Secondary`。
- 爆发继承 AttackPower、Crit、Defense、Shield-first、OnCrit 和 OnKill。
- 破盾爆发不会递归产生新的 ShieldBreakBlast。

## Shield Build 双视角与多人

### 测试方法

1. 在顶视角和第三人称分别触发一次破盾爆发，观察 Niagara 的中心和尺寸。
2. 2-player Listen Server 中只给 Client A 授予破盾升级，让敌人分别击破 A 和 B 的 Shield。
3. 再给两名玩家都授予升级，使两人分别在不同位置被破盾。
4. Host 和 Client 同时观察 Cue、伤害数字、敌人属性和服务器日志。

### 通过标准

- Burst Cue 始终以护盾拥有者 Avatar 世界位置为中心，不受相机模式影响。
- 只有拥有升级且实际被破盾的玩家触发自己的被动。
- 爆发来源 ASC、AttackPower、OnCrit 和 OnKill 归属于护盾拥有者。
- 每次破盾只在服务器结算一次，所有客户端看到同一次复制 Cue，不出现重复伤害或表现。

## Dash Build 编译与资产生成

> 当前状态：2026-07-16 已完成最新 Editor DLL 编译和首次资产生成，七个 Dash 资产均存在且 Content Validation 无错误。以下步骤保留用于后续幂等重跑和字段人工复核。

### 测试方法

1. 关闭 Live Coding 或关闭编辑器，只编译窄目标 `ProjectArcaneArenaEditor Win64 Development`。
2. 重启编辑器后执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/setup_build_assets.py"
```

3. 连续执行生成器两次，检查 Output Log、Content Validation 和 UpgradePool。
4. 检查 `GA_DashLightningTrail.DamageEffectClass = GE_Damage`。
5. 检查 `GA_DashLightningTrail.TrailAreaClass = BP_ArenaDashTrailArea`。
6. 检查 `DA_Upgrade_DashCooldown`、`DA_Upgrade_DashLightningTrail`、两个独立图标和 `GCN_DashLightningTrail_Active` 的字段。

### 通过标准

- UHT 和 C++ 编译通过，没有新增 Warning/Error。
- 第二次运行不重复创建资产，也不重复追加 UpgradePool。
- 两个 DataAsset 的 ID、Build/Upgrade Tags、RequiredTags、数值、稀有度和 MaxStacks 与设计一致。
- Trail Cue 使用 `GameplayCue.Ability.Dash.Trail`、`DO_NOT_ATTACH`、`KEEP_WORLD` 和循环闪电 Niagara。
- Cue 由每个复制 Trail Area 在各端本地成对启动/移除，不依赖玩家 ASC 上同 Tag 的批量移除。
- `BP_ArenaDashTrailArea` 的原生父类是 `ArenaDashTrailArea`，且 Ability 使用该 Blueprint Class。

## Dash Cooldown 与 OnDashEnd 边界

### 测试方法

1. 分别授予 `DA_Upgrade_DashCooldown` 的 `0 / 1 / 2 / 3` 层，记录 HUD 和 ActiveGE 中的 Dash Cooldown 总持续时间。
2. 正常完成 Dash，使用 Gameplay Debugger 或临时日志确认只发送一次 `Trigger.OnDashEnd`，并检查事件中的服务器实际起终点。
3. 分别在 TargetData 取消、`arena.Net.RejectNextAbility 3`、Dash 中途死亡和 Dash 中途 Stun 时观察事件与 Trail Actor。
4. Dash 撞墙提前停止，再检查事件终点是否为服务器最终位置而不是固定 `DashDistance` 目标点。
5. 在 `150ms RTT / 2% Loss` 和 `5% Loss` 下重复正常与拒绝路径。
6. 在高延迟下记录客户端预测 Dash 计时结束和服务器 `Trigger.OnDashEnd` 的先后关系。

### 通过标准

- 如果基础 Cooldown 为 `C`，`0 / 1 / 2 / 3` 层分别得到 `C / 0.85C / 0.70C / 0.55C`，且不低于 `0.25s`。
- 预测端和服务器最终 Cooldown 一致，HUD 读取实际 ActiveGE Duration，不残留错误冷却。
- 只有正常完成的服务器 Dash 产生一次 `Trigger.OnDashEnd`。
- 客户端预测实例先结束时不会通过 `ServerEndAbility` 提前结束权威实例，服务器仍会正常发送一次完成事件。
- 取消、拒绝、死亡和 Stun 不产生事件、Trail Actor 或 Trail Cue。
- 撞墙路径使用实际服务器起终点，Trail 不穿过玩家没有到达的墙后区域。

## Dash Lightning Trail 伤害与表现

### 测试方法

1. 先获得 `Build.Dash`，确认此前 `DA_Upgrade_DashLightningTrail` 不会出现，之后可以进入候选。
2. 获得 Trail 升级，沿直线穿过多个高血量敌人，观察 World Outliner、Cue 和伤害日志。
3. 把敌人分别放在线段中心附近、端点附近、距离路径约 `100` 和 `140` 的位置。
4. 固定 AttackPower、CritChance 和敌人 Defense，记录 `2s / 0.5s` 的伤害次数；再测试敌人 Shield、CritChance `1`、Shocked 和 `State.Invincible`。
5. 让 Trail 击杀敌人，并在拥有 Overload 时让 Trail 命中 Burning 敌人，观察 OnCrit、OnKill 和 Lightning 事件联动。
6. 在 Trail 存续期间连续 Dash，观察多个 Area/Cue 生命周期是否各自正确结束。

### 通过标准

- 每次正常 Dash 只由服务器生成一个复制 `AArenaDashTrailArea`。
- 路径半径为 `120`：严格二维点到线段距离内的敌人受伤，范围外敌人不受伤。
- 默认共结算四次伤害：生成时一次，随后每 `0.5s` 一次，单目标单 Tick 不重复。
- 每次使用 `GE_Damage + Damage.Lightning`，继承 AttackPower、Crit、Defense、Shield-first、Shocked、Hit Cue、OnCrit 和 OnKill。
- 死亡或无敌敌人不受伤；Trail 不伤害玩家或其他非敌人 Pawn。
- Burning 目标可以按现有 Lockout 规则触发 Overload，Overload 的 Secondary 伤害不会递归生成新 Overload。
- Trail Cue 位于实际路径中点，方向沿起点到终点，并在 Area 销毁时可靠移除。
- 多条 Trail 重叠时，每个 Area 的 Cue 独立结束，早销毁的路径不会清掉后生成路径的特效。

## Dash Build 双视角与多人

### 测试方法

1. 在顶视角和第三人称分别向四个方向 Dash，检查路径中点、方向和 Niagara 可读性。
2. 2-player Listen Server 只给 Client A 授予 Trail，分别由 A 和 B Dash。
3. 再让两名玩家都获得 Trail，在不同位置同时 Dash，观察 Host/Client 的 Area、Cue 和敌人属性。
4. Dedicated Server 双客户端重复一次，并打开 `arena.Net.AbilityAudit 1` 检查权威生成数量。
5. 在一个客户端切换视角、另一个客户端保持原视角时重复，确认本地相机状态不影响路径玩法数据。

### 通过标准

- Dash 方向继续来自各视角共用的规范化移动方向 TargetData，Trail 使用服务器实际移动路径。
- 只有拥有升级的玩家正常完成 Dash 时生成自己的 Trail。
- Host、Owner Client 和 Simulated Client 看到同一个复制 Area 和同一次持续 Cue。
- 每个 Area 的伤害来源、AttackPower、OnCrit、OnKill 和 Overload 归属于对应玩家 ASC。
- 视角切换不会复制相机状态，也不会增加事件、Area、Cue 或伤害次数。

## 通用升级拾取物测试关卡

### 测试方法

1. 关闭 Live Coding 或编辑器，完成一次窄范围 `ProjectArcaneArenaEditor Win64 Development` 编译并重启编辑器。
2. 打开 `/Game/Tests/Overload/Lvl_OverloadTest`，执行：

```text
py "E:/UE_DEMO/ProjectArcaneArena/Content/Python/overload_test/setup_overload_test.py"
```

3. 再执行一次脚本，检查 World Outliner 中仍只有 15 个 `UpgradePickup_*` 和 3 个 `OverloadDummy_*`。
4. PIE 中依次拾取基础升级与其依赖升级；离开碰撞球后重新进入，测试可堆叠升级直到 MaxStacks。
5. 先直接触碰缺少 RequiredTags 的升级，再补齐前置并重新触碰同一道具。
6. 双人 PIE 中让两名玩家分别拾取同一升级，检查各自 PlayerState 层数、Build Tags 和技能行为。

### 通过标准

- 15 个升级拾取物和三个木桩均贴合地面，重复运行脚本不会累积测试 Actor。
- 每个道具的放大球体、ASCII 升级名称和简短效果均可见；球体与文字颜色可区分属性/构筑，名称与描述在顶视角和第三人称中朝向各自本地相机且不严重互相遮挡。
- 玩家仍从正式 Character StartupAbilities 获得 BasicAttack、Fireball、Dash、Shield 和 LightningStorm。
- 升级只由服务器授予；成功后层数、GameplayEffect、GrantedAbility、Build Tags、生命和能量恢复与正式升级选择一致。
- 缺少 RequiredTags、命中 BlockedTags 或达到 MaxStacks 时拒绝授予，道具不消失；补齐条件后可再次拾取。
- 道具对不同玩家独立生效，不因第一名玩家拾取而销毁，也不会让客户端直接修改升级状态。
- 三个木桩保持 `5000 Health`、无 AI、无移动，并继续支持中心、250、350 距离边界测试。

## OnAbilityCast 权威事件与奥术回流数值

### 测试方法

1. 使用临时测试 GameMode 或升级拾取物，分别授予 `DA_Upgrade_EnergyOnAbilityCast` 的一至三层，并先消耗足够 Energy。
2. 分别成功施放 BasicAttack、Dash、Fireball、Shield 和 LightningStorm，记录 Commit 前后 Energy，并在 `UArenaAbilitySystemComponent::NotifyAbilityCommit` 和被动激活处设置断点。
3. 检查每个成功主动技能的 Payload：具体 Ability Tag、`Ability.Type.PlayerActive`、可选的 `Ability.Type.EnergySkill`、Instigator/Target、OptionalObject 和 EventMagnitude。
4. 将 Energy 调整到接近 MaxEnergy 后施放 EnergySkill，再把 MaxEnergy 临时设为 `0` 重复。
5. 分别在 Energy Cost 不足、Cooldown 存在、TargetData 取消和 `arena.Net.RejectNextAbility` 服务器拒绝时尝试施放。
6. 观察奥术回流被动激活期间是否再次产生 `Trigger.OnAbilityCast`，并让敌人成功 Commit 一次攻击。

### 通过标准

- 五个玩家主动技能成功 Commit 时各产生一次服务器 `Trigger.OnAbilityCast`；BasicAttack 和 Dash 不恢复 Energy。
- Fireball、Shield、LightningStorm 在一至三层时分别于 Cost 扣除后恢复 `5/10/15 Energy`，且恢复通过 `GE_Trigger_EnergyOnAbilityCast` 应用。
- Energy 不超过 MaxEnergy；MaxEnergy 为 `0` 时保持 `0`。
- Commit 失败、Cooldown/Cost 阻断、TargetData 取消和服务器预测拒绝均不恢复 Energy。
- 被动 Ability 和敌人 Ability 不产生玩家奖励，奥术回流自身不 Commit、不递归触发。
- Payload 的具体 Ability Tag 与两个 Ability Type Tag 精确反映当前技能分类。

## OnAbilityCast 两人网络归属与预测

### 测试方法

1. PIE 设置为 `2 Players`、`Play As Listen Server` 和独立窗口，分别给 Host 与 Client 授予不同层数的奥术回流。
2. 两名玩家交替施放 Fireball、Shield 和 LightningStorm，记录各自服务器 Energy 变化和客户端 HUD 更新。
3. 开启约 `150ms RTT / 2% Loss`，快速输入并在冷却边界重复施法。
4. 对 Client 使用 `arena.Net.RejectNextAbility` 依次拒绝 Fireball、Shield、LightningStorm，观察预测 Cost/Cooldown 回滚和 Energy。
5. 在一个窗口切换顶视角、另一个保持第三人称，重复一次目标技能施放。

### 通过标准

- 每名玩家只响应自己 PlayerState ASC 的 Commit 和永久升级层数，另一名玩家的 Energy 不变。
- 客户端预测不会产生第二次恢复；每次服务器确认施放只有一次权威恢复 GE。
- 服务器拒绝后不保留回能、Cost、Cooldown、Projectile 或 Area Actor，最终属性与服务器一致。
- 本地视角切换不复制相机状态，也不增加 OnAbilityCast 事件、Projectile、Area Actor 或恢复次数。

## 同帧伤害 GameplayCue RPC 合并

### 测试方法

1. 开启 `AbilitySystem.GameplayCueCheckForTooManyRPCs 1`，让 Boss 同时受到 Fireball 直接伤害、Burning 周期伤害、LightningStorm 和 Overload 中至少两种同帧结算。
2. 在单人 PIE 观察每次伤害对应的元素命中特效与普通/暴击数字，并检查 Output Log。
3. 使用 `2 Players` Listen Server 重复测试，分别从 Host 和 Client 对 Boss 造成重叠伤害。
4. 开启 `arena.Net.AbilityAudit 1`，对比服务器实际伤害结算次数、批量 Cue 标签和两端可见表现次数。
- 测试已完成，但是会出现数字重叠
### 通过标准

- 同一目标在一个 Tick 内的所有权威伤害结算只发送一个项目级批量 Multicast；每段结算仍分别携带一个命中标签、一个数字标签和独立参数。
- Output Log 不再出现并发命中与数字耗尽 `net.MaxRPCPerNetUpdate=2` 的警告，三段以上同帧伤害也不会丢失后续表现。
- Host 与 Client 均看到对应元素命中特效和一个伤害数字；暴击仍显示金色 Critical 样式。
- 合并只改变表现 RPC 数量，不改变 Health、Shield、Crit、OnDamage、OnCrit、OnKill 或 Overload 的权威结算次数。
