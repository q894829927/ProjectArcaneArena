# Project Arcane Arena 背包系统开发计划

## 文档职责

本文档是阶段五 C“轻量 MVC 背包系统”的开发规范，维护系统边界、阶段顺序、公开接口和验收标准。

状态：`Partial`，最后更新：2026-07-30。

当前已完成阶段五 C-A 至 C-C 的 C++ 第一版、原生 fallback UI 和幂等资产脚本，并开始阶段五 C-D 的代码侧网络与生命周期收尾。最新 Editor DLL 已能加载全部背包原生类型；资产脚本已成功创建并在保存后验证共享冷却 GE、两种药水 DataAsset、两种 Pickup Blueprint、`WBP_Inventory`、`WBP_InventorySlot` 以及 `BP_ArenaPlayerController` 的 View 引用，重复执行没有产生 `_1/_2` 资产。两种药水 DataAsset 已保存瓶体 Mesh、红/蓝液体材质、瓶体/软木材质和相对缩放，丢弃后的原生 Pickup 不再依赖 Blueprint 组件外观。FastArray 已改为精确标记真实变化并使用 UE 5.6 `PostReplicatedReceive` 在完整 Delta 批次后刷新 View；药水使用会在满资源时提前拒绝，并在恢复失败时回滚冷却；同一 `ItemTag` 的冲突 DataAsset 和无效 Deferred Pickup 初始化也会被服务器拒绝。阶段权限已集中到 `AArenaGameState`，View、客户端意图和服务器操作使用同一矩阵；`Lvl_OverloadTest` 的测试 GameMode 可在 `Waiting` 开放完整操作。七项 `ProjectArcaneArena.Inventory` Automation Tests 已通过；单人 PIE 已确认 `0/19/20/21/40/41` 堆栈分页、第 `11` 个同类药水拆分新堆栈、删除末页堆栈后页码回退、多 Tag OR 筛选、Health/Energy Potion 使用、满资源拒绝、共享冷却、部分/整组丢弃与重新拾取、Tab 轻点/长按/Escape、连续快速开关、背包打开时重新 Possess、双视角输入恢复、Upgrade 只读背包交接，以及超量拾取失败后同一 Pickup 恢复碰撞并可再次成功交互。Dead/Stunned、完整阶段权限、分辨率布局和多人网络验收尚未完成，因此本阶段保持 `Partial`。

---

## 阶段目标

实现一个本局内、服务器权威、兼容两人 Listen Server 和顶视角/第三人称的消耗品背包：

* 每页固定二十个槽位，使用五列四行布局。
* 物品堆栈超过二十个时自动增加页数，并支持上一页/下一页。
* 支持按 GameplayTag 多选筛选、拾取、堆叠、使用和部分/整组丢弃。
* 第一版提供 Health Potion 与 Energy Potion。
* 保留现有自动重叠并立即恢复资源的 `AArenaPickupActor`；可入包道具使用独立 Pickup 类型。
* Upgrade 构筑继续由现有升级系统管理，不转换为背包物品。

---

## 轻量 MVC 职责

### Model

* `UArenaInventoryComponent` 创建在 `AArenaPlayerState` 上，使玩家重生或更换 Character 后仍保留本局物品。
* `FArenaInventoryList` 使用 `FFastArraySerializer`，只向所属客户端复制；客户端不拥有增加、删除、使用或丢弃的最终权限。
* `FArenaInventoryEntry` 保存服务器生成的 `StackId`、`UArenaItemDataAsset`、`Quantity` 和连续 `DisplayOrder`。
* 所有客户端操作使用 `StackId` 定位堆栈，不依赖可能因压紧或筛选而变化的界面槽位索引。
* Model 只保存原始堆栈顺序，不保存本地页码、筛选选择、悬浮槽位或弹窗状态。

### Controller

* `AArenaPlayerController` 管理背包开关、当前页、选中筛选 Tag、选中堆栈、输入模式和 Widget 刷新。
* Controller 接收 View 事件并调用 `ServerInteractWithInventoryPickup()`、`ServerUseInventoryItem()` 和 `ServerDropInventoryItem()`。
* Server RPC 只提交 Pickup Actor、StackId 和请求数量；服务器重新验证阶段、玩家状态、距离、视线、物品数量和 Actor 生命周期。
* Controller 根据 Model 快照与本地筛选状态生成页面 ViewData，View 不直接遍历或修改复制容器。

