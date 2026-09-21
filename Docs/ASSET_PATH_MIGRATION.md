# Project Arcane Arena 资产路径迁移记录

本文档是本项目 Content 目录重构的**唯一迁移映射记录**。后续修改 C++、Config、Python、GameplayCue 扫描路径、Cook 路径、软引用和文档中的硬编码 `/Game/...` 路径时，以本表为准。

> 全量文件级清单与迁移完成后的分类目录见：`Docs/CONTENT_ASSET_INVENTORY_AND_TARGET_LAYOUT.md`。该文档基于当前 Git tree 自动统计，包含本次范围内全部 Content 文件。

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
| Rewritten | `/Game/GameMode` | `/Game/ProjectArcaneArena/Core/GameMode` | GameMode 相关 Blueprint/资产统一进入 Core |
| Rewritten | `/Game/Characters/ArenaPlayer` | `/Game/ProjectArcaneArena/Characters/Player` | 玩家 Character、AnimBP 等 |
| Rewritten | `/Game/Characters/ArenaEnemy` | `/Game/ProjectArcaneArena/Characters/Enemies` | 普通、远程、Elite 等敌人 |
| Rewritten | `/Game/Boss` | `/Game/ProjectArcaneArena/Characters/Boss` | Boss Character、AI、Animation、GAS、VFX 等整体迁移 |
| Rewritten | `/Game/GAS/GameplayAbility` | `/Game/ProjectArcaneArena/Combat/GAS/Abilities` | 再按 Player / Enemy / Triggers 分类 |
| Rewritten | `/Game/GAS/GameplayEffect` | `/Game/ProjectArcaneArena/Combat/GAS/Effects` | 再按 Core / Init / Cooldowns / Costs / Status / Triggers / Upgrades / Enemy 分类 |
| Rewritten | `/Game/GAS/GameplayCues` | `/Game/ProjectArcaneArena/Combat/GAS/Cues` | `InstaneCue` 同时纠正为 `Instant`，`DurationCue` 统一为 `Looping` |
| Rewritten | `/Game/GAS/Area` | `/Game/ProjectArcaneArena/Combat/Areas` | Area Actor 不是 GAS 资产，从 GAS 根中拆出 |
| Rewritten | `/Game/GAS/DamageFeedback` | `/Game/ProjectArcaneArena/Combat/Feedback` | CameraShake / Material / Sound 等表现资产从 GAS 根中拆出 |
| Rewritten | `/Game/GAS/Projectile` | `/Game/ProjectArcaneArena/Combat/Projectiles/Actors` | 旧 Fireball / Enemy Actor Projectile；与新 Data Projectile 共用 Projectile 功能域 |
| Rewritten | `/Game/Data/Upgrade` | `/Game/ProjectArcaneArena/Systems/Upgrades/Data` | Roguelike Upgrade DataAsset |
| Rewritten | `/Game/Blueprints/DataAsset/DA_Waves_Prototype` | `/Game/ProjectArcaneArena/Systems/Waves/Data/DA_Waves_Prototype` | 单资产迁移；后续新增 Wave Data 也放此目录 |
| Rewritten | `/Game/UI` | `/Game/ProjectArcaneArena/UI` | HUD、MainMenu、Upgrade、Inventory、DamageNumber 等 |
| Rewritten | `/Game/TopDown/Lvl_TopDown` | `/Game/ProjectArcaneArena/World/Maps/Lvl_Arena` | **同时改名**：`Lvl_TopDown -> Lvl_Arena` |
| Rewritten | 项目自有 `/Game/Niagara` | `/Game/ProjectArcaneArena/VFX/Common` | 只迁项目自制资产；第三方 Niagara 不迁 |
| Rewritten | `/Game/Data/Weapon` | `/Game/ProjectArcaneArena/Combat/Weapons/Data` | 现有 ArcaneBolt / Fast / Shotgun DataAsset |
| Planned | 新 Weapon 资产 | `/Game/ProjectArcaneArena/Combat/Weapons` | 新增资产直接使用目标路径，无旧路径 |
| Planned | 新 Data Projectile 资产 | `/Game/ProjectArcaneArena/Combat/Projectiles` | 新增资产直接使用目标路径，无旧路径 |
| Rewritten | `/Game/Blueprints/ArenaLightningStormArea` | `/Game/ProjectArcaneArena/Combat/Areas/LightningStorm` | 与 DashTrail Area 统一归入 Combat/Areas |
| Rewritten | `/Game/Data/EnemyAffix` | `/Game/ProjectArcaneArena/Characters/Enemies/Data/Affixes` | Elite/Affix 配置归敌人功能域 |
| Rewritten | `/Game/Items/Inventory` | `/Game/ProjectArcaneArena/Systems/Inventory` | Inventory Data / Effects / Pickup Item 资产 |
| Rewritten | `/Game/Items/Pickups` | `/Game/ProjectArcaneArena/Systems/Pickups/Blueprints` | Health/Energy Pickup Actor Blueprint |
| Rewritten | `/Game/Data/Pickup` | `/Game/ProjectArcaneArena/Systems/Pickups/Data` | Pickup DropTable / 配置 |
| Rewritten | `/Game/Assets/Pickups` | `/Game/ProjectArcaneArena/Systems/Pickups/Art` | 仅项目自有美术；若确认来自外部包则保持原目录 |
| Rewritten | `/Game/Core/BP_ArenaPlayerController` | `/Game/ProjectArcaneArena/Core/Controllers/BP_ArenaPlayerController` | Controller 资产从旧根 Core 收敛 |
| Rewritten | `/Game/TopDown/Input` | `/Game/ProjectArcaneArena/Input` | Enhanced Input Actions / Mapping Context |
| Rewritten | `/Game/TopDown/Blueprints` | `/Game/ProjectArcaneArena/Dev/LegacyTemplate/TopDown` 或删除 | 先用 Reference Viewer 确认模板 BP 是否仍被正式项目引用 |
| Rewritten | `/Game/TopDown/Cursor` + `/Game/Cursor` | `/Game/ProjectArcaneArena/UI/Cursor` | 两套存在同名资产，必须先确认实际引用后再合并，不能直接覆盖 |
| Rewritten | `/Game/TopDown/MI_Colorway` | `/Game/ProjectArcaneArena/World/Materials/MI_Colorway` | 地图/环境材质 |
| Rewritten | `/Game/Mass` | `/Game/ProjectArcaneArena/Dev/Experiments/Mass` | 当前 MassCluster 属学习/实验对照，不进入正式 Gameplay 根 |
| Rewritten | `/Game/Tests` | `/Game/ProjectArcaneArena/Dev/Tests` | Overload / PCG / Projectile 测试资产 |

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

