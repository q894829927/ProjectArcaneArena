# Project Arcane Arena Boss 开发阶段规划

## 文档职责

本文档同时承担 Boss 阶段开发规范与阶段进度摘要，用于约束开发顺序、职责边界、验收门槛，并记录每个阶段已经实际完成的部分。

- `IMPLEMENTED_FEATURES.md` 仍是全项目功能实现状态的规范记录；本文档只维护 Boss 各阶段的范围内进度和进入下一阶段的条件。
- 尚未完成的运行时、多人或表现验证记录到 `PENDING_VERIFICATION.md`。
- Boss 功能发生实现、移除、职责迁移或验证状态变化时，应在同一变更中同步更新本文档对应阶段的“当前实现进度”。
- 只有已经存在于代码或已保存项目资产中的内容才能列为已完成；仅有方案、未成功执行的生成步骤或待验证推测不得描述为已完成。
- 使用 `Planned`、`Partial`、`Implemented`、`Verified` 描述阶段状态，不使用勾选框维护完成状态。
- 默认完成当前阶段的验收标准后再进入下一阶段；用户明确调整范围时，可以修改阶段顺序，但必须同步更新对应边界。

当前开发阶段为“阶段二 B：Boss Charge”，状态为 `Partial`。阶段一已完成关键双人闭环验收，阶段二 A 的单人核心追击/攻击循环已通过；Charge 分支已经接入 Behavior Tree 并完成首轮单人核心验收，仍需重编译复测预警与高差修正、异常取消和双人权威行为。

---

## 共同架构边界

所有 Boss 阶段都必须遵守以下规则：

- Boss、技能、伤害、阶段转换、区域效果和召唤物均由服务器决定。
- Boss 属性继续由 ASC、AttributeSet 和 GameplayEffect 管理，不直接修改 Health、MaxHealth、AttackPower 等属性。
- Boss 伤害继续通过 `GE_Damage` 和现有伤害执行管线结算。
- Boss 由服务器 WaveManager 生成，移动和必要状态复制给客户端。
- Boss 状态通过 GameState、ASC Attribute Delegate 和 GameplayTag 向客户端公开，UI 不拥有玩法状态。
- Boss 相机和 HUD 动画属于本地表现，不复制相机 Transform 或本地视角状态。
- Boss AI 只负责选择和激活 GameplayAbility，不直接修改属性或制造伤害。
- GameplayTag 负责阶段、攻击、施法、冷却和状态阻断，不增加分散的重复布尔状态。
- Boss 死亡、Ability 取消、阶段结束和 Actor 销毁时必须清理 Timer、Delegate、Root Motion、Area Actor、GameplayCue 和召唤物。
- 顶视角与第三人称使用相同的服务器战斗结果；预警范围和 VFX 必须在两种视角下可读。
- 不为 Boss 添加全面元素免疫或构筑免疫，避免让已有 Fire、Lightning、Crit、Shield 和 Dash 构筑整体失效。

### 决策约束

- Boss 多技能决策固定采用 UE Behavior Tree（行为树）作为唯一主实现，不得增加并行的技能调度器。
- EQS 只用于选择 Charge 路径、FireZone 位置或召唤点等空间结果，不用于替代简单的最近存活玩家选择。
- Boss 召唤物默认不加入 WaveManager 的 Boss 胜利计数；Boss 本体死亡是 Boss Wave 的完成条件。
- Boss Intro 相机只由本地 `AArenaPlayerController` 执行；服务器只同步 Intro 阶段、Boss 引用和必要时序。
- Boss 与构筑系统的特殊联动继续通过 Ability、Damage、Status、Build 和 Trigger GameplayTag 查询，不硬编码 Upgrade ID。

---

## 阶段一：Boss Foundation

### 阶段目标

完成一个最小但完整的 Boss 战斗闭环：Boss 可以作为最终波生成、追击和攻击玩家，拥有独立 HUD，并在死亡后可靠进入 Victory。

### 开发内容