### View

* `WBP_Inventory` 负责二十槽布局、标签筛选、翻页、Use、Drop 和当前页显示。
* `WBP_InventorySlot` 只显示图标、数量、选中和禁用状态，并发出点击、双击或悬浮事件。
* Drop 数量面板限制输入范围为 `1` 到当前复制数量；服务器使用最新权威数量重新验证，非正数或超量请求直接拒绝，不静默改成其他数量。
* Widget 不应用 GameplayEffect、不生成 Pickup、不修改 Attribute 或背包数组。

---

## 数据与 GameplayTag

新增 `UArenaItemDataAsset`，至少包含：

* `ItemTag`：物品唯一 GameplayTag。
* `ItemTags`：物品可拥有的多个分类、效果或筛选 Tag。
* `DisplayName`、`Description`、`Icon`。
* `MaxStackSize`。
* `UseGameplayEffectClass`、`SetByCallerMagnitudeTag`、`UseMagnitude`。
* `WorldPickupClass`。

第一版 Native Tags：

```text
Item.Consumable.HealthPotion
Item.Consumable.EnergyPotion
Item.Type.Consumable
Item.Effect.Restore.Health
Item.Effect.Restore.Energy
Cooldown.Item.Consumable
```

物品唯一 `ItemTag` 同时参与筛选；`ItemTags` 允许一个物品属于多个分类。筛选使用 GameplayTag 层级匹配，不使用 Actor 名称或资产路径判断类型。

首批数据：

| 物品 | 恢复 | MaxStackSize | Tags |
| --- | ---: | ---: | --- |
| Health Potion | `50 Health` | `10` | `Item.Type.Consumable`、`Item.Effect.Restore.Health` |
| Energy Potion | `25 Energy` | `10` | `Item.Type.Consumable`、`Item.Effect.Restore.Energy` |

两种药水复用现有 Health/Energy Restore GE 与 SetByCaller 管线，并共享持续一秒的 `Cooldown.Item.Consumable`。

---

## 堆叠、分页与筛选规则

* 拾取时先按 `ItemTag` 补满已有未满堆栈，再把剩余数量按 `MaxStackSize` 拆成新堆栈并追加。
* 单次拾取最多新建 `100` 个堆栈；超过时整笔拒绝且不改变已有数量，避免错误关卡数据在一帧内创建海量 FastArray 条目。该限制不限制背包总页数，后续合法拾取仍可继续增长。
* 删除空堆栈后重新生成连续 `DisplayOrder`；第一版不保留空洞，也不支持拖拽排序或槽位交换。
* 页数按当前筛选后的堆栈数量计算：`Max(1, Ceil(FilteredStackCount / 20))`。
* `0–20` 个堆栈显示一页，`21–40` 显示两页，依此类推；页数不设置硬上限。
* 每一页始终渲染二十个槽位，不足部分显示为空槽。
* 筛选允许同时选中多个 Tag，匹配任意一个选中 Tag 即显示；没有选中 Tag 等同 `All`。
* 筛选变化或堆栈删除后重新计算页数，并把当前页 Clamp 到有效范围。
* 筛选和翻页只改变本地 ViewData，不改变服务器堆栈顺序或复制内容。

---

## 拾取、使用与丢弃

### 可入包 Pickup

* 新增复制的 `AArenaInventoryPickupActor`，保存 ItemData 和 Quantity；拾取与物品归属结果只由服务器决定。
* `G` 从约 `220` 单位范围内选择最近、存活且有视线的可入包 Pickup；距离相同使用稳定 Actor 路径排序。
* 客户端只提交候选 Actor，服务器重新验证距离、视线、ItemData、Quantity 和未消费状态。
* 两名玩家同时请求同一 Actor 时，服务器消费门闩保证最多一人成功；成功加入全部数量后销毁复制 Actor。
* 现有 `AArenaPickupActor` 继续自动重叠并立即恢复，不进入新背包流程。

### 使用

* `Tab` 按阶段权限打开背包；轻点保持开关语义，长按超过 `0.25s` 时作为临时查看并在松开后关闭。打开后世界不暂停，玩家仍会受到服务器伤害。
* 打开背包时停止本地移动、Look、Sprint 和主动技能输入；关闭后恢复原顶视角或第三人称鼠标、准星和输入模式，顶视角鼠标保持关闭前的位置，不执行视角切换使用的居中操作。
* 药水通过双击槽位或选中后点击 Use 使用。
* 服务器只允许处于可操作阶段、存活、未眩晕且没有 `Cooldown.Item.Consumable` 的玩家使用。
* 服务器通过配置的 GameplayEffect 和 SetByCaller 恢复资源；只有属性实际增加时才扣除一个物品并应用共享冷却。
* 资源已满、GE 配置无效或状态不允许时，不扣数量、不添加冷却。