/Game/GAS/GameplayAbility
    -> /Game/ProjectArcaneArena/Combat/GAS/Abilities

/Game/GAS/GameplayEffect
    -> /Game/ProjectArcaneArena/Combat/GAS/Effects

/Game/GAS/GameplayCues
    -> /Game/ProjectArcaneArena/Combat/GAS/Cues

/Game/GAS/Area
    -> /Game/ProjectArcaneArena/Combat/Areas

/Game/GAS/DamageFeedback
    -> /Game/ProjectArcaneArena/Combat/Feedback

/Game/GAS/Projectile
    -> /Game/ProjectArcaneArena/Combat/Projectiles/Actors

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

## 4. GAS 与 Combat 详细规划

旧 `/Game/GAS` 实际混合了 **GAS 配置资产、Gameplay Actor、Projectile Actor 和表现资产**。迁移时不再把整个目录原样塞进新的 `Combat/GAS`；只保留真正属于 GAS 的 Ability / Effect / Cue，其余按职责拆出。

### 4.1 GAS 最终结构

```text
/Game/ProjectArcaneArena/Combat/GAS/
├─ Abilities/
│  ├─ Player/
│  │  ├─ GA_BasicAttack
│  │  ├─ GA_Dash
│  │  ├─ GA_DashLightningTrail
│  │  ├─ GA_Fireball
│  │  ├─ GA_LightningStorm
│  │  ├─ GA_Overload
│  │  ├─ GA_Shield
│  │  └─ GA_ShieldBreakBlast
│  ├─ Enemy/
│  │  ├─ GA_EnemyMeleeAttack
│  │  └─ GA_EnemyRangedAttack
│  └─ Triggers/
│     ├─ GA_EnergyOnAbilityCast
│     ├─ GA_EnergyOnCrit
│     └─ GA_EnergyOnKill
│
├─ Effects/
│  ├─ Core/
│  │  ├─ GE_Damage
│  │  ├─ GE_Shield
│  │  └─ GE_Shield_Grant
│  ├─ Init/
│  │  ├─ GE_Init_PlayerAttributes
│  │  └─ GE_Init_EnemyAttributes
│  ├─ Cooldowns/
│  │  ├─ GE_Cooldown_BasicAttack
│  │  ├─ GE_Cooldown_Dash
│  │  ├─ GE_Cooldown_Fireball
│  │  ├─ GE_Cooldown_LightningStorm
│  │  ├─ GE_Cooldown_Shield
│  │  ├─ GE_Cooldown_EnemyMeleeAttack
│  │  └─ GE_Cooldown_EnemyRangedAttack
│  ├─ Costs/
│  │  ├─ GE_Cost_Fireball
│  │  ├─ GE_Cost_LightningStorm
│  │  └─ GE_Cost_Shield
│  ├─ Status/
│  │  ├─ GE_Status_Stunned
│  │  ├─ GE_Status_Burning
│  │  ├─ GE_Status_Shocked
│  │  └─ GE_Status_OverloadLockout
│  ├─ Triggers/
│  │  ├─ GE_Trigger_EnergyOnAbilityCast
│  │  ├─ GE_Trigger_EnergyOnCrit
│  │  └─ GE_Trigger_EnergyOnKill
│  ├─ Upgrades/
│  │  ├─ GE_Upgrade_AttackPower
│  │  ├─ GE_Upgrade_CritChance
│  │  ├─ GE_Upgrade_MaxHealth
│  │  └─ GE_Upgrade_MoveSpeed
│  └─ Enemy/
│     └─ Elite/
│        ├─ GE_Elite_BaseAttributes
│        └─ GE_Elite_Frenzy
│
└─ Cues/
   ├─ Instant/
   │  ├─ GCN_BasicAttack_Activate
   │  ├─ GCN_DamageCritical
   │  ├─ GCN_DamageNumber
   │  ├─ GCN_EnemyMelee_Activate
   │  ├─ GCN_Fireball_Cast
   │  ├─ GCN_HealthHit
   │  ├─ GCN_Hit_Fire
   │  ├─ GCN_Hit_Lightning
   │  ├─ GCN_Hit_Physical
   │  ├─ GCN_LightningStorm_Cast
   │  ├─ GCN_Overload_Explosion
   │  ├─ GCN_ShieldBreak
   │  ├─ GCN_ShieldBreakHealthHit
   │  ├─ GCN_ShieldBreak_Burst
   │  └─ GCN_ShieldHit
   ├─ Looping/
   │  ├─ GCN_Burning_Active
   │  ├─ GCN_DashLightningTrail_Active
   │  ├─ GCN_Dash_Active
   │  ├─ GCN_LightningStorm_Active
   │  ├─ GCN_Shield_Active
   │  └─ GCN_Shocked_Active
   └─ Elite/
      ├─ GCN_Elite_ArcaneWarden_Active
      ├─ GCN_Elite_ArcaneWarden_Pulse
      ├─ GCN_Elite_Frenzy_Active
      ├─ GCN_Elite_Frenzy_Trigger
      ├─ GCN_Elite_Volatile_Active
      ├─ GCN_Elite_Volatile_Explode
      └─ GCN_Elite_Volatile_Telegraph
```

