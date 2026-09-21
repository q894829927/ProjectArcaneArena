# Project Arcane Arena 资产路径迁移记录

本文档是本项目 Content 目录重构的**唯一迁移映射记录**。后续修改 C++、Config、Python、GameplayCue 扫描路径、Cook 路径、软引用和文档中的硬编码 `/Game/...` 路径时，以本表为准。

> 状态说明：`Planned` = 尚未迁移；`Moved` = 已在 Unreal Editor 中移动但旧 Redirector 仍保留；`Rewritten` = 代码/配置/脚本硬编码路径已同步；`Verified` = 重启 Editor、PIE/构建/关键资产加载验证通过并清理 Redirector。

## 1. 规范根目录

所有项目自研资产逐步收敛到：

```text
/Game/ProjectArcaneArena/
```

第三方、Marketplace、引擎示例或外部资源目录不为了“整齐”强行迁移，例如：

```text
/Game/SlashTrail_SoftTofu
/Game/ParagonMuriel
/Game/CombatMagicAnims
/Game/wukongManny
/Game/Characters/Mannequins
/Game/LevelPrototyping
```

## 2. Canonical Migration Map

| 状态 | 当前路径 | 目标路径 | 备注 |
|---|---|---|---|
| Planned | `/Game/GameMode` | `/Game/ProjectArcaneArena/Core/GameMode` | GameMode 相关 Blueprint/资产统一进入 Core |
| Planned | `/Game/Characters/ArenaPlayer` | `/Game/ProjectArcaneArena/Characters/Player` | 玩家 Character、AnimBP 等 |
| Planned | `/Game/Characters/ArenaEnemy` | `/Game/ProjectArcaneArena/Characters/Enemies` | 普通、远程、Elite 等敌人 |
| Planned | `/Game/Boss` | `/Game/ProjectArcaneArena/Characters/Boss` | Boss Character、AI、Animation、GAS、VFX 等整体迁移 |
| Planned | `/Game/GAS` | `/Game/ProjectArcaneArena/Combat/GAS` | GameplayAbility、GameplayEffect、GameplayCue、Area 等 |
| Planned | `/Game/Data/Upgrade` | `/Game/ProjectArcaneArena/Systems/Upgrades/Data` | Roguelike Upgrade DataAsset |
| Planned | `/Game/Blueprints/DataAsset/DA_Waves_Prototype` | `/Game/ProjectArcaneArena/Systems/Waves/Data/DA_Waves_Prototype` | 单资产迁移；后续新增 Wave Data 也放此目录 |
| Planned | `/Game/UI` | `/Game/ProjectArcaneArena/UI` | HUD、MainMenu、Upgrade、Inventory、DamageNumber 等 |
| Planned | `/Game/TopDown/Lvl_TopDown` | `/Game/ProjectArcaneArena/World/Maps/Lvl_Arena` | **同时改名**：`Lvl_TopDown -> Lvl_Arena` |
| Planned | 项目自有 `/Game/Niagara` | `/Game/ProjectArcaneArena/VFX/Common` | 只迁项目自制资产；第三方 Niagara 不迁 |
| Planned | 新 Weapon 资产 | `/Game/ProjectArcaneArena/Combat/Weapons` | 新增资产直接使用目标路径，无旧路径 |
| Planned | 新 Data Projectile 资产 | `/Game/ProjectArcaneArena/Combat/Projectiles` | 新增资产直接使用目标路径，无旧路径 |

## 3. 后续硬编码路径替换基准

后续代码和脚本迁移时优先按以下映射搜索：

```text
/Game/GameMode
    -> /Game/ProjectArcaneArena/Core/GameMode

/Game/Characters/ArenaPlayer
    -> /Game/ProjectArcaneArena/Characters/Player

/Game/Characters/ArenaEnemy
    -> /Game/ProjectArcaneArena/Characters/Enemies

/Game/Boss
    -> /Game/ProjectArcaneArena/Characters/Boss

/Game/GAS
    -> /Game/ProjectArcaneArena/Combat/GAS

/Game/Data/Upgrade
    -> /Game/ProjectArcaneArena/Systems/Upgrades/Data

/Game/Blueprints/DataAsset/DA_Waves_Prototype
    -> /Game/ProjectArcaneArena/Systems/Waves/Data/DA_Waves_Prototype

/Game/UI
    -> /Game/ProjectArcaneArena/UI

/Game/TopDown/Lvl_TopDown
    -> /Game/ProjectArcaneArena/World/Maps/Lvl_Arena
```

`/Game/Niagara` **不能做无脑全局字符串替换**，必须逐项确认资产所有权；第三方或历史模板引用保持原路径。

## 4. P5 / Weapon 新资产固定目录

### 4.1 Data Projectile