### 阶段权限矩阵

| 阶段 | 查看 | 拾取/使用/丢弃 | 说明 |
| --- | --- | --- | --- |
| `Waiting` | 允许 | 默认禁止 | 编辑器测试 GameMode 可通过 `bAllowInventoryOperationsWhileWaiting` 开放完整操作 |
| `Combat` | 允许 | 允许 | 正常战斗背包权限 |
| `Upgrade` | 允许 | 禁止 | 打开背包时隐藏升级选择，关闭后恢复原候选 |
| `BossIntro` | 禁止 | 禁止 | 保留镜头和 Space 长按跳过 |
| `BossOutro` | 禁止 | 禁止 | 保留死亡演出和 Space 长按跳过 |
| `Victory` | 允许 | 允许 | 按明确列出的操作采用完整权限，Restart 状态仍由服务器独立管理 |
| `Defeat` | 允许 | 禁止 | 允许复盘物品，不允许使用或丢弃 |

* `AArenaGameState::CanViewInventory()` 和 `CanPerformInventoryOperations()` 是阶段权限的唯一规则入口。
* `FArenaInventoryPageViewData::bCanPerformActions` 只控制 View 的操作可用性；Controller 和服务器 Model 仍分别重新验证。
* Dead/Stunned 即使处于可操作阶段也不能拾取、使用或丢弃。

### 丢弃

* 选中 Drop 后显示数量面板，可选择 `1` 到当前堆栈数量。
* 服务器验证 StackId 和数量后，在玩家前方查找有地面且未被墙体、Pawn 或动态 Actor 占用的安全位置，生成对应 ItemData/Quantity 的复制 Pickup，再扣除背包数量。
* 找不到合法生成位置时不扣除物品。
* 新 Pickup 对丢弃者提供短暂拾取保护，避免角色仍在重叠范围时立即捡回；其他玩家可以正常拾取。

---

## 开发阶段

### 阶段五 C-A：数据与复制 Model

状态：`Implemented`，UHT、源码编译和 Editor 链接已通过，待网络验证。

* 新增 Item DataAsset、FastArray Entry/List 和 `UArenaInventoryComponent`。
* 在 `AArenaPlayerState` 创建组件并完成 OwnerOnly 增量复制、变化 Delegate 和服务器查询/修改接口。
* 实现补栈、拆栈、压紧顺序和 StackId 防错操作。
* 已实现 `UArenaItemDataAsset`、`FArenaInventoryEntry`、`FArenaInventoryList` 与 `UArenaInventoryComponent`，并由 `AArenaPlayerState` 创建组件。
* FastArray 只标记实际新增、数量变化、移除或改序的条目；客户端在 `PostReplicatedReceive` 的完整 Delta 批次后通过 `OnInventoryChanged` 构建最终只读页面快照，不使用下一帧 Timer 读取中间状态。
* 服务器拒绝把两个不同 DataAsset 以同一唯一 `ItemTag` 合并，避免堆叠后继承错误的恢复量、图标或 Pickup Class。
* `ApplyStackAddition()` 先在副本中计算并执行补栈；无效已有数量或单次需要超过 `100` 个新堆栈时保持原数组不变，避免异常 Pickup 造成部分写入或服务器内存突增。
* 运行时模块已显式依赖 `NetCore`，为公共 FastArray 类型和生成代码提供正确链接边界。

完成标准：服务器能稳定保存任意数量堆栈；所属客户端收到相同顺序和数量，其他客户端看不到私有背包数据。

### 阶段五 C-B：权威物品流程

状态：`Partial`，C++、脚本及首批资产生成已完成并通过保存后自检，核心单人 PIE 已验证，多人权威待验证。