说明：

- 旧目录 `InstaneCue` 拼写错误，迁移时统一改成 `Cues/Instant`。
- 旧 `DurationCue` 迁移为 `Cues/Looping`，更贴合项目中持续 GameplayCue 的实际用途。
- Boss 的专属 Ability / Effect / Cue **继续归 `Characters/Boss/GAS`**，不重新塞回公共 `Combat/GAS`。公共 GAS 只放可复用或常规玩家/敌人战斗资产。
- `Effects/Upgrades` 存放真正的 GameplayEffect；`Systems/Upgrades/Data` 存放 `UArenaUpgradeDataAsset`。两者不要混在同一个 Data 目录。

### 4.2 从旧 GAS 拆出的非 GAS 资产

```text
/Game/GAS/Area/BP_ArenaDashTrailArea
    -> /Game/ProjectArcaneArena/Combat/Areas/Dash/BP_ArenaDashTrailArea

/Game/Blueprints/ArenaLightningStormArea/BP_ArenaLightningStormArea
    -> /Game/ProjectArcaneArena/Combat/Areas/LightningStorm/BP_ArenaLightningStormArea

/Game/GAS/Projectile/BP_ArenaFireballProjectile
    -> /Game/ProjectArcaneArena/Combat/Projectiles/Actors/BP_ArenaFireballProjectile

/Game/GAS/Projectile/BP_ArenaEnemyProjectile
    -> /Game/ProjectArcaneArena/Combat/Projectiles/Actors/BP_ArenaEnemyProjectile
```