- 新增 `AArenaBossCharacter`，继承 `AArenaEnemyCharacter`，复用现有 ASC、AttributeSet、复制移动、默认属性应用和死亡流程。
- 创建 `GE_Init_BossAttributes` 与 `BP_ArenaBossCharacter`，Boss 基础属性由资产配置。
- 让 `FArenaWaveConfig::bBossWave` 参与 WaveManager 流程，并在现有普通波次后追加最终 Boss Wave。
- 在 `AArenaGameState` 复制 `ActiveBoss`，提供变化委托，Boss 生成和死亡时由服务器设置或清空。
- 扩展玩家 HUD，显示 Boss 名称、Health、MaxHealth，并在没有 Boss 或 Boss 死亡时隐藏。
- 实现 `GA_Boss_GroundSlam`，继续使用服务器权威的 `GE_Damage`，提供固定位置的范围预警和范围伤害。
- GroundSlam 使用 `State.Attacking` 阻止并行攻击，并在死亡、眩晕或取消时清除尚未兑现的伤害与表现。
- 最终 Boss 死亡后复用 WaveManager 的完成检测进入 Victory，不建立第二套胜利规则。

### 当前实现进度

状态：`Partial`，最后更新：2026-07-20。

已完成实现：

- 已新增 `AArenaBossCharacter`，复用敌人 ASC、AttributeSet、服务器 AI、复制移动、伤害反馈和标签驱动死亡流程，并默认关闭头顶普通敌人血条。
- 已新增 `UArenaGameplayEffect_BossAttributes` 与 `UArenaGameplayEffect_BossGroundSlamCooldown`，提供第一阶段 Boss 属性和 GroundSlam 冷却配置。
- 已新增 `Ability.Enemy.Boss.GroundSlam`、`Cooldown.Enemy.Boss.GroundSlam` 以及 Telegraph、Impact GameplayCue 标签。
- 已实现 `UArenaGameplayAbility_BossGroundSlam`：服务器锁定固定圆心、显示持续预警、按范围过滤存活玩家，并通过独立 `GE_Damage` Spec 结算 `Damage.Physical`。
- 已让 WaveManager 验证 Boss 波必须只有一个 `AArenaBossCharacter`，设置和清理 `ActiveBoss`，跳过普通 Pickup DropTable，并继续复用最终波 Victory 流程。
- 已在 `AArenaGameState` 复制 `ActiveBoss`，由本地 `AArenaPlayerController` 将玩家 HUD 绑定到 Boss ASC。
- 已扩展 `UArenaPlayerHUDWidget`，通过 Health/MaxHealth Attribute Delegate 和 `State.Dead` 显示或隐藏 Boss HUD，并在蓝图缺少控件时创建顶部备用血条。
- 已新增 `Content/Python/boss/setup_boss_foundation.py` 及说明文档，支持保留现有普通波、幂等追加或更新唯一最终 Boss 波。
- 已将 `/Game/Boss` 加入 GameplayCue 扫描路径，并完成 Boss 原生类型的 UHT/编辑器加载链路。
- 已完成 2-player Listen Server 运行时验收：Host/Client 只观察到一个权威 Boss，两名玩家的 GroundSlam 范围分别正确结算，当前目标死亡后 Boss 能重新追击存活玩家，两端 Boss Health、Cue、死亡和 Victory 状态一致。
- 已完成双视角可读性验收：顶视角和第三人称均能清楚判断 GroundSlam 的预警范围与兑现时机。

尚未完成或尚未验证：

- 修复 `BossDisplayName` 继承 CDO 的只读本地化写入后，仍需重新完整执行 Boss 资产脚本并确认所有 Blueprint、GE、Cue、动画和最终 Boss 波均已保存。
- 尚未完成脚本连续执行两次的幂等验证。
- 尚未完成单人完整波次和无效 Boss 波配置防线验证。
- 尚未完成 GroundSlam 在 Boss 眩晕、前摇中死亡、Montage 中断或目标死亡时的取消清理验证。
- `PENDING_VERIFICATION.md` 中的 Boss 条目完成前，本阶段不得标记为 `Verified`。

### 阶段边界

本阶段不实现多阶段、狂暴、`GA_Boss_Charge`、FireZone、召唤小怪、Behavior Tree、EQS、Boss Intro、专属掉落、动态多人血量缩放或 Boss 专属构筑升级。