* 新增交互 Pickup、G 目标选择、服务器距离/视线验证和竞争消费门闩。
* 实现药水 GAS 使用、共享冷却、满资源失败保护和部分丢弃。
* 创建 Health Potion、Energy Potion、Cooldown GE 和对应可入包 Pickup 资产。
* 已实现复制的 `AArenaInventoryPickupActor`、三个服务器 RPC、稳定 `StackId` 使用/丢弃和一秒 `Cooldown.Item.Consumable`；Pickup 从 ItemData 的软引用 Mesh、材质和相对变换构建世界外观，仅旋转 Mesh，名称与数量在各客户端朝向本地相机并按恢复类型着色。专服不加载纯表现资产。
* 药水恢复语义由 `SetByCallerMagnitudeTag` 决定，`ItemTags` 只承担分类与筛选；资源已满时不执行恢复 GE，Deferred Pickup 也只接受通过运行时定义校验的 ItemData 和正数量。
* `UArenaItemDataAsset::IsRuntimeDefinitionValid()` 统一校验 `ItemTag`、`MaxStackSize` 和使用事务；`IsUseConfigurationValid()` 继续要求恢复 GE 为 Instant、冷却 GE 为 HasDuration 并授予 `Cooldown.Item.Consumable`，同时拒绝不支持的恢复 Tag 和非正数值。Pickup、背包 Model 与编辑器 Content Validation 共用这些规则，避免无效物品进入背包后无法使用；缺少图标或专属 Pickup Class 仍为非阻断警告。
* `TryAddItem()`、`TryUseItem()` 和 `TryDropItem()` 都在 Model 层重验阶段、Dead/Stunned 与 Authority，并使用服务器侧不可重入事务门闩，阻止 GameplayEffect、Actor `BeginPlay` 或 UI Delegate 的同步回调重复修改同一 FastArray。Drop 额外验证 Source Pawn 的 PlayerState 与背包所有者一致；Pickup 在调用背包前先占用消费门闩并关闭碰撞，失败时恢复门闩与原碰撞模式，保证阶段切换、多人竞争和回调重入最多成功一次。
* 丢弃者保护优先使用 GameState 同步服务器时间；缺少 `AArenaGameState` 的测试世界回退到 `UWorld` 时间，避免保护期永久无法结束。
* `Content/Python/inventory/setup_inventory_items.py` 已重新执行并保存共享冷却 GE、两个药水 DataAsset 与 Pickup Blueprint；红/蓝药水瓶的软引用世界 Mesh、三材质槽与缩放已写入 DataAsset，保存后重新加载验证了 Tag、恢复量、堆叠、GE、图标、世界外观、Pickup CDO 引用及 `_1/_2` 重复资产。
* 单人 PIE 已使用超量事务制造真实加入失败，并通过先补入 `x9` 后重试同一个 `Quantity=1001` Pickup 验证：失败不会销毁 Actor 或污染背包，消费门闩与原碰撞模式恢复后可以再次成功交互。

完成标准：拾取、使用和丢弃均只结算一次，客户端不能伪造物品、恢复或世界 Pickup。

### 阶段五 C-C：分页筛选与双视角 UI

状态：`Partial`，C++ View 与 WBP 资产已实现并通过保存后引用自检，双视角交互待验证。