DamageFeedback 拆为：

```text
/Game/ProjectArcaneArena/Combat/Feedback/
├─ Camera/
│  ├─ CS_DamageLight
│  ├─ CS_DamageMedium
│  ├─ CS_DamageHeavy
│  └─ CS_ShieldBreak
├─ Materials/
│  └─ M_ArenaHitFlashOverlay
└─ Audio/
   └─ SA_ArenaHitFeedback
```

这类资产虽然由 GAS Damage 流程触发，但本质是 Presentation，不应因为“由 GAS 调用”就继续放在 GAS 目录。

### 4.3 GAS 路径迁移注意事项

`/Game/GAS` 现在**不能再做一条简单的全局 Prefix 替换**，因为旧目录已经被拆成多个职责域。后续脚本修改要按第 2 节和本节的具体子路径匹配。

特别需要修改：

- `generate_gameplay_ability_blueprints.py` → `Combat/GAS/Abilities/...`
- `generate_gameplay_effect_blueprints.py` → `Combat/GAS/Effects/...`
- `generate_burst_gameplay_cues.py` / `generate_looping_gameplay_cues.py` → `Combat/GAS/Cues/...`
- `configure_build_asset_links.py` 中 Ability / Effect / Area / Projectile 的路径分别指向对应新域。
- `DefaultGame.ini` 的 `GameplayCueNotifyPaths` 最终只需要扫描 `/Game/ProjectArcaneArena/Combat/GAS/Cues` 与 Boss Cue 路径；Cook 路径则按最终资产依赖和动态发现需求更新。

## 5. P5 / Weapon 新资产固定目录

### 5.1 Data Projectile

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

### 5.2 Weapons

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

## 6. 目标目录树

```text
/Game/ProjectArcaneArena/
├─ Core/
│  ├─ GameMode/
│  └─ Controllers/
├─ Characters/
│  ├─ Player/
│  ├─ Enemies/
│  │  └─ Data/
│  │     └─ Affixes/
│  └─ Boss/
│     ├─ Character/
│     ├─ AI/
│     ├─ Animation/
│     ├─ GAS/
│     └─ VFX/
├─ Combat/
│  ├─ GAS/
│  │  ├─ Abilities/
│  │  ├─ Effects/
│  │  └─ Cues/
│  ├─ Areas/
│  ├─ Feedback/
│  ├─ Weapons/
│  └─ Projectiles/
│     ├─ Actors/
│     ├─ Data/
│     └─ VFX/
├─ Systems/
│  ├─ Waves/
│  │  └─ Data/
│  ├─ Upgrades/
│  │  └─ Data/
│  ├─ Inventory/
│  └─ Pickups/
│     ├─ Blueprints/
│     ├─ Data/
│     └─ Art/
├─ Input/
│  ├─ Actions/
│  └─ Contexts/
├─ UI/
│  ├─ HUD/
│  ├─ DamageNumbers/
│  ├─ MainMenu/
│  ├─ Upgrade/
│  ├─ Inventory/
│  └─ Cursor/
├─ World/
│  ├─ Maps/
│  └─ Materials/
├─ VFX/
│  └─ Common/
└─ Dev/
   ├─ Tests/
   ├─ Experiments/
   │  └─ Mass/
   └─ LegacyTemplate/
      └─ TopDown/
```

