# Overload Test Setup

该目录创建独立的 Overload 测试 GameMode、静止高血量木桩和测试关卡，不修改正式波次、正式 GameMode 或玩家 StartupAbilities。

## 生成方式

新增 C++ 反射类型完成编译并重启 Unreal Editor 后，在编辑器控制台执行：

```text
py "E:/UE_DEMO/ProjectArcaneArena/Content/Python/overload_test/setup_overload_test.py"
```

首次执行只创建三个测试资产，不会由 Python 创建、复制或切换 World Partition 地图。

保持正式 `Lvl_TopDown` 已打开，使用编辑器菜单 `File > Save Current Level As...` 保存为：

```text
/Game/Tests/Overload/Lvl_OverloadTest
```

`Save Current Level As` 会让编辑器原生处理 World Partition 与 External Actor 包。保持新测试关卡打开，再执行一次相同命令；第二次执行会设置 GameMode Override、放置三个木桩并保存当前测试关卡。

不要把 `EditorAssetLibrary.duplicate_asset()`、`load_asset(World)`、`LevelEditorSubsystem.load_level()` 或其他地图创建/切换调用加回生成脚本。Python 持有的 Inactive World 与同包地图切换可能让 UE 的 World GC 检测触发 Fatal。

## 自动配置

- `BP_ArenaGameMode_OverloadTest` 关闭 WaveData 与 PickupDropTable。
- 玩家按正式服务器升级路径依次获得 Fireball Damage、Fireball Burning、LightningStorm Damage 和 Overload。
- `BP_ArenaEnemy_OverloadDummy` 没有 AI、攻击能力或移动速度，使用 `5000 Health / 0 Shield / 0 Defense`。
- 三个木桩分别位于主目标中心、中心右侧 `250` 和 `350` 单位，用于验证 Overload 的 `300` 范围边界；脚本会按木桩胶囊尺寸向下扫掠，让胶囊底部自动贴合当前地面。
- 脚本可重复执行，只替换带 `OverloadTestDummy` Actor Tag 的测试木桩。
