# Boss Foundation 资产生成

本目录负责 Boss Foundation 与阶段二 A 决策资产的幂等创建和连接；阶段二 A 只把现有 GroundSlam 接入 Behavior Tree，不包含 Charge、FireZone、阶段切换或演出逻辑。

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

脚本会让 `BP_ArenaBossCharacter` 使用专用 Controller，并把 `BT_ArenaBoss` 连接到 Controller。它不会修改 Behavior Tree 图，因此连续执行不会覆盖手工节点。部分 UE 5.6 Python 环境不导出 Blackboard Key 类型或 `BehaviorTree.BlackboardAsset`；此时脚本仍会创建并连接可访问的资产，只把 `TargetActor` 和 BT 的 Blackboard 引用留给下面的手动步骤。

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

开始 PIE 前还要在 Class Defaults 中确认：

- `BP_ArenaBossAIController.BehaviorTreeAsset = BT_ArenaBoss`
- `BP_ArenaBossCharacter.AIControllerClass = BP_ArenaBossAIController`
- `BP_ArenaBossCharacter.Auto Possess AI = Placed in World or Spawned`

Output Log 如果出现 `ArenaBossAIController_0 is missing BehaviorTreeAsset or BlackboardAsset`，表示运行时仍生成了原生 Controller。重新设置并 Compile 上述两个 Blueprint，停止当前 PIE 后重新开始；正确的运行时 Controller 名称通常包含 `BP_ArenaBossAIController_C`。

## 生成内容

- `/Game/Boss/Character/BP_ArenaBossCharacter`
- `/Game/Boss/GAS/GameplayAbility/GA_BossGroundSlam`
- `/Game/Boss/GAS/GameplayEffect/GE_Init_BossAttributes`
- `/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossGroundSlam`
- `/Game/Boss/GAS/GameplayCue/GCN_BossGroundSlam_Telegraph`
- `/Game/Boss/GAS/GameplayCue/GCN_BossGroundSlam_Impact`
- `/Game/Boss/Animation/AM_BossGroundSlam`
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
