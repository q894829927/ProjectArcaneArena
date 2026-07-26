# Project Arcane Arena 背包系统开发计划

## 文档职责

本文档是阶段五 C“轻量 MVC 背包系统”的开发规范，维护系统边界、阶段顺序、公开接口和验收标准。

状态：`Planned`，最后更新：2026-07-27。

当前只完成方案记录，尚未创建背包 C++ 类型、资产或 UI。开始实现后，必须同步更新本文档、`IMPLEMENTED_FEATURES.md` 和 `PENDING_VERIFICATION.md`。

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
* Drop 数量面板限制输入范围为 `1` 到当前复制数量，但服务器仍使用最新权威数量重新 Clamp 或拒绝。
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

* `Tab` 只允许在 `Combat` 打开背包；打开后世界不暂停，玩家仍会受到服务器伤害。
* 打开背包时停止本地移动、Look、Sprint 和主动技能输入；关闭后恢复原顶视角或第三人称鼠标、准星和输入模式。
* 药水通过双击槽位或选中后点击 Use 使用。
* 服务器只允许处于 `Combat`、存活、未眩晕且没有 `Cooldown.Item.Consumable` 的玩家使用。
* 服务器通过配置的 GameplayEffect 和 SetByCaller 恢复资源；只有属性实际增加时才扣除一个物品并应用共享冷却。
* 资源已满、GE 配置无效或状态不允许时，不扣数量、不添加冷却。

### 丢弃

* 选中 Drop 后显示数量面板，可选择 `1` 到当前堆栈数量。
* 服务器验证 StackId 和数量后，在玩家前方安全地面生成对应 ItemData/Quantity 的复制 Pickup，再扣除背包数量。
* 找不到合法生成位置时不扣除物品。
* 新 Pickup 对丢弃者提供短暂拾取保护，避免角色仍在重叠范围时立即捡回；其他玩家可以正常拾取。

---

## 开发阶段

### 阶段五 C-A：数据与复制 Model

状态：`Planned`。

* 新增 Item DataAsset、FastArray Entry/List 和 `UArenaInventoryComponent`。
* 在 `AArenaPlayerState` 创建组件并完成 OwnerOnly 增量复制、变化 Delegate 和服务器查询/修改接口。
* 实现补栈、拆栈、压紧顺序和 StackId 防错操作。

完成标准：服务器能稳定保存任意数量堆栈；所属客户端收到相同顺序和数量，其他客户端看不到私有背包数据。

### 阶段五 C-B：权威物品流程

状态：`Planned`。

* 新增交互 Pickup、G 目标选择、服务器距离/视线验证和竞争消费门闩。
* 实现药水 GAS 使用、共享冷却、满资源失败保护和部分丢弃。
* 创建 Health Potion、Energy Potion、Cooldown GE 和对应可入包 Pickup 资产。

完成标准：拾取、使用和丢弃均只结算一次，客户端不能伪造物品、恢复或世界 Pickup。

### 阶段五 C-C：分页筛选与双视角 UI

状态：`Planned`。

* 新增 `WBP_Inventory`、`WBP_InventorySlot` 和 Drop 数量面板。
* 实现二十槽分页、多 Tag OR 筛选、双击/Use、翻页和当前页 Clamp。
* 接入 Tab/G Enhanced Input，并与 Upgrade、BossIntro、BossOutro、Victory、Dead 和 Stunned 状态协调输入清理。

完成标准：顶视角和第三人称均可打开、操作和关闭背包，输入、鼠标和准星不会残留或串到玩法层。

### 阶段五 C-D：网络验收与收尾

状态：`Planned`。

* 完成单人、两人 Listen Server、Dedicated Server 双客户端和异常生命周期回归。
* 检查 OwnerOnly 复制、同物竞争、迟到 RPC、关卡重载和 UI Delegate 清理。
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
* 验证 Health/Energy 恢复、满资源失败、共享冷却、Dead/Stunned/非 Combat 阻断和 GAS 复制。
* 验证部分/整组丢弃、地面生成、重新拾取和两人竞争同一 Pickup。
* 验证背包只复制给拥有者，客户端伪造 StackId、数量、Actor 或迟到请求均被服务器拒绝。
* 验证死亡、阶段切换、关卡旅行和 Widget 销毁会清理页面、弹窗、Delegate 与输入状态。
* 分别在顶视角和第三人称验证 Tab、G、鼠标、准星、移动和主动技能输入恢复。

---

## 阶段边界

第一版不实现 SaveGame、跨局持久化、装备、耐久、商店、热键栏、拖拽排序、槽位交换或背包容量上限。

新增 C++ 类型后，在本机执行：

```text
"E:\Unreal engine\UnrealEngine\GenerateProjectFiles.bat" -project="E:\UE_DEMO\ProjectArcaneArena\ProjectArcaneArena.uproject" -game -engine -2022
```

项目编译继续遵守 `AGENTS.md` 的源码引擎和 Live Coding 安全规则。
