# Boss Foundation 资产生成

本目录负责第一阶段 Boss 资产的幂等创建和连接，不承担 Boss 多技能 Behavior Tree、阶段切换或演出逻辑。

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
