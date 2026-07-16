# Overload Test Setup

该目录创建独立的升级构筑测试 GameMode、可重复拾取的升级道具、静止高血量木桩和测试关卡，不修改正式波次、正式 GameMode 或玩家 StartupAbilities。

## 生成方式

新增 C++ 反射类型完成编译并重启 Unreal Editor 后，在编辑器控制台执行：

```text
py "E:/UE_DEMO/ProjectArcaneArena/Content/Python/overload_test/setup_overload_test.py"
```

首次执行会创建或刷新测试 GameMode、木桩属性、木桩蓝图和通用升级拾取物蓝图，不会由 Python 创建、复制或切换 World Partition 地图。

保持正式 `Lvl_TopDown` 已打开，使用编辑器菜单 `File > Save Current Level As...` 保存为：

```text
/Game/Tests/Overload/Lvl_OverloadTest
```

`Save Current Level As` 会让编辑器原生处理 World Partition 与 External Actor 包。保持新测试关卡打开，再执行一次相同命令；第二次执行会设置 GameMode Override、放置 15 个升级拾取物和三个木桩，并保存当前测试关卡。

不要把 `EditorAssetLibrary.duplicate_asset()`、`load_asset(World)`、`LevelEditorSubsystem.load_level()` 或其他地图创建/切换调用加回生成脚本。Python 持有的 Inactive World 与同包地图切换可能让 UE 的 World GC 检测触发 Fatal。

## 自动配置

- `BP_ArenaGameMode_OverloadTest` 关闭 WaveData 与 PickupDropTable。
- 玩家继续通过 `BP_ArenaPlayerCharacter.StartupAbilities` 获得 BasicAttack、Fireball、Dash、Shield 和 LightningStorm；测试 GameMode 不再自动授予升级。
- `BP_ArenaUpgradeTestPickup` 是常驻测试道具。玩家进入球体后，服务器执行正式的 RequiredTags、BlockedTags、MaxStacks、GAS 授予和 PlayerState 层数记录；成功后补满生命与能量。
- 每个道具使用放大的旋转球体，并在头顶显示 ASCII 升级名称和简短效果，避开默认 TextRender 字体缺少中文字形的问题；球体和文字按属性或构筑使用不同颜色，并在 PIE 中朝向各客户端自己的本地相机。
- 脚本在 PlayerStart 周围扫描候选位置，只保留能 Sweep 到与首个位置高度接近地面的点，直到放满当前全部 15 个升级 DataAsset；失败时会清理本轮所有半成品。道具不会被销毁，离开碰撞球后重新进入即可测试下一层。
- `BP_ArenaEnemy_OverloadDummy` 没有 AI、攻击能力或移动速度，使用 `5000 Health / 0 Shield / 0 Defense`。
- 三个木桩分别位于主目标中心、中心右侧 `250` 和 `350` 单位，用于验证 Overload 的 `300` 范围边界；脚本会按木桩胶囊尺寸向下扫掠，让胶囊底部自动贴合当前地面。
- 脚本可重复执行，只替换带 `OverloadTestDummy` 或 `OverloadTestUpgradePickup` Actor Tag 的测试 Actor，不会累积重复项。

## 推荐拾取顺序

以下升级带有构筑前置，先拾取左侧基础道具，再拾取箭头右侧道具：

```text
Fireball Damage        -> Fireball Burning
LightningStorm Damage -> LightningStorm Shocked
Fire 或 Lightning 构筑 -> Overload（同时拥有两种构筑时用于完整联动）
Crit Chance            -> Energy On Crit
Shield Amount          -> Shield Break Blast
Dash Cooldown          -> Dash Lightning Trail
```

双人 PIE 中道具由服务器处理且不会销毁，两名玩家可以分别拾取同一个道具并形成不同构筑。

如果只生成了 `BP_ArenaUpgradeTestPickup`，但关卡中没有看到道具，说明脚本是在其他地图打开时执行的。保持 `Lvl_OverloadTest` 作为当前主编辑关卡，再执行一次脚本；Output Log 应出现 `Placed 15 grounded upgrade test pickups.`。
