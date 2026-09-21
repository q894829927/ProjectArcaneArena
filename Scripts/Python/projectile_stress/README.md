# Projectile Stress Test Blueprint

该目录用于生成 P0/P1 高密度 Projectile 压力测试 Blueprint。

## 生成资产

脚本：

`Content/Python/projectile_stress/setup_projectile_stress.py`

生成：

`/Game/ProjectArcaneArena/Dev/Tests/Projectile/BP_ArenaProjectileStressTestActor`

父类：

`AArenaProjectileStressTestActor`

脚本是幂等的；资产已经存在时会复用并保存，不重复创建。

## 执行方式

完成 C++ 编译并重启 Unreal Editor 后：

`Tools > Execute Python Script`

选择：

`Content/Python/projectile_stress/setup_projectile_stress.py`

首轮 DataPool 压测直接使用 C++ 默认值：

- Stress Mode = DataPool
- Auto Start = true
- Run On Authority Only = true
- Target Active Projectiles = 1000
- Derive Spawn Rate From Target = true
- Projectile Lifetime = 3.0
- Projectile Speed = 1800
- Spawn Radius = 100
- Max Spawn Per Frame = 512
- Prefill Target On Start = false
- Report Interval Seconds = 5.0
- Random Seed = 1337

脚本不会自动修改或保存任何地图。资产生成后，将 Blueprint 手动放入独立测试地图进行 PIE。