### 6.1 当前根目录的额外收敛建议

除最初迁移表外，当前仓库还存在一些项目自有根目录，建议一起收敛：

```text
/Game/Core/BP_ArenaPlayerController
    -> /Game/ProjectArcaneArena/Core/Controllers/

/Game/Data/Weapon
    -> /Game/ProjectArcaneArena/Combat/Weapons/Data/

/Game/Data/EnemyAffix
    -> /Game/ProjectArcaneArena/Characters/Enemies/Data/Affixes/

/Game/Items/Inventory
    -> /Game/ProjectArcaneArena/Systems/Inventory/

/Game/Items/Pickups
    -> /Game/ProjectArcaneArena/Systems/Pickups/Blueprints/

/Game/Data/Pickup
    -> /Game/ProjectArcaneArena/Systems/Pickups/Data/

/Game/Assets/Pickups
    -> /Game/ProjectArcaneArena/Systems/Pickups/Art/   （仅项目自有资产）

/Game/Tests
    -> /Game/ProjectArcaneArena/Dev/Tests/

/Game/Mass
    -> /Game/ProjectArcaneArena/Dev/Experiments/Mass/
```

`/Game/TopDown/Blueprints` 属模板遗留，先通过 Reference Viewer 判断是否仍被正式 GameMode/Character/Controller 引用。若无引用直接删除；若仍需保留用于对照，则迁入 `Dev/LegacyTemplate/TopDown`，不要继续作为正式 gameplay 路径。

`/Game/TopDown/Cursor` 与 `/Game/Cursor` 存在同名 Cursor 资产。必须先确认正式引用，选择一套作为主资产后再迁到 `UI/Cursor`，不能简单合并覆盖。

仓库根 `Content/3` 是约 100 MB 的非标准大文件，`Content/mcp-conversation-export.md` 也不是 Unreal Content 资产；迁移阶段单独确认用途，非必须文件应移出 `Content`，避免无意义进入资产目录/仓库体积管理。

## 7. 迁移执行规则

1. 资产只能通过 Unreal Editor Content Browser 的 **Move** 操作迁移，不使用 Windows Explorer 直接剪切 `.uasset`。
2. 每次迁移前保存全部资产，并保留 Git checkpoint。
3. 迁移完成但硬编码路径尚未修改时，保留 Redirector，状态记为 `Moved`。
4. 同步修改 C++、`.ini`、Python、GameplayCue 扫描路径、Cook 路径和文档后，状态改为 `Rewritten`。
5. 完成 Editor 重启、PIE、窄目标编译、关键资产加载和必要的 Packaged/Cook 验证后，才允许 Fix Up Redirectors，并把状态改为 `Verified`。
6. 不对第三方资源目录执行批量迁移或无脑字符串替换。
7. 如果一个旧目录中同时存在项目自有资产和第三方资产，必须按资产逐个确认，不以文件夹整体迁移。

## 8. 已知需要同步修改的硬编码来源

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

最终全部替换为第 5.1 节中的正式路径。

## 9. 迁移状态记录方式

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


## 10. 自动迁移脚本

仓库已提供：

```text
Scripts/Python/migration/migrate_content_layout.py
```

该脚本必须在 **Unreal Editor Python 环境**中执行，不能用系统 Python 直接搬 `.uasset/.umap`。Unreal 资产统一通过 `unreal.EditorAssetLibrary.rename_asset()` 迁移，普通 Python/Markdown/PNG 等辅助文件才使用文件系统移动。

### 10.1 默认 Dry Run

脚本默认：

```python
RUN_MODE = "dry_run"
ALLOW_APPLY = False
```

第一次执行只扫描当前 Asset Registry、检查目标冲突、输出 `Old -> New` 计划，并生成：

```text
Saved/MigrationReports/content_migration_*.log
```

不修改任何资产。

在 Unreal Editor 中可通过 **Tools -> Execute Python Script** 选择：

```text
E:\UE_DEMO\ProjectArcaneArena\Scripts\Python\migration\migrate_content_layout.py
```