* 新增 `WBP_Inventory`、`WBP_InventorySlot` 和 Drop 数量面板。
* 实现二十槽分页、多 Tag OR 筛选、双击/Use、翻页和当前页 Clamp。
* 接入 Tab/G Enhanced Input；Tab 支持轻点切换和长按临时查看，并与 Upgrade、BossIntro、BossOutro、Victory、Dead 和 Stunned 状态协调输入清理。
* 已实现无需蓝图资产即可运行的 `UArenaInventoryWidget` 与 `UArenaInventorySlotWidget` fallback；使用紧凑 `800×570` 左右分栏、固定五列四行与二十个 `76×76` 图标槽，物品名称和描述集中到右侧详情栏，底部分页与隐藏的 Drop 确认行保持稳定占位。
* 背包打开时使用全屏半透明遮罩降低战斗 HUD 干扰；槽位不再把完整物品名覆盖在图标上，只显示图标和右上角数量，详情栏负责名称、描述、Use、Drop 与数量确认。
* `UArenaInventoryWidget.InventorySlotWidgetClass` 允许二十槽 fallback 使用项目专属槽位 WBP；未配置时安全回退原生槽位。
* 资产脚本已幂等创建 `/Game/UI/Inventory/WBP_Inventory` 和 `WBP_InventorySlot`，连接两者并将 `BP_ArenaPlayerController.InventoryWidgetClass` 指向正式背包 View；保存后重新加载验证通过。
* 已提供 `K2_OnInventoryPageUpdated`、`K2_OnInventoryHidden` 和 `K2_OnSlotDataUpdated`，后续可直接美化两个 WBP 而继续消费同一份只读 ViewData。
* `ArenaInventory::CalculatePageCount()`、`ApplyStackAddition()`、`IsValidDropQuantity()`、`ShouldCloseOnTabRelease()`、`MatchesFilters()`、`UArenaItemDataAsset::IsUseConfigurationValid()` 和 `AArenaGameState` 阶段权限是运行时与自动化测试共用的规则路径；`Pagination` 覆盖 `0/19/20/21/40/41`，`Stacking` 覆盖补栈、溢出拆栈及正数/超量 Drop 边界，`TabGesture` 覆盖轻点保持、长按关闭、原本已打开和迟到松开，`Filters` 覆盖 Tag 规则，`PhaseAccess` 覆盖完整权限矩阵，`ItemValidation` 覆盖身份、恢复和冷却资产约束，`ReplicationContract` 固化 PlayerState 默认子对象、组件默认复制和 `COND_OwnerOnly` FastArray 契约。
* `AArenaPlayerController` 持有本地页码、筛选 Tag 和选中 StackId；`Tab`、`G` 已接入角色默认 Enhanced Input。
* `AArenaPlayerController.InventoryHoldThreshold` 默认 `0.25s`；Character、Upgrade 和背包的 `GameAndUI` 焦点路径共用同一按下/松开状态机，键盘重复不会重置长按时间。Upgrade Widget 优先把初始焦点交给第一个有效且可聚焦的候选按钮，缺失时才回退根节点；父级 Preview/KeyDown 路径仍会先拦截并转发 Tab。背包根 Widget 同样在 Slate 重建前启用焦点，避免蓝图按钮或 Slate 导航禁用只读背包。
* 背包打开时使用 `GameAndUI`、停止 Sprint 和本地移动/Look/主动技能输入，关闭后按原顶视角或第三人称状态恢复鼠标和准星；顶视角保留当前鼠标屏幕位置。
* `AArenaPlayerController::RefreshLocalUIInputLocks()` 是本地 UI 输入冻结的唯一所有者：它根据复制的 Upgrade 阶段以及 Inventory、BossIntro、BossOutro 和 Victory 的模式集合幂等持有至多一层 Move/Look 锁，不再让多个界面直接操作 `SetIgnoreMoveInput()` 的计数栈。玩家提前完成升级选择后仍保持冻结，直到全员完成并进入 Combat；模式交错、复制回调顺序变化和销毁清理不会留下额外锁层。
* `Upgrade` 中打开背包会临时折叠升级选择并保持背包只读；升级 UI 向背包转交输入时不会调用 `FlushPressedKeys()` 或恢复游戏视角，关闭背包后再恢复当前候选与 `GameAndUI` 输入。选择升级进入 Combat 后，统一协调器会在没有其他占用模式时一次性恢复移动与观察。
* Controller 同时监听当前 PlayerState ASC 的 `State.Dead`、`State.Stunned` 与 `Cooldown.Item.Consumable` Tag：状态变化会立即刷新背包权限，共享冷却期间只禁用 Use，Drop、筛选和翻页不受影响；PlayerState 更换、重连和 Controller 销毁时对称解绑，避免过期按钮或重复回调。
* 单人 PIE 已确认真实 Widget 在 `0/19/20/21/40/41` 个堆栈下正确计算页数，第 `11` 个同类药水会拆出新堆栈，删除唯一末页堆栈后页码会自动回退，多 Tag 筛选按 OR 语义恢复正确物品集合和顺序。Tab 轻点、长按、Escape、连续快速开关十次及背包打开时重新 Possess 均未残留移动/观察锁；顶视角与第三人称关闭背包后均恢复鼠标、准星和移动。Upgrade 阶段打开只读背包会暂时隐藏候选，关闭后仍可完成选择并恢复移动。

完成标准：顶视角和第三人称均可打开、操作和关闭背包，输入、鼠标和准星不会残留或串到玩法层。

### 阶段五 C-D：网络验收与收尾

状态：`Partial`，源码权威与生命周期审计、七项 Automation Tests、单人堆叠/筛选和输入生命周期 PIE 已完成；Dead/Stunned、完整阶段权限、分辨率布局及多人/Dedicated Server 验收待执行。

