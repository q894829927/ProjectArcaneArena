# 构筑资产 Python 生成器

本目录集中管理 Fire/Lightning/Crit/Shield/Dash/Overload 构筑与 OnKill、OnCrit、OnAbilityCast 等事件触发升级使用的 Upgrade DataAsset、GameplayEffect/GameplayAbility/Area Actor Blueprint、持续/爆发/伤害数字 GameplayCue，以及 Ability/GameMode/原型波次资产连接。

生成器只负责编辑器资产配置，不修改运行时玩法状态。所有脚本都应在 Unreal Editor 已加载项目 C++ 反射类型后执行。

## 目录职责

| 文件 | 职责 | 主要配置 |
| --- | --- | --- |
| `arena_asset_tools.py` | GameplayTag、资产加载、Blueprint 验证、本地化文本和保存等共享工具 | 无 |
| `generate_gameplay_effect_blueprints.py` | 创建原生 GameplayEffect 的 Blueprint 子类 | `EFFECT_BLUEPRINT_CONFIGS` |
| `generate_gameplay_ability_blueprints.py` | 创建原生 GameplayAbility 的 Blueprint 子类 | `ABILITY_BLUEPRINT_CONFIGS` |
| `generate_actor_blueprints.py` | 创建构筑玩法使用的原生 Actor Blueprint 子类 | `ACTOR_BLUEPRINT_CONFIGS` |
| `generate_upgrade_assets.py` | 创建或更新 `ArenaUpgradeDataAsset` | `UPGRADE_CONFIGS` |
| `generate_looping_gameplay_cues.py` | 复制模板并配置持续型 GameplayCue | `LOOPING_CUE_CONFIGS` |
| `generate_burst_gameplay_cues.py` | 复制模板并配置一次性爆发 GameplayCue | `BURST_CUE_CONFIGS` |
| `generate_damage_number_gameplay_cues.py` | 创建普通/暴击伤害数字 GameplayCue | `DAMAGE_NUMBER_CUE_CONFIGS` |
| `configure_build_asset_links.py` | 设置 Ability 的 GE/Actor Class、更新 UpgradePool、连接角色 DamageFeedback 组件，并在远程敌人资产存在时保持四波混合配置 | `ABILITY_BINDINGS`、`ABILITY_CLASS_BINDINGS`、`DAMAGE_FEEDBACK_CHARACTER_PATHS`、`UPGRADE_POOL_ASSET_PATHS` |
| `setup_build_assets.py` | 统一预检并按依赖顺序运行所有分类生成器 | `GENERATOR_MODULE_NAMES` |

`Content/Python/setup_build_assets.py` 是根目录一键入口，实际实现位于本目录。

## 一键执行

在 Unreal Editor 控制台运行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/setup_build_assets.py"
```

总入口会先完成全部预检，再按以下顺序写入：

1. GameplayEffect Blueprint
2. GameplayAbility Blueprint
3. Actor Blueprint
4. Upgrade DataAsset
5. Looping GameplayCue
6. Burst GameplayCue
7. Damage Number GameplayCue
8. Ability、GameMode 和原型波次连接

## 单独执行

需要只更新某一类资产时，可直接运行对应脚本：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_gameplay_effect_blueprints.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_gameplay_ability_blueprints.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_actor_blueprints.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_upgrade_assets.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_looping_gameplay_cues.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_burst_gameplay_cues.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/generate_damage_number_gameplay_cues.py"
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/build_assets/configure_build_asset_links.py"
```

连接脚本依赖其他目标资产已经存在；首次配置建议使用一键总入口。

## 新增升级

在 `generate_upgrade_assets.py` 的 `UPGRADE_CONFIGS` 中复制一项配置，只修改数据字段：

```python
{
    "asset_name": "DA_Upgrade_ExampleDamage",
    "destination_path": "/Game/Data/Upgrade",
    "upgrade_id": "Upgrade.Example.Damage",
    "display_name": "示例增幅",
    "description": "示例技能伤害提高 20%",
    "rarity": "COMMON",
    "upgrade_tags": ["Build.Example", "Upgrade.Example.Damage"],
    "required_tags": [],
    "blocked_tags": [],
    "target_ability_tag": "Ability.Example",
    "trigger_event_tag": None,
    "damage_type_tag": "Damage.Example",
    "numeric_value": 0.20,
    "max_stacks": 3,
    "stackable": True,
    "granted_gameplay_effect_path": None,
    "granted_ability_path": None,
}
```

注意：