### 10.2 真正执行

确认 Dry Run 中：

- `conflict=0`
- `review=0`，或已人工确认 Review 项
- Git 已建立迁移前 checkpoint

之后把脚本顶部改为：

```python
RUN_MODE = "all"
ALLOW_APPLY = True
```

`all` 会按顺序执行：

```text
Unreal Asset Move
    ↓
普通辅助文件移动
    ↓
Config / Source / Python 硬编码路径重写
    ↓
旧资产根与旧路径残留静态验证
```

支持的模式：

| RUN_MODE | 行为 |
|---|---|
| `dry_run` | 只生成计划，不修改 |
| `apply` | 只迁移 Unreal 资产和普通辅助文件 |
| `rewrite` | 只修改 Config / Source / Python 的旧硬编码路径 |
| `validate` | 只扫描旧资产根和旧字符串残留 |
| `all` | apply + rewrite + validate |

脚本具有幂等保护：目标已存在且旧位置只是 Redirector 时记为 `ALREADY`；如果旧资产和新资产同时真实存在，则记为 `CONFLICT` 并阻止 Apply。

### 10.3 地图和 External Actor

`Lvl_TopDown -> Lvl_Arena` 仍通过 Unreal Asset API 迁移。脚本永远不会直接移动：

```text
/Game/__ExternalActors__
/Game/__ExternalObjects__
```

地图相关 hash package 继续由 Unreal Engine 自动管理。

如果当前正在编辑的地图导致 `rename_asset` 失败，先打开其他地图，再重新执行脚本；已完成的资产会被幂等跳过。

### 10.4 Redirector

脚本**故意不自动 Fix Up Redirectors**。

必须先完成：

```text
Save All
→ 重启 Editor
→ ProjectArcaneArenaEditor 窄目标编译
→ PIE
→ 关键地图 / GAS / Wave / UI / Boss / Inventory 验证
→ validate 无真实旧资产残留
```

之后再在 Content Browser 对旧目录执行 Fix Up Redirectors。

这样即使迁移中途发现硬编码遗漏，仍可利用 Redirector 回退，不会过早删除兼容路径。

### 10.5 Dry Run 实际修正记录（2026-09-22）

首次 Dry Run 暴露了两个脚本层问题，已修正：

1. Unreal Asset Registry 会对部分 World 同时返回 `World` 与 `:PersistentLevel` 子对象。旧脚本会把 `Lvl_TopDown.Lvl_TopDown:PersistentLevel`、`Lvl_OverloadTest.Lvl_OverloadTest:PersistentLevel`误当成独立迁移项。现在统一规范化为所属 PackagePath，同一 `.umap` 只迁移一次。
2. 本地 `Content/Python/**/__pycache__/*.pyc` 是运行缓存，不属于源码或 Content 资产。现在 Dry Run / Apply 均忽略这些文件，不再迁到 `Scripts/Python`。

因此首次 Dry Run 报告不能直接进入 Apply；拉取上述修正后必须重新执行一次 `dry_run`，以第二次报告为准。

### 10.6 首次 Apply 中断记录（2026-09-22）

首次 `RUN_MODE="all"` 在第 `193/264` 个 Unreal 资产处中断：

```text
/Game/GameMode/BP_ArenaGameMode
-> /Game/ProjectArcaneArena/Core/GameMode/BP_ArenaGameMode
```

前 192 个资产已通过 Unreal API 成功迁移；由于资产阶段尚未完成，后续普通辅助文件移动、文本路径 Rewrite 和 Static Validation **均未执行**。因此当前工作区属于“部分资产已迁移、硬编码尚未重写”的中间状态，必须保留 Redirector，不得执行 Fix Up Redirectors。

迁移脚本已增加三层 Rename 策略：

```text
EditorAssetLibrary.rename_asset
    ↓ 失败
EditorAssetLibrary.rename_loaded_asset
    ↓ 失败
AssetTools.rename_assets
```

重新执行前先拉取脚本修正，并关闭可能正在打开的 `BP_ArenaGameMode` Blueprint Editor；建议打开一个不在迁移清单内的中立地图。脚本会把已经迁移成功的资产识别为 `ALREADY`，只继续剩余项。