* 完成单人、两人 Listen Server、Dedicated Server 双客户端和异常生命周期回归。
* 检查 OwnerOnly 复制、同物竞争、迟到 RPC、关卡重载和 UI Delegate 清理。
* 已统一多界面输入锁所有权；`AArenaPlayerController::ResetIgnoreInputFlags()` 会在 `ClientRestart` 清空引擎 IgnoreInput 计数后重置项目记账并按当前 UI 阶段重新持锁，避免重生/重新 Possess 期间实际输入锁与布尔状态失配。延迟 ASC/冷却委托重绑、销毁解绑、Drop Pawn 归属、Pickup 失败回滚及非法数量拒绝也已补齐；丢弃后由 ItemData 重新构建对应红/蓝药水世界外观，不再退回通用球体。
* 单人权威流程已确认 Health/Energy Potion 正常恢复、满资源不消耗、共享一秒冷却、部分丢弃、整组丢弃、重新拾取、同类堆叠溢出、末页删除回退、OR 筛选、Tab/Escape 输入生命周期、重新 Possess，以及 Pickup 事务失败后的碰撞和消费门闩回滚；状态/阶段边界、OwnerOnly、多客户端竞争、迟到 RPC 与 Dedicated Server 仍待验证。
* 根据实际验证更新 `IMPLEMENTED_FEATURES.md` 和 `PENDING_VERIFICATION.md`。

完成标准：背包数据、恢复、丢弃和世界 Actor 由服务器唯一决定，各端 UI 与所属玩家复制状态一致。

---

## 公开接口草案

```text
UArenaItemDataAsset
FArenaInventoryEntry
FArenaInventoryList
UArenaInventoryComponent
AArenaInventoryPickupActor
UArenaInventoryWidget
UArenaInventorySlotWidget

UArenaInventoryComponent::TryAddItem(...)
UArenaInventoryComponent::TryUseItem(...)
UArenaInventoryComponent::TryDropItem(...)
UArenaInventoryComponent::GetInventorySnapshot()

AArenaPlayerController::ServerInteractWithInventoryPickup(...)
AArenaPlayerController::ServerUseInventoryItem(...)
AArenaPlayerController::ServerDropInventoryItem(...)
```

具体参数在阶段五 C-A 实现时以 StackId、服务器权限和反射/复制安全为准，但不得改变本文档定义的 MVC 所有权。

---

## 验收清单

* 验证 `0/19/20/21/40/41` 个堆栈的页数、翻页、删除回退和筛选后页数。
* 验证同类第十一个物品开始新堆栈，多 Tag OR 与父标签筛选准确。
* 验证 Health/Energy 恢复、满资源失败、共享冷却、Dead/Stunned/只读阶段阻断和 GAS 复制。
* 验证 Waiting/Combat/Upgrade/BossIntro/BossOutro/Victory/Defeat 权限矩阵，以及测试 GameMode 的 Waiting 完整操作开关。
* 验证部分/整组丢弃、地面生成、重新拾取和两人竞争同一 Pickup。
* 验证背包只复制给拥有者，客户端伪造 StackId、数量、Actor 或迟到请求均被服务器拒绝。
* 验证死亡、快速 Tab、阶段切换、重叠 UI、关卡旅行和 Widget 销毁会清理页面、弹窗、Delegate 与统一 UI 输入锁。
* 分别在顶视角和第三人称验证 Tab、G、鼠标、准星、移动和主动技能输入恢复，并在 720p 与 1080p 检查槽位文字、详情操作和分页不重叠。

---

## 阶段边界

第一版不实现 SaveGame、跨局持久化、装备、耐久、商店、热键栏、拖拽排序、槽位交换或背包容量上限。

新增 C++ 类型后，在本机执行：

```text
"E:\Unreal engine\UnrealEngine\GenerateProjectFiles.bat" -project="E:\UE_DEMO\ProjectArcaneArena\ProjectArcaneArena.uproject" -game -engine -2022
```

项目编译继续遵守 `AGENTS.md` 的源码引擎和 Live Coding 安全规则。

编译完成后先执行 `Automation RunTests ProjectArcaneArena.Inventory`；七项自动化验证分页、堆叠、Tab 手势、筛选、阶段权限、物品资产约束和 OwnerOnly 结构契约，FastArray 实际 Delta、Widget、输入和网络可见性仍必须按 PIE 清单验收。