```text
/Game/ProjectArcaneArena/Combat/Projectiles/
├─ Data/
├─ Materials/
├─ Meshes/
└─ VFX/
   ├─ DataChannels/
   │  ├─ NDC_ArenaProjectiles
   │  └─ NDC_ArenaProjectileImpacts
   ├─ Systems/
   │  └─ NS_ArenaProjectiles_Shared
   └─ Emitters/
```

P5 最终固定对象路径：

```text
/Game/ProjectArcaneArena/Combat/Projectiles/VFX/DataChannels/NDC_ArenaProjectiles.NDC_ArenaProjectiles

/Game/ProjectArcaneArena/Combat/Projectiles/VFX/DataChannels/NDC_ArenaProjectileImpacts.NDC_ArenaProjectileImpacts

/Game/ProjectArcaneArena/Combat/Projectiles/VFX/Systems/NS_ArenaProjectiles_Shared.NS_ArenaProjectiles_Shared
```

当前 P5 C++ 若仍引用旧的 `/Game/Projectile/VFX/...`，必须在创建正式资产前同步到以上路径。

### 4.2 Weapons

```text
/Game/ProjectArcaneArena/Combat/Weapons/
├─ Data/
├─ Meshes/
├─ Materials/
├─ VFX/
└─ Audio/
```

当前/后续 Weapon DataAsset 建议统一为：

```text
/Game/ProjectArcaneArena/Combat/Weapons/Data/
├─ DA_Weapon_ArcaneBolt
├─ DA_Weapon_ArcaneBoltFast
└─ DA_Weapon_Shotgun
```

## 5. 目标目录树

```text
/Game/ProjectArcaneArena/
├─ Core/
│  └─ GameMode/
├─ Characters/
│  ├─ Player/
│  ├─ Enemies/
│  └─ Boss/
├─ Combat/
│  ├─ GAS/
│  ├─ Weapons/
│  └─ Projectiles/
├─ Systems/
│  ├─ Waves/
│  │  └─ Data/
│  └─ Upgrades/
│     └─ Data/
├─ UI/
├─ World/
│  └─ Maps/
└─ VFX/
   └─ Common/
```

## 6. 迁移执行规则

1. 资产只能通过 Unreal Editor Content Browser 的 **Move** 操作迁移，不使用 Windows Explorer 直接剪切 `.uasset`。
2. 每次迁移前保存全部资产，并保留 Git checkpoint。
3. 迁移完成但硬编码路径尚未修改时，保留 Redirector，状态记为 `Moved`。
4. 同步修改 C++、`.ini`、Python、GameplayCue 扫描路径、Cook 路径和文档后，状态改为 `Rewritten`。
5. 完成 Editor 重启、PIE、窄目标编译、关键资产加载和必要的 Packaged/Cook 验证后，才允许 Fix Up Redirectors，并把状态改为 `Verified`。
6. 不对第三方资源目录执行批量迁移或无脑字符串替换。
7. 如果一个旧目录中同时存在项目自有资产和第三方资产，必须按资产逐个确认，不以文件夹整体迁移。

## 7. 已知需要同步修改的硬编码来源

迁移过程中至少检查以下类别：

- `Config/DefaultEngine.ini`
  - `EditorStartupMap`
  - `GameDefaultMap`
  - `GlobalDefaultGameMode`
- `Config/DefaultGame.ini`
  - `GameplayCueNotifyPaths`
  - `MapsToCook`
  - `DirectoriesToAlwaysCook`
- `Content/Python/**`
  - GAS 生成脚本
  - Boss setup 脚本
  - Ranged / Elite Enemy 脚本
  - Upgrade / Balance / DamageFeedback / Inventory 脚本
- `Source/ProjectArcaneArena/**`
  - `LoadObject`
  - `ConstructorHelpers`
  - SoftObjectPath / SoftClassPath
  - 直接写死的 `/Game/...` 字符串
- 项目设计/验收文档中的旧资产路径。

当前已知 P5 临时旧路径：

```text
/Game/Projectile/VFX/NDC_ArenaProjectiles
/Game/Projectile/VFX/NDC_ArenaProjectileImpacts
/Game/Projectile/VFX/NS_ArenaProjectiles_Shared
```

最终全部替换为第 4.1 节中的正式路径。

## 8. 迁移状态记录方式

每完成一批迁移，直接更新第 2 节表格状态，并在本节追加一次记录：

```text
YYYY-MM-DD
Batch:
Moved:
Rewritten:
Verified:
Remaining:
Notes:
```

当前记录：

```text
2026-09-22
Batch: Migration Plan
Moved: None
Rewritten: None
Verified: None
Remaining: 全部正式迁移
Notes: 已锁定 Canonical Migration Map；后续路径修改必须以本文档为准。
```