### 10.7 Source Art 与 Auto Reimport 修正（2026-09-22）

第二次 Apply 已完成剩余 Unreal 资产迁移，包括 `BP_ArenaGameMode` 和全部待迁地图；其中部分 Blueprint/Map 通过 AssetTools fallback 成功。随后在普通辅助文件阶段，脚本把 Upgrade Icon 原始 PNG 搬进了 `Content/ProjectArcaneArena/UI/Upgrade/Icons`，触发 Unreal 的 Source Content Auto Reimport，并因某个目标 PNG 已存在而中断。

现已修正：

```text
Imported Texture (.uasset)
    -> /Game/ProjectArcaneArena/UI/Upgrade/Icons

Raw Source PNG
    -> <Project>/SourceArt/UI/UpgradeIcons
```

原始 PNG 不再放在 `Content`，避免 Auto Reimport 弹窗和 Cook/Asset Registry 噪音。脚本也支持中断续跑：如果目标普通文件已存在且与源文件二进制完全一致，会记录 `[FILE ALREADY]` 并删除重复源；内容不同才视为冲突并停止。

本次中断发生在辅助文件阶段，因此 Unreal 资产迁移已经完成，但 Text Rewrite / Static Validation 尚未执行。拉取修正版后重新执行 `all` 即可继续，不需要回滚已完成的资产迁移。


### 10.8 Final Rewrite Validation Cleanup（2026-09-22）

第三次 `all` 已达到：

```text
move=0
conflict=0
review=0
already_target=264
Asset/file apply complete
Text rewrite complete: changed_files=53
Validation: no non-redirector Unreal assets remain under old roots
```

唯一残留是 `Config/DefaultEditorPerProjectUserSettings.ini` 的 Content Browser 选中路径：

```text
ContentBrowserTab1.SelectedPaths=/Game/TopDownBP
```

该字符串不是正式资产引用，而是 Editor UI 偏好；旧验证器因为用简单子串匹配，把 `/Game/TopDownBP` 误判成 `/Game/TopDown` 残留。现已：

1. 将默认 Content Browser 路径改为 `/Game/ProjectArcaneArena`。
2. 将旧路径验证改为带路径边界判断，避免 `/Game/TopDownBP` 这类前缀误报。

完成一次 `RUN_MODE="rewrite"` 或直接同步上述配置后，再运行 `RUN_MODE="validate"`；通过后进入重启 Editor、窄目标编译、PIE 和 Redirector 清理阶段。

### 10.9 Static Validation Passed（2026-09-22）

最终 `RUN_MODE="validate"` 报告：

```text
move=0
conflict=0
review=0
already_target=264
Support-file plan: move=0
Validation: no non-redirector Unreal assets remain under old roots.
Validation: no configured old hard-coded path tokens remain.
Static validation PASSED.
```

这表示**资产迁移、辅助文件迁移、Config/C++/Python 硬编码路径重写和静态残留扫描已经完成**。当前 Canonical Migration Map 中已完成的旧资产迁移项统一记为 `Rewritten`，暂不记为 `Verified`，因为还缺以下运行时验收：

```text
重启 Editor
→ ProjectArcaneArenaEditor 窄目标编译
→ 主地图 / MainMenu / GAS / Wave / Upgrade / Inventory / Boss / P5 路径回归
→ PIE
→ 最后 Fix Up Redirectors
```

只有以上步骤全部通过后，迁移状态才从 `Rewritten` 升为 `Verified`。

### 10.10 序列化引用断链修复（2026-09-22）

重启后的日志确认：迁移目标存在，但 `DA_Waves_Prototype` 仍引用旧路径下的敌人 Blueprint、Affix 和 GE；敌人 Class 加载为空导致 Wave 1 被判定无效。药瓶材质、技能、物品与 Boss Cue 也存在旧依赖。10.9 的静态验收只扫描目录和文本，不能证明 `.uasset` 内部引用有效。

修复状态：`Partial`（配置已实现，独立进程引用验收通过，视觉与 PIE/Cook 待验证）。

