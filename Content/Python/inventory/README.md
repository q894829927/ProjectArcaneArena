# 背包首批资产设置

新增 C++ 类型完成编译并重启 Unreal Editor 后，在 Output Log 执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/inventory/setup_inventory_items.py"
```

脚本会幂等创建：

- `/Game/Items/Inventory/Data/DA_HealthPotion`
- `/Game/Items/Inventory/Data/DA_EnergyPotion`
- `/Game/Items/Inventory/Effects/GE_Cooldown_ItemConsumable`
- `/Game/Items/Inventory/Pickups/BP_HealthPotionPickup`
- `/Game/Items/Inventory/Pickups/BP_EnergyPotionPickup`
- `/Game/UI/Inventory/WBP_Inventory`
- `/Game/UI/Inventory/WBP_InventorySlot`

两个 Pickup Blueprint 已配置对应 `ItemData` 和默认数量 `1`，可以直接拖入测试关卡。
`WBP_Inventory` 使用 `WBP_InventorySlot` 构建二十个槽位，并自动连接到
`/Game/Core/BP_ArenaPlayerController.InventoryWidgetClass`。

保存后脚本会重新加载并核对全部七个新资产以及 PlayerController 引用，包括：

- 唯一 `ItemTag`、多值 `ItemTags`、恢复 GE、SetByCaller Tag 和恢复量。
- `MaxStackSize=10`、共享冷却 GE、非空图标和原生世界 Pickup Class。
- 两个 Pickup Blueprint CDO 的 `ItemData` 与 `Quantity=1`。
- 两个 Widget Blueprint 的原生父类、槽位类引用和 Controller View 类引用。
- `/Game/Items/Inventory` 与 `/Game/UI/Inventory` 下不存在同名 `_1/_2` 资产。

成功时 Output Log 会依次输出八条 `Verified ...`，最后输出
`Inventory item setup and post-save verification completed`。任一引用或数值不正确时脚本会明确报错，不会把未验证状态报告为完成。

在进入 PIE 前可通过 Session Frontend，或在控制台执行以下命令验证固定二十槽页数和 Tag 筛选规则：

```text
Automation RunTests ProjectArcaneArena.Inventory
```

运行 PIE 后：

- 靠近道具约 `220` 单位并按 `G` 拾取。
- 按 `Tab` 打开背包。
- 单击槽位选择，双击或点击 `Use` 使用。
- 设置数量并点击 `Drop` 丢弃。

分页快速测试可直接修改一个关卡 Pickup 的 `Quantity`：

- `190 / 200 / 210` 分别生成 `19 / 20 / 21` 个十瓶堆栈。
- `400 / 410` 分别生成 `40 / 41` 个十瓶堆栈。