### 完成标准

- 最终波只生成预期数量的 Boss，生成失败不会误判 Victory。
- Boss 可以在服务器选择最近的存活玩家、追击并释放 GroundSlam。
- GroundSlam 的预警中心、预警半径、实际命中范围和伤害时机一致。
- Boss Health 通过 GAS 正常变化，Boss HUD 只观察复制状态并及时显示或隐藏。
- Boss 死亡只处理一次，清理攻击状态，并使最终波进入 Victory。
- Listen Server 中只存在一个权威 Boss，所有客户端看到一致的移动、血量、预警、伤害和销毁结果。
- GroundSlam 在顶视角和第三人称下均能清楚判断危险范围。

### 进入下一阶段的前置条件

Boss 单技能闭环、ActiveBoss 复制、Boss HUD、最终波 Victory 和死亡清理均达到本阶段完成标准，且相关未完成测试已记录到 `PENDING_VERIFICATION.md`。

---

## 阶段二：Boss 决策层与多技能

### 阶段目标

建立由 `AArenaBossAIController` 驱动的 Behavior Tree 与 Blackboard，使 Boss 能根据距离、冷却和空间条件稳定选择 GroundSlam、Charge 和 FireZone。

### 开发内容

- 新增 `AArenaBossAIController`，仅在服务器运行 `BT_ArenaBoss` 与 `BB_ArenaBoss`，并负责启动、停止和清理行为树。
- Blackboard 只保存行为树决策所需的目标和空间结果；死亡、攻击、施法、阶段和冷却状态继续以 ASC GameplayTag 为准，不复制为重复布尔状态。
- Behavior Tree 通过 C++ Service 维护最近的存活玩家目标，通过 C++ Decorator 检查阶段、距离、GameplayTag 与 Ability 可激活条件。
- Behavior Tree 通过统一的 C++ Task 按 AbilityTag 请求 ASC 激活 Boss Ability，并等待 Ability 正常结束或取消后再继续选择。
- Behavior Tree 只负责选择、移动和激活 Ability，不直接处理伤害、属性或 GameplayEffect。
- 实现 `GA_Boss_Charge`：服务器锁定目标位置、停止寻路、显示直线预警、执行 Root Motion 冲刺，并通过 Sweep 对每名玩家最多结算一次伤害。
- Charge 遇到墙体、到达终点、Boss 死亡、眩晕或 Ability 取消时，立即结束 Root Motion 并恢复统一状态。
- 实现 `GA_Boss_FireZone` 和服务器生成的复制 Area Actor，使用 Timer 周期应用 `Damage.Fire`。
- FireZone 的客户端只显示范围与 VFX，服务器负责目标过滤、周期伤害和生命周期。
- 使用 `State.Attacking`、`State.Casting` 和 Ability 冷却防止技能重叠，并统一处理取消与恢复寻路。
- EQS 只用于得到合法的 Charge 方向、FireZone 位置或后续召唤点，不替代 Behavior Tree 的目标选择与 Ability 激活。

### 当前实现进度

状态：`Partial`，最后更新：2026-07-20。

已完成实现：

