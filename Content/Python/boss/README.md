# Boss Foundation 资产生成

本目录负责 Boss Foundation、阶段二 A 决策资产、阶段二 B Charge 和阶段二 C FireZone 资产的幂等创建与连接。Behavior Tree 图始终由编辑器手动维护，脚本不会覆盖已经连接的节点。

## 使用前提

1. 编译最新 `ProjectArcaneArenaEditor` C++。
2. 关闭并重新打开 Unreal Editor，使 Python 能读取新增原生类型和 GameplayTag。
3. 确认 Python Editor Script Plugin 已启用。

## 执行方式

在 Unreal Editor 中选择 `Tools -> Execute Python Script`，执行：

```text
Content/Python/boss/setup_boss_foundation.py
```

也可以在编辑器控制台执行：

```text
py "../../../../ProjectArcaneArena/Content/Python/boss/setup_boss_foundation.py"
```

脚本可连续执行多次。已有正确资产会复用并更新配置，不会重复追加最终 Boss 波；目标资产类型或父类错误时会停止并报告。

## 阶段二 A：Behavior Tree 决策骨架

完整编译并重启编辑器后执行：

```text
Content/Python/boss/setup_boss_decision.py
```

脚本创建并连接以下资产外壳：

- `/Game/Boss/AI/BP_ArenaBossAIController`
- `/Game/Boss/AI/BB_ArenaBoss`
- `/Game/Boss/AI/BT_ArenaBoss`

脚本会让 `BP_ArenaBossCharacter` 使用专用 Controller，把 `BT_ArenaBoss` 连接到 Controller，并把 `InitialAbilityDelay` 同步为 `3.0s`。它不会修改 Behavior Tree 图，因此连续执行不会覆盖手工节点。部分 UE 5.6 Python 环境不导出 Blackboard Key 类型或 `BehaviorTree.BlackboardAsset`；此时脚本仍会创建并连接可访问的资产，只把 `TargetActor` 和 BT 的 Blackboard 引用留给下面的手动步骤。

打开 `BB_ArenaBoss`。如果列表为空，点击 `New Key`，选择 `Object` 并命名为 `TargetActor`；然后确认以下配置：

- `TargetActor`：Object，Base Class 为 `Actor`，`Instance Synced` 关闭。

打开 `BT_ArenaBoss`，在 Behavior Tree 编辑器的 Blackboard Asset 选择器中指定 `BB_ArenaBoss`。如果顶部已经显示 `BB_ArenaBoss`，说明该引用已连接，不需要重复设置。

然后按以下固定结构手动连接：

1. 在 Root 下创建 `Selector`，右键该 Selector，选择 `Add Service -> Arena Boss Update Target`（C++ 类 `BTService_ArenaBossUpdateTarget`；加入后节点标题为 `Update Nearest Living Player`）；设置 `TargetActorKey=TargetActor`、更新间隔 `0.2`、切换优势 `150`。
2. Selector 第一分支创建 `Sequence`，命名 `GroundSlam`。
3. 给 `GroundSlam` 添加 Blackboard Decorator：`TargetActor Is Set`，`Observer Aborts=Both`。
4. 再选择 `Add Decorator -> Arena Boss Can Activate Ability`（C++ 类 `BTDecorator_ArenaBossCanActivateAbility`）：`TargetActorKey=TargetActor`、`AbilityTag=Ability.Enemy.Boss.GroundSlam`、`Observer Aborts=Lower Priority`。不要设为 `Self` 或 `Both`，否则 Ability 添加 `State.Attacking` 后会中断自身。
5. 在该 Sequence 内放置 `Arena Boss Activate Ability` Task（C++ 类 `BTTask_ArenaBossActivateAbility`），使用相同的 Target Key 与 AbilityTag。
6. Selector 第二分支创建 `Sequence`，命名 `Chase`，添加 `TargetActor Is Set / Observer Aborts=Both`。
7. 在 `Chase` 内放置 `Move To`：Blackboard Key 为 `TargetActor`、`Acceptable Radius=50`，启用 `Observe Blackboard Value` 与 `Track Moving Goal`。
8. Selector 最后放置 `Wait`，时间设为 `0.2` 秒，供无目标或配置失败时降频。

