# 拾取物与掉落表资产生成器

本目录用于幂等创建和配置服务器权威拾取系统所需的 Blueprint 资产。运行前需先编译新增 C++ 类并重启 Unreal Editor，使 Python 能读取项目反射类型。

## 一键执行

在 Unreal Editor 控制台运行：

```text
py "../../../../ProjectArcaneArena/Content/Python/setup_pickup_items.py"
```

脚本会创建或更新：

- `/Game/Items/Pickups/BP_HealthPickup`：恢复 `25 Health`，`15s` 后超时销毁。
- `/Game/Items/Pickups/BP_EnergyPickup`：恢复 `20 Energy`，`15s` 后超时销毁。
- `/Game/Data/Pickup/DA_PickupDropTable_Default`：`25%` 掉率，Health/Energy 权重均为 `1`。
- `/Game/GameMode/BP_ArenaGameMode`：将 `PickupDropTable` 设置为上述默认表。

重复执行不会创建重复资产，但会把这些数据字段重置为脚本顶部的配置。

## 可选蓝图表现

C++ 父类已提供小型 Sphere Mesh 和旋转组件，不配置 Niagara 也可测试拾取。需要更清晰的占位表现时：

1. 打开 `BP_HealthPickup`，添加 Niagara Component，`System Asset` 设为 `NS_AuraFX_Nature`，缩放到不遮挡场景。
2. 打开 `BP_EnergyPickup`，添加 Niagara Component，`System Asset` 设为 `NS_AuraFX_Lightning`，缩放到不遮挡场景。
3. 两个 Niagara Component 均关闭碰撞，拾取判定仍由 C++ `PickupCollisionComponent` 负责。

本轮不生成音效、世界文字、GameplayCue 或磁吸逻辑。