- 已新增服务器专用 `AArenaBossAIController`，Boss 原生默认 Controller 不再使用普通敌人的低频 Tick 决策；普通近战与远程敌人保持现有 `AArenaEnemyAIController`。
- 已明确 Boss 移动朝向职责：Behavior Tree 保留原有 `Move To` 路径行为，关闭角色 Controller Yaw，`CharacterMovement` 以 `720 deg/s` 面向路径速度；进入攻击 Task 时仍立即水平朝向校验目标。
- Controller 仅在 `Combat` 阶段且 Boss 未死亡、未眩晕时启动配置的 Behavior Tree；死亡、眩晕、终局、`UnPossess` 和 `EndPlay` 会停止 Brain/寻路、清除 Focus、Blackboard `TargetActor`、`CombatTarget` 并取消当前主攻击。
- 已新增 `UBTService_ArenaBossUpdateTarget`，每 `0.2s` 从 `GameState.PlayerArray` 选择最近存活玩家，并以 `150` 单位距离优势作为切换滞回；攻击期间锁定仍存活的当前目标，目标失效时先取消旧攻击，再同步 Blackboard 与 Boss `CombatTarget`。
- 已新增 `UBTDecorator_ArenaBossCanActivateAbility`，按精确 AssetTag 查找唯一 AbilitySpec，检查 Combat 阶段、目标、Boss 状态、攻击距离、攻击路径以及 GAS `CanActivateAbility()`；条件变化时支持中断低优先级 Chase。
- 已新增实例化 `UBTTask_ArenaBossActivateAbility`，按精确 Spec Handle 激活 Ability 并等待对应 `OnAbilityEnded`；Abort 时先解绑再取消 Spec，并处理 Ability 在激活调用内同步结束的边界。
- 已新增 `Content/Python/boss/setup_boss_decision.py`，用于幂等创建 `BP_ArenaBossAIController`、`BB_ArenaBoss`、`BT_ArenaBoss` 资产外壳，连接 Python 反射层可访问的 Controller/Boss/BehaviorTree 引用，且不覆盖手工 Behavior Tree 图；UE Python 未导出 Blackboard Key 类型或 `BehaviorTree.BlackboardAsset` 时会保留对应手动配置入口，不再中止整个脚本。
- 已在 `Content/Python/boss/README.md` 记录阶段二 A 固定树结构、Decorator Abort 配置、MoveTo 参数与手工连接步骤。
- 已成功执行 `setup_boss_decision.py` 并保存 `BP_ArenaBossAIController`、`BB_ArenaBoss`、`BT_ArenaBoss` 与更新后的 `BP_ArenaBossCharacter`；当前 UE Python 未导出 Blackboard Key 和 BehaviorTree Blackboard 引用，因此这两项按文档手工配置。
- 已完成阶段二 A 的单人核心循环验收：`TargetActor` 能驱动 Chase/MoveTo，Boss 路径朝向稳定，GroundSlam 在进入范围后中断追击，冷却期间恢复 Chase，墙体遮挡时继续 NavMesh 寻路且不会原地停滞。
- 已为 `UArenaGameplayAbility_EnemyAttackBase` 增加可选最小攻击距离和可复用的服务器校验、Commit、目标缓存及 `State.Attacking` 初始化入口，既有近战、远程和 GroundSlam 仍走原生命周期。
- 已让 `UBTDecorator_ArenaBossCanActivateAbility` 同时检查攻击最小/最大距离，并让 `AArenaEnemyCharacter::CancelPrimaryAttack()` 取消全部活跃 `EnemyAttackBase` Spec，支持多技能 Boss 的异常清理。
- 已实现 `UArenaGameplayAbility_BossCharge`：服务器在 Commit 时锁定方向和终点，先显示默认 `0.8s` 固定直线预警，再使用 GAS RootMotion 直线冲锋；路径 Sweep 可分别命中多个存活玩家且每人最多一次，Pawn 不阻挡冲锋，墙体阻挡会立即结束。
- Charge 伤害继续使用 `GE_Damage + Damage.Physical`；Shield、Defense、Crit、Dash 无敌和死亡判定继续由现有权威伤害管线处理，不在 Ability 中直接修改属性。
- 已新增 Charge Ability、Cooldown 与 Telegraph/Active/Impact GameplayCue 原生标签，以及按固定位置、方向和实际距离缩放 Niagara 的 `AArenaGameplayCueNotify_BossChargeTelegraph`。
- Charge 的 Timer、RootMotion、Montage、Cue、临时 Pawn 碰撞响应、速度、命中缓存和 `State.Attacking` 均由 Ability 结束路径统一清理；Stun、死亡、终局和 BT Abort 会复用取消路径。
- 已新增幂等 `Content/Python/boss/setup_boss_charge.py`，用于创建 Charge 动画副本、关闭动画 Root Motion、创建 Montage、GA/GE/Cue、复制 Boss 专属 Niagara，并向 Boss `StartupAbilities` 追加且只保留一份 Charge；脚本不会修改 Behavior Tree 图。
- 已成功完成首次 `setup_boss_charge.py` 执行并保存 `AM_BossCharge`、`GA_BossCharge`、三个 Charge GameplayCue 与更新后的 `BP_ArenaBossCharacter`；Content Validation 已启动且日志没有脚本异常。
- 首次生成期间 Active/Impact Cue 曾在改写 Tag 前被编辑器临时按 Shield/Physical 模板 Tag 注册；脚本现已改为直接从原生 Looping/Burst 类创建新 Cue，并显式保存冷却 GE，现有资产需重启编辑器后重跑确认注册结果。
- 已在 `Content/Python/boss/README.md` 记录 `GroundSlam -> Charge -> Chase -> Wait` 的手工接线顺序和 Charge 节点参数。
- 已完成 Charge 单人核心行为的首轮验收：行为树分支与 StartupAbilities 配置正确，中距离能进入锁向冲锋，玩家可横移躲避，每名玩家最多受伤一次，撞墙和到达终点均会结束，冷却期间回退 Chase/GroundSlam，正常结束未观察到速度或表现残留。
- 已使用 AbilitySystem Debug Target 验证正常 Charge 生命周期：冲锋期间存在 `State.Attacking`，结束后标签消失，Boss 随后恢复 Chase/Attack，未残留 RootMotion 或 BT Task 阻塞。
- 根据首轮验收反馈，Charge 碰撞结束逻辑已改为忽略 `CharacterMovement` 判定为可行走的地面 Hit，避免斜坡/台阶被误认为墙；Telegraph 默认加宽、抬高并延长到 `0.8s`，等待重新编译与资产脚本同步后复测。