保存并编译 `BP_ArenaBossAIController`、`BP_ArenaBossCharacter`、`BB_ArenaBoss` 和 `BT_ArenaBoss`。墙体阻挡攻击路径时，GroundSlam Decorator 会失败，Selector 会继续执行 Chase；重新获得合法攻击路径后，Decorator 会中断低优先级 MoveTo。

## 阶段二 B：Boss Charge

完整编译并重启编辑器后执行：

```text
Content/Python/boss/setup_boss_charge.py
```

也可以在编辑器控制台执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/boss/setup_boss_charge.py"
```

脚本会创建或复用 `AS_BossCharge`、`AM_BossCharge`、`GA_BossCharge`、Charge 冷却和三个 GameplayCue，并把 `GA_BossCharge` 向 `BP_ArenaBossCharacter.StartupAbilities` 追加且只保留一份。Active/Impact Cue 直接继承原生 Looping/Burst 类，避免复制模板时被 GameplayCueManager 临时注册为模板旧 Tag；脚本不修改 `BT_ArenaBoss` 图。

打开 `BT_ArenaBoss`，把现有优先级调整为 `GroundSlam -> Charge -> Chase -> Wait`：

1. 在 `GroundSlam` 与 `Chase` 之间新建 `Sequence`，命名为 `Charge`。
2. 给 `Charge` 添加 Blackboard Decorator：`TargetActor Is Set`，`Observer Aborts=Both`。
3. 再添加 `Arena Boss Can Activate Ability` Decorator：`TargetActorKey=TargetActor`、`AbilityTag=Ability.Enemy.Boss.Charge`、`Observer Aborts=Lower Priority`。不要设为 `Self` 或 `Both`，否则 Ability 添加 `State.Attacking` 后会中断自身。
4. 在该 Sequence 内添加 `Arena Boss Activate Ability` Task：`TargetActorKey=TargetActor`、`AbilityTag=Ability.Enemy.Boss.Charge`。
5. 确认左到右分支顺序严格为 `GroundSlam`、`Charge`、`Chase`、`Wait`，然后保存 Behavior Tree。

最后打开并 Compile `BP_ArenaBossCharacter`，确认 `StartupAbilities` 同时包含且各只有一份：

- `GA_BossGroundSlam`
- `GA_BossCharge`

Charge Decorator 会从 Ability CDO 读取 `350` 最小距离和 `900` 最大距离。近距离优先进入 GroundSlam，中距离可进入 Charge，技能不可用时回退 Chase。

Charge 还会在服务器同时执行 NavMesh 直线 Raycast 和缩小胶囊 Sweep。隔墙高台、断崖或需要绕路才能到达的目标不会进入 Charge 分支，也不会消耗冷却；可直接通行的斜坡仍允许冲刺。运行中后来进入路径的动态墙体继续由碰撞命中结束 Charge。

默认 Charge Telegraph 持续 `0.8s`；原生 Cue 会把 Niagara 沿路径长度缩放，并使用 `WidthScale=3`、`HeightScale=2`、`VerticalOffset=12` 提高双视角可读性。修改这些参数后应重新保存 `GCN_BossCharge_Telegraph`。

开始 PIE 前还要在 Class Defaults 中确认：

- `BP_ArenaBossAIController.BehaviorTreeAsset = BT_ArenaBoss`
- `BP_ArenaBossCharacter.AIControllerClass = BP_ArenaBossAIController`
- `BP_ArenaBossCharacter.Auto Possess AI = Placed in World or Spawned`

Output Log 如果出现 `ArenaBossAIController_0 is missing BehaviorTreeAsset or BlackboardAsset`，表示运行时仍生成了原生 Controller。重新设置并 Compile 上述两个 Blueprint，停止当前 PIE 后重新开始；正确的运行时 Controller 名称通常包含 `BP_ArenaBossAIController_C`。

## 阶段二 C：Boss FireZone

完整编译并重启编辑器后执行：

```text
Content/Python/boss/setup_boss_fire_zone.py
```

也可以在编辑器控制台执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/boss/setup_boss_fire_zone.py"
```