- 省略 `icon_path` 会保留现有图标。
- 设置 `icon_path = None` 会清空图标。
- `granted_gameplay_effect_path` 或 `granted_ability_path` 设置为 `None` 会清空对应授予项。
- 与伤害类型无关的事件升级可以把 `damage_type_tag` 设置为 `None`，例如 `Trigger.OnKill` 或 `Trigger.OnAbilityCast` 驱动的能量恢复。
- 通用属性升级可以把 `target_ability_tag` 设置为 `None`，例如通过 GE 增加 `CritChance`。
- Ability 专属但与伤害类型无关的升级可以保留 `target_ability_tag`，同时把 `damage_type_tag` 设置为 `None`，例如 Shield 获得量升级。
- GameplayTag 必须已在项目中注册，否则预检会停止且不写入资产。
- `upgrade_id` 用于服务器选择和堆叠记录，应保持稳定，不要因显示文本变化而修改。

## 新增 Ability、状态 GE 或 Cue

新增状态 GE 时，在 `EFFECT_BLUEPRINT_CONFIGS` 中配置资产名、目录和原生父类 Python 名称。

新增原生 Ability 蓝图子类时，在 `ABILITY_BLUEPRINT_CONFIGS` 中配置相同三项字段。总入口会先生成 Ability，再让 Upgrade DataAsset 和连接步骤引用它。

新增构筑区域 Actor 蓝图子类时，在 `ACTOR_BLUEPRINT_CONFIGS` 中配置资产名、目录和原生 Actor 父类；需要由 Ability 使用时，再加入 `ABILITY_CLASS_BINDINGS`。

新增持续 Cue 时，在 `LOOPING_CUE_CONFIGS` 中配置模板、Cue Tag、Niagara、附着规则和变换覆盖。`rotation` 顺序为 `(Pitch, Yaw, Roll)`，`scale` 顺序为 `(X, Y, Z)`。

新增一次性 Cue 时，在 `BURST_CUE_CONFIGS` 中填写放置参数，并通过 `niagara_paths` 配置一个或多个同点播放的 Niagara；World Location 表现应使用 `DO_NOT_ATTACH` 和 `KEEP_WORLD`。Damage Feedback Foundation 也复用该配置表幂等生成 `GCN_ShieldHit`、`GCN_ShieldBreak`、`GCN_HealthHit` 和 `GCN_ShieldBreakHealthHit`，占位资源使用独立的 Scifi/Basic 结果层风格，避免与 Physical/Fire/Lightning 元素命中 Cue 重复；生成器不会复制或改写 Niagara Graph。

旧普通/暴击伤害数字 Cue 由 `generate_damage_number_gameplay_cues.py` 继续幂等维护，供已有资产兼容；统一 DamageFeedback 主路径不再派发这两个 Tag，而是直接复用 `ArenaDamageNumberActor/Widget`，避免同次伤害生成重复数字。

最后在 `ABILITY_BINDINGS` 中连接 Ability 属性，并把需要进入随机候选池的升级路径加入 `UPGRADE_POOL_ASSET_PATHS`。

## 幂等与错误处理

- 已存在且类型、父类正确的资产会原地更新。
- 已存在但类型或 Blueprint 父类错误时会明确报错，不会覆盖。
- UpgradePool 使用规范化包路径去重，重复运行不会追加同一升级。
- `DA_Waves_Prototype` 的前四波会保持 `3M / 3M+2R / 4M+3R / 5M+4R`，不会因重复执行构筑脚本退回纯近战或追加额外波次。
- Overload 首次生成时会从现有 Shocked 图标复制一个独立的 `T_Upgrade_Overload_Icon` 占位资产；之后替换该纹理不会被脚本覆盖。
- EnergyOnKill 首次生成时会复制独立的 `T_Upgrade_EnergyOnKill_Icon` 占位资产；之后可替换为正式图标且不会被脚本覆盖。
- CritChance 与 EnergyOnCrit 首次生成时分别复制独立占位图标；之后替换纹理不会被脚本覆盖。
- EnergyOnAbilityCast 首次生成时从 EnergyOnCrit 图标复制独立占位图标；之后替换纹理不会被脚本覆盖。
- ShieldAmount 与 ShieldBreakBlast 首次生成时分别复制独立占位图标；`GA_Shield` 会连接新的 SetByCaller `GE_Shield_Grant`，旧 `GE_Shield` 资产不会被删除。
- DashCooldown 与 DashLightningTrail 首次生成时分别复制独立占位图标；Trail Ability 会连接 `GE_Damage` 和生成的单个 `BP_ArenaDashTrailArea`。
- 总入口会先完成所有分类预检，减少执行到中途才发现缺失依赖的情况。
- 修改配置后可连续运行两次，第二次不应创建重复资产或 UpgradePool 条目。