尚未完成或尚未验证：

- 尚未验证当前目标死亡后的 `0.2s` 内重选，以及 GroundSlam 在 Stun、Boss 死亡、终局和 Montage 中断时的 Task/Ability/Focus 清理。
- 尚未单独记录墙体路径重新满足攻击条件后，Decorator 能及时从 Chase 切回 GroundSlam。
- 双人 `150` 单位目标切换滞回、当前目标死亡后的存活玩家重选，以及单次权威 GroundSlam 仍待 PIE 验收。
- 尚未验证 `setup_boss_decision.py` 连续执行不会生成重复资产或覆盖已连接的 Behavior Tree 图。
- 修正版脚本重跑、加强后的 Telegraph 可读性、斜坡高差移动、Stun/死亡/终局/Montage 中断清理和多人 Sweep 尚未验证。
- FireZone 与 EQS 尚未开始，本阶段不能标记为 `Implemented` 或 `Verified`。

### 阶段边界

本阶段所有技能对 Boss 全程可用，不根据 Health 解锁；不实现阶段切换、狂暴、动态血量缩放、召唤物、构筑特殊联动、开场镜头或专属掉落。

### 完成标准

- Boss 不会同时激活两个互斥攻击，也不会在 `State.Attacking` 或 `State.Casting` 中恢复寻路滑动。
- Behavior Tree 能在 GroundSlam、Charge 和 FireZone 之间选择，并在技能不可激活时回退到追击或其他可用分支，不会永久停滞。
- Charge 的预警、路径、墙体碰撞和实际命中一致，同一玩家在一次 Charge 中最多受伤一次。
- FireZone 只由服务器结算伤害，复制 Actor、GameplayCue 和周期 Timer 不会重复生成或残留。
- Boss 死亡或战斗进入 Victory/Defeat 时，所有运行中的技能、路径、Area Actor 和决策任务正确停止。

### 进入下一阶段的前置条件

`AArenaBossAIController`、`BT_ArenaBoss` 和 `BB_ArenaBoss` 已经稳定承担所有技能选择，三个 Boss 技能的服务端结算、取消边界和双视角表现达到完成标准。

---

## 阶段三：多阶段、狂暴与多人缩放

### 阶段目标

将已有技能组织成清晰的 Boss 战节奏，并根据参战玩家数量在 Boss 生成时确定一次性属性缩放。

### 开发内容