脚本会创建或复用 `AS_BossFireZone`、`AM_BossFireZone`、`GA_BossFireZone`、FireZone 冷却、复制 Area 和两个半径 Cue，并把 `GA_BossFireZone` 向 `BP_ArenaBossCharacter.StartupAbilities` 追加且只保留一份。脚本不修改 `BT_ArenaBoss` 图。

打开 `BT_ArenaBoss`，把现有优先级调整为 `GroundSlam -> Charge -> FireZone -> Chase -> Wait`：

1. 在 `Charge` 与 `Chase` 之间新建 `Sequence`，命名为 `FireZone`。
2. 给 `FireZone` 添加 Blackboard Decorator：`TargetActor Is Set`，`Observer Aborts=Both`。
3. 再添加 `Arena Boss Can Activate Ability` Decorator：`TargetActorKey=TargetActor`、`AbilityTag=Ability.Enemy.Boss.FireZone`、`Observer Aborts=Lower Priority`。不要设为 `Self` 或 `Both`，否则 Ability 添加 `State.Attacking` 后会中断自身。
4. 在该 Sequence 内添加 `Arena Boss Activate Ability` Task：`TargetActorKey=TargetActor`、`AbilityTag=Ability.Enemy.Boss.FireZone`。
5. 确认左到右分支顺序严格为 `GroundSlam`、`Charge`、`FireZone`、`Chase`、`Wait`，然后保存 Behavior Tree。

最后打开并 Compile `BP_ArenaBossCharacter`，确认 `StartupAbilities` 同时包含且各只有一份：

- `GA_BossGroundSlam`
- `GA_BossCharge`
- `GA_BossFireZone`

FireZone Decorator 从 Ability CDO 读取 `600` 最小距离和 `1200` 最大距离。`600-900` 内 Charge 优先，Charge 冷却时可回退 FireZone；`900-1200` 主要由 FireZone 覆盖。找不到目标脚下地面或存在视线阻挡时，分支失败并继续 Chase。

默认 FireZone 固定预警 `1.0s`，半径 `300`，持续 `5s`，生成时立即结算第一跳并每 `0.5s` 继续结算。Active Cue 会同时播放火焰主体、边界 Niagara 和常驻圆环 Mesh；火焰 Niagara 默认每 `0.8s` 强制重播，因此不依赖 System Active 状态，圆环 Mesh 使用持续 Unlit Glow 材质。多个复制 Area 使用各自本地 Cue Target，不会在移除一个火区时清掉其他重叠火区。

## 生成内容