- `Config/DefaultEngine.ini` 增加 264 条精确 `PackageRedirects`，依据迁移报告去除 PersistentLevel 重复记录后生成；所有目标文件已确认存在。对 `Lvl_TopDown -> Lvl_Arena` 额外配置对象改名映射。
- 不二进制替换 `.uasset`，不删除迁移后的资产，不修改波次内容、属性值或服务器权威边界。兼容映射让尚未重存的旧引用在加载时解析到新 Package。
- 新增 `Scripts/Python/migration/validate_migrated_references.py`，仅加载不保存。在新启动的 `UnrealEditor-Cmd` Python commandlet（NullRHI）中已通过：264 个目标加载、同名旧路径解析一致、两份 Wave Data 的敌人/精英依赖、瓶体/红蓝液体父材质及瓶体/液体/软木材质贴图节点。日志：`Saved/Logs/MigrationReferenceValidation.log`，`redirects=264 failures=0`，退出码 0。
- NullRHI 不验证 Shader 编译与渲染外观；材质检查直接读取 Texture Expression，避免把无 ShaderMap 时 `GetUsedTextures` 返回空数组误判为丢失贴图。
- 迁移脚本 rewrite/validate 显式保留 `[CoreRedirects]`，防止把 `OldName` 改成新路径或误报旧路径残留。
- 已打开的编辑器必须重启才能使用新的 CoreRedirects；不要在旧会话里保存加载失败后的空引用。后续重存资产和清理 Redirector 必须在引用与 PIE 验证通过后进行。

独立 `-game -NullRHI` 正式地图冒烟已通过：Wave 1 解析为 3 个敌人并开始生成，日志确认自动攻击锁定生成的敌人、双方 GAS 伤害结算正常（`Saved/Logs/MigrationWaveSmoke.log`，退出码 0）。此短测不代表完整通关。

仍需完成：重启编辑器检查材质外观、PIE 后续波次与 Boss/升级/物品回归、双视角与 Listen Server PIE、Cook。改名地图使用新路径加载通过；旧地图对象名的软引用重定向未单独验证。P5 三个原本未创建的 NDC/共享 Niagara 目标不在本次迁移清单中，不将其不存在计为本次修复完成。

### 10.11 移除 CoreRedirects 前的独立验收（2026-09-22）

`validate_migrated_references.py` 的正常通过只证明兼容映射生效，不证明资产已脱离旧路径。新增普通 Python 启动脚本：

```powershell
py -3 Scripts/Python/migration/audit_redirect_independence.py
```

脚本只读扫描全部 `.uasset/.umap`，将含旧 Package 名称的文件列为候选；候选可包含元数据或 Redirector 自身，不直接断言为真实引用。随后生成 `Saved/MigrationReports/redirect_audit_*/DefaultEngine.ini` 临时副本，只移除本次项目迁移的 264 条 Package 与 1 条 Object 映射，通过 `-DEFENGINEINI` 在独立 Unreal Python commandlet 中加载，不修改正式 `Config/DefaultEngine.ini`，不重存资产，不删除 Redirector。其他引擎/插件配置保持继承。

本次结果：`NOT_READY`。扫描 3111 个资产，131 个含旧包名候选；无映射加载时 38 项业务引用断言失败，包括 Wave Data 的敌人/精英依赖和药瓶父材质/软木贴图。正式配置字节核对未变。报告：`Saved/MigrationReports/redirect_audit_02nfcreu/report.json`，摘要见同目录 `README.md`，原始日志见 `Unreal.log`。

结论：暂不能删除 `[CoreRedirects]`。后续需要在映射启用且引用完整的全新 Editor 会话中，针对报告确认的依赖资产通过 UE 重存，将解析后的新引用写入 Package；地图/ExternalActors 与蓝图也必须覆盖。重存后重新运行本检查，再进行 PIE/视觉/Cook 验收。不得在无映射的失败验收进程中保存资产，也不能用二进制字符串替换冒充重存。

`--scan-only` 只生成候选报告，不启动 Unreal；`--editor <UnrealEditor-Cmd.exe 路径>` 可指定现有编辑器程序。`REVIEW_CANDIDATES` 表示加载通过但旧字符串仍需分类，`LOAD_CHECKS_PASSED` 也不替代完整玩法、渲染与打包验收。