- Boss 在服务器监听 Health Attribute Delegate，使用单向且防重入的阈值转换阶段。
- 默认阶段划分为：Phase 1 使用 GroundSlam；Phase 2 解锁 Charge 与 FireZone；Phase 3 进入 Enraged。
- 使用 `Boss.Phase.*` GameplayTag 作为 Ability 激活条件和客户端表现来源。
- 使用 Infinite `GE_Boss_Enrage` 修改 AttackPower、MoveSpeed 或其他明确配置的 GAS 属性。
- 使用 GameplayCue 表现阶段转换和 Enraged 状态，Boss 死亡时自动清理。
- Boss 生成时由服务器统计参战 `AArenaPlayerState`，快照人数并应用一次初始化缩放 GE。
- 默认 Health 缩放为一名玩家 `1.0x`、两名玩家 `1.75x`；同时正确更新 MaxHealth 和当前 Health。
- Boss 生成后的玩家死亡或暂时丢失 Pawn 不触发重新缩放，避免当前生命比例跳变。

### 阶段边界

本阶段不实现召唤物、Boss Intro、Boss 专属掉落或新的 Boss 专属构筑升级；不在阶段转换时直接重置 Boss Health，也不加入全面状态免疫。

### 完成标准

- 阶段只能向前转换，每个阶段入口最多执行一次。
- 单次高伤害跨越多个阈值时，最终阶段、Ability 解锁和 GameplayCue 结果确定且不重复。
- Enraged 属性完全由 GameplayEffect 提供，客户端正确观察 Tag 和表现。
- 一人与两人模式分别得到预期 MaxHealth，缩放只在 Boss 初始化时执行一次。
- 两名玩家分别死亡、重生或暂时缺少 Pawn 时，不会改变已经确定的 Boss MaxHealth。

### 进入下一阶段的前置条件

阶段切换、Ability 门控、Enraged GE、多人缩放和死亡清理均达到完成标准，并完成至少一轮单人完整 Boss 战回归。

---

## 阶段四：召唤物与构筑兼容

### 阶段目标

增加可控的战场压力，并确保现有 Fire、Lightning、Crit、Shield 和 Dash 构筑在 Boss 战中保持有效且具有自然联动。

### 开发内容

- 实现 `GA_Boss_SummonMinions`，只允许服务器生成配置的小怪 Class。
- Boss 独立保存召唤物集合、绑定死亡回调并限制同时存在数量，默认上限为四个。
- 召唤物不加入 WaveManager 的 Boss 胜利计数，不使用普通波次剩余敌人数阻止 Boss Victory。
- Boss 死亡、Actor 销毁或战斗结束时取消召唤并清理仍存活的召唤物。
- 召唤物是否允许触发 OnKill、OnCrit 等玩家被动必须通过 GameplayTag 规则明确，不依赖 Actor 名称。
- 验证 Burning、Shocked、Overload、Crit、ShieldBreakBlast 和 Dash 对 Boss 与召唤物的现有规则。
- 保持 Boss 可被现有元素状态影响；如需抗性，使用明确的 GameplayEffect 或 Tag 规则，不改变通用伤害公式。
- 如增加 Boss 专属构筑联动，本阶段最多加入一项可解释、可测试且不硬编码 Upgrade ID 的传奇规则。

### 阶段边界

本阶段不实现开场镜头、专属掉落、战斗锁定镜头、持久进度、Boss 装备或新的通用构筑系统。

### 完成标准

- 召唤物数量不超过上限，生成失败不会阻塞 Boss AI 或 Victory。
- Boss 本体死亡后能立即完成 Boss Wave，并可靠清理所有召唤物和相关 Delegate。
- 召唤物死亡不会重复修改 WaveManager 的 `RemainingEnemyCount`。
- Overload 等范围联动能够自然作用于 Boss 周围召唤物，现有五条构筑不会因 Boss 规则整体失效。
- Listen Server 中召唤、移动、伤害、死亡和清理只由服务器决定，客户端不会重复生成。

### 进入下一阶段的前置条件

召唤生命周期、Boss Victory 边界、现有构筑兼容和两人服务器权威行为均达到完成标准。

---

## 阶段五：演出与奖励

### 阶段目标

在不改变核心战斗规则的前提下，补齐 Boss 出场、阶段转换、攻击反馈、死亡演出和可选奖励。

### 开发内容

