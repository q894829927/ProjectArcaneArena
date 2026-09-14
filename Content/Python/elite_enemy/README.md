# 精英敌人资产配置

`setup_elite_enemy.py` 会创建三个精英词缀 DataAsset、原生 GE 的 Blueprint 子类和
GameplayCue 资产，并把已确认的精英曲线写入 `DA_Waves_Prototype`。

## 执行方式

1. 编译包含精英 C++ 类型的项目，然后重启 Unreal Editor。
2. 在 Unreal Editor 中选择 **Tools > Execute Python Script**。
3. 执行 `Content/Python/elite_enemy/setup_elite_enemy.py`。
4. 再执行一次，确认操作保持幂等。

脚本会在创建任何资产前检查全部依赖与九波结构，不加载或保存地图。它会修改既有
WaveData 的 `EliteBaseline`、`EliteBaselineEffectClass`、`EliteCount` 和
`EliteAffixPool`。若检测到旧正式资产的 W6–W8 仍为单一近战 Entry，会在保持每波总数
`13/16/17` 不变的前提下幂等迁移为 `8M+5R / 10M+6R / 10M+7R`；已有远程 Entry 时
不会再次拆分。SpawnInterval、RewardCount 和 Boss 配置始终保留。

生成资产位于：

- `/Game/Data/EnemyAffix`
- `/Game/GAS/GameplayEffect`
- `/Game/GAS/GameplayCues/Elite`

Looping 与 Burst Cue 会从项目现有模板起步，作为安全可用的首版表现占位。后续可以直接
调整美术内容，不需要改变玩法契约或重新配置 C++。