- `/Game/Boss/Character/BP_ArenaBossCharacter`
- `/Game/Boss/GAS/GameplayAbility/GA_BossGroundSlam`
- `/Game/Boss/GAS/GameplayEffect/GE_Init_BossAttributes`
- `/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossGroundSlam`
- `/Game/Boss/GAS/GameplayCue/GCN_BossGroundSlam_Telegraph`
- `/Game/Boss/GAS/GameplayCue/GCN_BossGroundSlam_Impact`
- `/Game/Boss/Animation/AM_BossGroundSlam`
- `/Game/Boss/GAS/GameplayAbility/GA_BossCharge`
- `/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossCharge`
- `/Game/Boss/GAS/GameplayCue/GCN_BossCharge_Telegraph`
- `/Game/Boss/GAS/GameplayCue/GCN_BossCharge_Active`
- `/Game/Boss/GAS/GameplayCue/GCN_BossCharge_Impact`
- `/Game/Boss/Animation/AS_BossCharge`
- `/Game/Boss/Animation/AM_BossCharge`
- `/Game/Boss/VFX/NS_BossCharge_Telegraph`
- `/Game/Boss/VFX/NS_BossCharge_Active`
- `/Game/Boss/VFX/NS_BossCharge_Impact`
- `/Game/Boss/GAS/GameplayAbility/GA_BossFireZone`
- `/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossFireZone`
- `/Game/Boss/GAS/Area/BP_ArenaBossFireZoneArea`
- `/Game/Boss/GAS/GameplayCue/GCN_BossFireZone_Telegraph`
- `/Game/Boss/GAS/GameplayCue/GCN_BossFireZone_Active`
- `/Game/Boss/Animation/AS_BossFireZone`
- `/Game/Boss/Animation/AM_BossFireZone`
- `/Game/Boss/VFX/NS_BossFireZone_Telegraph`
- `/Game/Boss/VFX/NS_BossFireZone_Active`
- `/Game/ParagonMuriel/FX/Meshes/Hero_Specific/SM_Knock_Up_Runes_Ring`（Active Cue 持续边界）
- Boss 直接引用的 Mesh、AnimBP、Animation 与 Niagara 副本
- `DA_Waves_Prototype` 现有普通波次之后的唯一最终 Boss 波

脚本保留全部现有普通波。没有 Boss 波时在末尾追加，已有唯一末尾 Boss 波时原地更新；若存在多个 Boss 波，或 Boss 波之后仍有其他波，脚本会停止而不是静默改写流程。

`BossDisplayName` 默认继承 C++ 中通过 `NSLOCTEXT` 定义的“悟空战将”。需要改名时直接在 `BP_ArenaBossCharacter` 的 Class Defaults 中覆盖，不由 Python 重写继承 CDO 的本地化文本身份。

## 可选 HUD 蓝图控件

`WBP_PlayerHUD` 可以增加以下 `Is Variable` 控件：

- `BossPanel`
- `BossNameText`
- `BossHealthProgressBar`
- `BossHealthText`

没有这些控件时，C++ 会在顶部中央创建备用 Boss 血条，因此脚本执行后无需额外创建 HUD 蓝图即可进行功能验收。

## 资产边界

脚本只复制 Boss 直接引用的资源。Skeleton、PhysicsAsset、材质、贴图以及 Niagara 内部依赖继续引用项目现有资产，避免递归复制完整资源包。

## 阶段三 A：Boss 阶段系统

完整编译并重启编辑器后执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/boss/setup_boss_phase_system.py"
```

脚本会幂等创建并连接：

- `/Game/Boss/GAS/GameplayEffect/GE_Boss_Enrage`
- `/Game/Boss/GAS/GameplayCue/GCN_BossPhase_Transition`
- `/Game/Boss/GAS/GameplayCue/GCN_BossEnraged_Active`
- `/Game/Boss/VFX/NS_BossPhase_Transition`
- `/Game/Boss/VFX/NS_BossEnraged_Active`

同时把 `BP_ArenaBossCharacter` 的 `PhaseTwoHealthRatio`、`PhaseThreeHealthRatio` 和 `EnrageEffectClass` 配置为 `0.70`、`0.35` 与 `GE_Boss_Enrage`。脚本不会修改 `BT_ArenaBoss`，现有 `GroundSlam -> Charge -> FireZone -> Chase -> Wait` 顺序保持不变。

`WBP_PlayerHUD` 可以添加一个勾选 `Is Variable` 的 `TextBlock`，命名为 `BossPhaseText`。若不添加，C++ 会把阶段文本合并到 `BossNameText`；完全没有 Boss 控件时，运行时备用 Boss 面板会自动包含阶段文本。