- 增加明确的 Boss Intro 阶段或等价的服务器同步状态，在 Intro 结束后再进入 Combat。
- 每个本地 `AArenaPlayerController` 独立执行 Boss 开场镜头、Camera Blend 和 HUD 动画。
- Intro 期间由服务器阻止玩家和 Boss 激活战斗 Ability，不能只在客户端禁用输入。
- 补充 Boss 出场、GroundSlam、Charge、FireZone、召唤、阶段转换、Enraged、受击和死亡的 Montage、Niagara 与 Sound。
- 调整 Boss HUD、阶段提示、危险预警和伤害反馈，保证顶视角与第三人称都清晰可读。
- 根据完整游戏循环决定是否增加 Boss 专属掉落；如果 Victory 后没有继续消费资源的场景，可以省略 Health/Energy 掉落。
- 若实现专属掉落，使用独立 DropTable 或 Boss Override，由服务器生成并复制，不建立背包或持久化系统。

### 阶段边界

本阶段不修改伤害公式、阶段阈值、AI 技能选择或构筑规则；不复制相机 Transform，不强制两个本地玩家使用相同视角，也不加入战斗期间的长期锁定镜头。

### 完成标准

- 每个本地玩家独立看到一次开场演出，镜头结束后恢复其原有顶视角或第三人称模式。
- Intro 期间没有玩家或 Boss 提前造成伤害，Listen Server 各端进入 Combat 的服务器时序一致。
- 所有持续 VFX、音效、Cue 和镜头状态在 Boss 死亡、关卡结束或中途取消时正确清理。
- 所有攻击预警在两种视角下都能让玩家判断位置、范围和兑现时机。
- 专属掉落若被保留，必须在 Victory 流程中具有明确用途并保持服务器权威。

### 进入下一阶段的前置条件

Boss 战斗规则不再扩展，主要动画、VFX、音效、HUD 和 Intro 已接入，剩余工作以验证、平衡和修复为主。

---

## 阶段六：验证、平衡与收尾

### 阶段目标

停止增加新功能，通过单人、双人、双视角和异常生命周期测试完成 Boss Demo 的最终稳定性与作品集展示质量。

### 开发内容

- 将所有尚未完成的 Boss 测试方法和通过标准加入 `PENDING_VERIFICATION.md`。
- 验证完整普通波次、升级选择、Boss Intro、Boss 战斗、Boss 死亡和 Victory 流程。
- 验证一人与两人的 Health 缩放、选敌、伤害、召唤、状态复制和相机独立性。
- 分别在顶视角和第三人称验证 GroundSlam、Charge、FireZone、召唤物和阶段切换表现。
- 验证 Boss 在技能前摇、Root Motion、周期区域和召唤过程中死亡、眩晕、取消或进入终局的清理行为。
- 验证所有现有构筑对 Boss 的基础兼容、触发次数、伤害权威和 GameplayCue 唯一性。
- 调整数值、预警时间和技能频率，但不在本阶段增加新的机制或系统。
- 完成验证后，从 `PENDING_VERIFICATION.md` 删除对应条目，并将 `IMPLEMENTED_FEATURES.md` 的 Boss 状态更新为 `Verified`。

### 阶段边界

本阶段只允许测试所需的修复、数值调整和表现修正，不增加新 Ability、新阶段、新构筑、新掉落类型或新的 AI 架构。

### 完成标准

- 单人完整流程可以稳定到达 Victory，没有重复 Boss、重复伤害或残留 Actor。
- 两人 Listen Server 中 Boss 行为、伤害和阶段转换只在服务器结算一次，客户端表现一致。
- 玩家死亡后 Boss 能重新选择存活目标，全员死亡后正确进入 Defeat。
- 两种视角下预警、镜头、HUD 和 VFX 均可读，视角切换不会复制 Ability、Area Actor、召唤物或伤害。
- Boss 相关待验证条目完成后已从 `PENDING_VERIFICATION.md` 删除，功能记录与实际行为一致。

### 后续边界

达到本阶段完成标准后，Boss 功能进入维护状态。除非用户明确扩展作品集范围，否则不继续增加新阶段、复杂团队副本机制、持久化奖励或大规模多人 Boss 系统。
