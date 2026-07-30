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
两个 ItemData 还会保存同一药水瓶 Mesh、红/蓝液体材质、瓶身/软木塞材质和 `0.25` 缩放；
因此玩家丢弃物品后，即使由原生 Pickup Class 生成，也会恢复对应药水外观而不是通用球体。
`WBP_Inventory` 使用 `WBP_InventorySlot` 构建二十个槽位，并自动连接到
`/Game/Core/BP_ArenaPlayerController.InventoryWidgetClass`。

保存后脚本会重新加载并核对全部七个新资产以及 PlayerController 引用，包括：

- 唯一 `ItemTag`、多值 `ItemTags`、恢复 GE、SetByCaller Tag 和恢复量。
- `MaxStackSize=10`、共享冷却 GE、非空图标、原生世界 Pickup Class，以及药水瓶 Mesh/三材质槽。
- 两个 Pickup Blueprint CDO 的 `ItemData` 与 `Quantity=1`。
- 两个 Widget Blueprint 的原生父类、槽位类引用和 Controller View 类引用。
- `/Game/Items/Inventory` 与 `/Game/UI/Inventory` 下不存在同名 `_1/_2` 资产。

成功时 Output Log 会依次输出八条 `Verified ...`，最后输出
`Inventory item setup and post-save verification completed`。任一引用或数值不正确时脚本会明确报错，不会把未验证状态报告为完成。

在进入 PIE 前可通过 Session Frontend，或在控制台执行以下命令验证固定二十槽页数和 Tag 筛选规则：

```text
Automation RunTests ProjectArcaneArena.Inventory
```

当前应列出并通过七项规则测试：`Pagination`、`Stacking`、`TabGesture`、`Filters`、`PhaseAccess`、`ReplicationContract` 和 `ItemValidation`。其中 `TabGesture` 固化轻点保持、长按松开关闭、已打开再次按 Tab 关闭以及迟到 KeyUp 不重复改变界面的规则。

运行 PIE 后：

- 靠近道具约 `220` 单位并按 `G` 拾取。
- 按 `Tab` 打开背包。
- 单击槽位选择，双击或点击 `Use` 使用。
- 设置数量并点击 `Drop` 丢弃。

分页快速测试可直接修改一个关卡 Pickup 的 `Quantity`：

- `190 / 200 / 210` 分别生成 `19 / 20 / 21` 个十瓶堆栈。
- `400 / 410` 分别生成 `40 / 41` 个十瓶堆栈。

单次拾取最多新建 `100` 个堆栈。使用默认 `MaxStackSize=10` 时，`Quantity=1000`
仍可成功，`Quantity=1001` 会整笔拒绝且不会改动背包；该保护只限制单次异常分配，
不会限制通过后续合法拾取继续增加页数。
