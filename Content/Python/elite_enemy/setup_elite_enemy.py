"""幂等创建精英词缀资产并配置原型波次曲线。

新增反射 C++ 类型编译并重启 Unreal Editor 后执行；脚本不会加载、保存、创建或切换地图。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

WAVE_DATA_PATH = "/Game/Blueprints/DataAsset/DA_Waves_Prototype"
MELEE_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter"
RANGED_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaRangedEnemy"
DAMAGE_EFFECT_PATH = "/Game/GAS/GameplayEffect/GE_Damage"
SHIELD_EFFECT_PATH = "/Game/GAS/GameplayEffect/GE_Shield_Grant"

EFFECT_FOLDER = "/Game/GAS/GameplayEffect"
BASELINE_EFFECT_PATH = f"{EFFECT_FOLDER}/GE_Elite_BaseAttributes"
FRENZY_EFFECT_PATH = f"{EFFECT_FOLDER}/GE_Elite_Frenzy"
AFFIX_FOLDER = "/Game/Data/EnemyAffix"
CUE_FOLDER = "/Game/GAS/GameplayCues/Elite"

LOOPING_CUE_TEMPLATE = "/Game/GAS/GameplayCues/DurationCue/GCN_Shield_Active"
BURST_CUE_TEMPLATE = "/Game/GAS/GameplayCues/InstaneCue/GCN_Hit_Physical"

AFFIX_CONFIGS = {
    "Volatile": {
        "asset_name": "DA_EnemyAffix_Volatile",
        "display_name": "易爆",
        "description": "死亡时短暂预警，随后对视线内的玩家造成范围伤害。",
        "color": (1.0, 0.16, 0.05, 1.0),
        "behavior": "VOLATILE",
        "affix_tag": "Enemy.Affix.Volatile",
        "active_cue": "GameplayCue.Enemy.Affix.Volatile.Active",
        "trigger_cue": "GameplayCue.Enemy.Affix.Volatile.Explode",
    },
    "ArcaneWarden": {
        "asset_name": "DA_EnemyAffix_ArcaneWarden",
        "display_name": "护阵",
        "description": "周期性为附近的其他同波次敌人补充护盾。",
        "color": (0.12, 0.55, 1.0, 1.0),
        "behavior": "ARCANE_WARDEN",
        "affix_tag": "Enemy.Affix.ArcaneWarden",
        "active_cue": "GameplayCue.Enemy.Affix.ArcaneWarden.Active",
        "trigger_cue": "GameplayCue.Enemy.Affix.ArcaneWarden.Pulse",
    },
    "Frenzy": {
        "asset_name": "DA_EnemyAffix_Frenzy",
        "display_name": "狂暴",
        "description": "首次降至低生命时永久提升攻击力与移速。",
        "color": (0.95, 0.05, 0.35, 1.0),
        "behavior": "FRENZY",
        "affix_tag": "Enemy.Affix.Frenzy",
        "active_cue": "GameplayCue.Enemy.Affix.Frenzy.Active",
        "trigger_cue": "GameplayCue.Enemy.Affix.Frenzy.Trigger",
    },
}

CUE_CONFIGS = (
    ("GCN_Elite_Volatile_Active", "GameplayCue.Enemy.Affix.Volatile.Active", LOOPING_CUE_TEMPLATE, "GameplayCueNotify_Looping"),
    ("GCN_Elite_Volatile_Telegraph", "GameplayCue.Enemy.Affix.Volatile.Telegraph", LOOPING_CUE_TEMPLATE, "GameplayCueNotify_Looping"),
    ("GCN_Elite_Volatile_Explode", "GameplayCue.Enemy.Affix.Volatile.Explode", BURST_CUE_TEMPLATE, "GameplayCueNotify_Burst"),
    ("GCN_Elite_ArcaneWarden_Active", "GameplayCue.Enemy.Affix.ArcaneWarden.Active", LOOPING_CUE_TEMPLATE, "GameplayCueNotify_Looping"),
    ("GCN_Elite_ArcaneWarden_Pulse", "GameplayCue.Enemy.Affix.ArcaneWarden.Pulse", BURST_CUE_TEMPLATE, "GameplayCueNotify_Burst"),
    ("GCN_Elite_Frenzy_Active", "GameplayCue.Enemy.Affix.Frenzy.Active", LOOPING_CUE_TEMPLATE, "GameplayCueNotify_Looping"),
    ("GCN_Elite_Frenzy_Trigger", "GameplayCue.Enemy.Affix.Frenzy.Trigger", BURST_CUE_TEMPLATE, "GameplayCueNotify_Burst"),
)

FULL_POOL = ("Volatile", "ArcaneWarden", "Frenzy")
# 旧正式资产的 W6-W8 只有近战；首次迁移按约 40% 远程占比拆分，并保持每波总数不变。
LEGACY_LATE_WAVE_RANGED_COUNTS = {
    5: 5,
    6: 6,
    7: 7,
}
WAVE_ELITE_ASSIGNMENTS = {
    2: {MELEE_ENEMY_PATH: (1, ("Volatile",))},
    3: {RANGED_ENEMY_PATH: (1, ("ArcaneWarden",))},
    4: {MELEE_ENEMY_PATH: (1, ("Frenzy",))},
    5: {
        MELEE_ENEMY_PATH: (1, FULL_POOL),
        RANGED_ENEMY_PATH: (1, FULL_POOL),
    },
    6: {
        MELEE_ENEMY_PATH: (1, FULL_POOL),
        RANGED_ENEMY_PATH: (1, FULL_POOL),
    },
    7: {
        MELEE_ENEMY_PATH: (2, FULL_POOL),
        RANGED_ENEMY_PATH: (1, FULL_POOL),
    },
}


def _class_asset_path(enemy_class):
    """把 BlueprintGeneratedClass 引用规范化为 Blueprint 包路径。"""
    if enemy_class is None:
        return ""
    object_path = enemy_class.get_path_name()
    return object_path.split(".", 1)[0]


def _find_entry(entries, enemy_path):
    """按敌人 Blueprint 路径查找唯一的波次 Entry。"""
    matches = [
        entry
        for entry in entries
        if _class_asset_path(entry.get_editor_property("enemy_class")) == enemy_path
    ]
    if len(matches) != 1:
        actual_entries = ", ".join(
            f"{_class_asset_path(entry.get_editor_property('enemy_class')) or '<None>'}"
            f" x{int(entry.get_editor_property('count'))}"
            for entry in entries
        ) or "<empty>"
        raise RuntimeError(
            f"Expected exactly one {enemy_path} entry, found {len(matches)}; "
            f"actual entries: [{actual_entries}]."
        )
    return matches[0]


def _preflight():
    """在任何写入前校验反射类型、源资产、目标冲突与九波结构。"""
    for type_name in (
        "ArenaEnemyAffixDataAsset",
        "ArenaEnemyAffixBehavior",
        "ArenaEliteBaselineConfig",
        "ArenaVolatileAffixConfig",
        "ArenaArcaneWardenAffixConfig",
        "ArenaFrenzyAffixConfig",
        "ArenaGameplayEffect_EliteBaseline",
        "ArenaGameplayEffect_EliteFrenzy",
        "ArenaWaveDataAsset",
        "ArenaWaveEnemyEntry",
    ):
        tools.require_unreal_type(type_name)

    tools.require_blueprint(
        MELEE_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    tools.require_blueprint(
        RANGED_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)
    tools.require_blueprint(SHIELD_EFFECT_PATH, unreal.GameplayEffect)
    tools.require_blueprint(LOOPING_CUE_TEMPLATE, unreal.GameplayCueNotify_Looping)
    tools.require_blueprint(BURST_CUE_TEMPLATE, unreal.GameplayCueNotify_Burst)

    for config in AFFIX_CONFIGS.values():
        tools.make_tag(config["affix_tag"])
        tools.make_tag(config["active_cue"])
        tools.make_tag(config["trigger_cue"])
    for _, cue_tag, _, _ in CUE_CONFIGS:
        tools.make_tag(cue_tag)

    generated_blueprints = (
        (
            BASELINE_EFFECT_PATH,
            tools.require_unreal_type("ArenaGameplayEffect_EliteBaseline"),
        ),
        (
            FRENZY_EFFECT_PATH,
            tools.require_unreal_type("ArenaGameplayEffect_EliteFrenzy"),
        ),
    )
    for asset_path, parent_class in generated_blueprints:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)
    for asset_name, _, _, parent_type_name in CUE_CONFIGS:
        asset_path = f"{CUE_FOLDER}/{asset_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(
                asset_path,
                tools.require_unreal_type(parent_type_name),
            )
    affix_class = tools.require_unreal_type("ArenaEnemyAffixDataAsset")
    for config in AFFIX_CONFIGS.values():
        asset_path = f"{AFFIX_FOLDER}/{config['asset_name']}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_asset(asset_path, affix_class)

    wave_data = tools.require_asset(
        WAVE_DATA_PATH,
        tools.require_unreal_type("ArenaWaveDataAsset"),
    )
    waves = list(wave_data.get_editor_property("waves"))
    if len(waves) != 9:
        raise RuntimeError(f"{WAVE_DATA_PATH} must contain exactly nine waves.")

    boss_indices = [
        index
        for index, wave in enumerate(waves)
        if bool(wave.get_editor_property("boss_wave"))
    ]
    if boss_indices != [8]:
        raise RuntimeError(
            f"Expected only the final wave to be a boss wave; found {boss_indices}."
        )

    wave_curve_errors = []
    for wave_index, assignments in WAVE_ELITE_ASSIGNMENTS.items():
        entries = list(waves[wave_index].get_editor_property("enemies"))
        for enemy_path, (elite_count, _) in assignments.items():
            matching_entries = [
                entry
                for entry in entries
                if _class_asset_path(entry.get_editor_property("enemy_class"))
                == enemy_path
            ]
            if (
                len(matching_entries) == 0
                and enemy_path == RANGED_ENEMY_PATH
                and wave_index in LEGACY_LATE_WAVE_RANGED_COUNTS
            ):
                melee_entries = [
                    entry
                    for entry in entries
                    if _class_asset_path(entry.get_editor_property("enemy_class"))
                    == MELEE_ENEMY_PATH
                ]
                ranged_count = LEGACY_LATE_WAVE_RANGED_COUNTS[wave_index]
                if len(melee_entries) == 1:
                    melee_count = int(melee_entries[0].get_editor_property("count"))
                    melee_elite_count = assignments.get(
                        MELEE_ENEMY_PATH,
                        (0, ()),
                    )[0]
                    if (
                        ranged_count >= elite_count
                        and melee_count - ranged_count >= melee_elite_count
                    ):
                        continue
                wave_curve_errors.append(
                    f"Wave {wave_index + 1}: cannot split the legacy melee entry "
                    f"into a {ranged_count}-count ranged entry while preserving "
                    "the approved elite slots"
                )
                continue
            if len(matching_entries) != 1:
                actual_entries = ", ".join(
                    f"{_class_asset_path(entry.get_editor_property('enemy_class')) or '<None>'}"
                    f" x{int(entry.get_editor_property('count'))}"
                    for entry in entries
                ) or "<empty>"
                wave_curve_errors.append(
                    f"Wave {wave_index + 1}: expected exactly one {enemy_path} "
                    f"entry, found {len(matching_entries)}; actual entries: "
                    f"[{actual_entries}]"
                )
                continue
            entry = matching_entries[0]
            count = int(entry.get_editor_property("count"))
            if elite_count > count:
                wave_curve_errors.append(
                    f"Wave {wave_index + 1} requests {elite_count} elites from "
                    f"{enemy_path}, but Count is only {count}"
                )
    if wave_curve_errors:
        raise RuntimeError(
            "WaveData does not satisfy the approved elite curve:\n- "
            + "\n- ".join(wave_curve_errors)
        )
    return wave_data


def _create_effect_blueprints():
    """幂等创建或复用两个原生 GameplayEffect 的 Blueprint 子类。"""
    _, baseline_class, baseline_path = tools.create_or_load_blueprint(
        "GE_Elite_BaseAttributes",
        EFFECT_FOLDER,
        tools.require_unreal_type("ArenaGameplayEffect_EliteBaseline"),
    )
    _, frenzy_class, frenzy_path = tools.create_or_load_blueprint(
        "GE_Elite_Frenzy",
        EFFECT_FOLDER,
        tools.require_unreal_type("ArenaGameplayEffect_EliteFrenzy"),
    )
    tools.save_asset(baseline_path)
    tools.save_asset(frenzy_path)
    return baseline_class, frenzy_class


def _ensure_destination_folders():
    """预检通过后一次性确保新增资产目录存在。"""
    for folder_path in (AFFIX_FOLDER, CUE_FOLDER):
        if unreal.EditorAssetLibrary.does_directory_exist(folder_path):
            continue
        if not unreal.EditorAssetLibrary.make_directory(folder_path):
            raise RuntimeError(f"Failed to create destination folder: {folder_path}")


def _create_gameplay_cues():
    """复用稳定的项目 Cue 模板并写入精英专属 Tag。"""
    for asset_name, cue_tag, template_path, parent_type_name in CUE_CONFIGS:
        parent_class = tools.require_unreal_type(parent_type_name)
        _, generated_class, asset_path = tools.duplicate_or_load_blueprint(
            asset_name,
            CUE_FOLDER,
            template_path,
            parent_class,
        )
        defaults = unreal.get_default_object(generated_class)
        defaults.modify()
        defaults.set_editor_property("gameplay_cue_tag", tools.make_tag(cue_tag))
        tools.save_asset(asset_path)


def _create_or_load_affix(config):
    """幂等创建或复用一个强类型敌人词缀 DataAsset。"""
    affix_class = tools.require_unreal_type("ArenaEnemyAffixDataAsset")
    asset_path = f"{AFFIX_FOLDER}/{config['asset_name']}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return tools.require_asset(asset_path, affix_class), asset_path

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", affix_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        config["asset_name"],
        AFFIX_FOLDER,
        affix_class,
        factory,
    )
    if asset is None:
        raise RuntimeError(f"Failed to create enemy affix DataAsset: {asset_path}")
    return asset, asset_path


def _configure_affixes(frenzy_effect_class):
    """写入本地化身份、Tag、Cue 路由和行为专属数值。"""
    _, damage_effect_class = tools.require_blueprint(
        DAMAGE_EFFECT_PATH,
        unreal.GameplayEffect,
    )
    _, shield_effect_class = tools.require_blueprint(
        SHIELD_EFFECT_PATH,
        unreal.GameplayEffect,
    )
    behavior_enum = tools.require_unreal_type("ArenaEnemyAffixBehavior")
    affix_assets = {}

    for affix_key, config in AFFIX_CONFIGS.items():
        asset, asset_path = _create_or_load_affix(config)
        asset.modify()
        asset.set_editor_property("affix_id", unreal.Name(affix_key))
        tools.set_localized_text_property(asset, "DisplayName", config["display_name"])
        tools.set_localized_text_property(asset, "Description", config["description"])
        asset.set_editor_property("accent_color", unreal.LinearColor(*config["color"]))
        asset.set_editor_property(
            "behavior",
            tools.resolve_enum_value(behavior_enum, config["behavior"]),
        )
        asset.set_editor_property("affix_tag", tools.make_tag(config["affix_tag"]))
        asset.set_editor_property(
            "active_gameplay_cue_tag",
            tools.make_tag(config["active_cue"]),
        )
        asset.set_editor_property(
            "trigger_gameplay_cue_tag",
            tools.make_tag(config["trigger_cue"]),
        )

        if affix_key == "Volatile":
            volatile = tools.require_unreal_type("ArenaVolatileAffixConfig")()
            volatile.set_editor_property("telegraph_duration", 0.8)
            volatile.set_editor_property("explosion_radius", 325.0)
            volatile.set_editor_property("base_damage", 18.0)
            volatile.set_editor_property("damage_effect_class", damage_effect_class)
            volatile.set_editor_property(
                "telegraph_gameplay_cue_tag",
                tools.make_tag("GameplayCue.Enemy.Affix.Volatile.Telegraph"),
            )
            asset.set_editor_property("volatile", volatile)
        elif affix_key == "ArcaneWarden":
            warden = tools.require_unreal_type("ArenaArcaneWardenAffixConfig")()
            warden.set_editor_property("initial_delay", 2.0)
            warden.set_editor_property("pulse_interval", 5.0)
            warden.set_editor_property("radius", 550.0)
            warden.set_editor_property("shield_cap", 30.0)
            warden.set_editor_property("shield_effect_class", shield_effect_class)
            asset.set_editor_property("arcane_warden", warden)
        else:
            frenzy = tools.require_unreal_type("ArenaFrenzyAffixConfig")()
            frenzy.set_editor_property("health_threshold", 0.4)
            frenzy.set_editor_property("frenzy_effect_class", frenzy_effect_class)
            asset.set_editor_property("frenzy", frenzy)

        tools.save_asset(asset_path)
        affix_assets[affix_key] = asset
    return affix_assets


def _migrate_legacy_late_wave_mix(wave_index, entries, ranged_enemy_class):
    """把旧 W6-W8 的纯近战 Count 拆成近战/远程，且保持总敌人数与重复执行稳定。"""
    if wave_index not in LEGACY_LATE_WAVE_RANGED_COUNTS:
        return entries

    ranged_entries = [
        entry
        for entry in entries
        if _class_asset_path(entry.get_editor_property("enemy_class"))
        == RANGED_ENEMY_PATH
    ]
    if len(ranged_entries) == 1:
        return entries
    if ranged_entries:
        raise RuntimeError(
            f"Wave {wave_index + 1} contains duplicate ranged enemy entries."
        )

    melee_entry = _find_entry(entries, MELEE_ENEMY_PATH)
    original_melee_count = int(melee_entry.get_editor_property("count"))
    ranged_count = LEGACY_LATE_WAVE_RANGED_COUNTS[wave_index]
    remaining_melee_count = original_melee_count - ranged_count
    if remaining_melee_count <= 0:
        raise RuntimeError(
            f"Wave {wave_index + 1} cannot preserve a positive melee Count after "
            f"splitting {ranged_count} ranged enemies from {original_melee_count}."
        )

    melee_entry.set_editor_property("count", remaining_melee_count)
    ranged_entry = tools.require_unreal_type("ArenaWaveEnemyEntry")()
    ranged_entry.set_editor_property("enemy_class", ranged_enemy_class)
    ranged_entry.set_editor_property("count", ranged_count)
    ranged_entry.set_editor_property("elite_count", 0)
    ranged_entry.set_editor_property("elite_affix_pool", [])
    entries.append(ranged_entry)
    unreal.log(
        f"Migrated Wave {wave_index + 1} enemy mix: "
        f"{remaining_melee_count} melee + {ranged_count} ranged "
        f"(total {original_melee_count})."
    )
    return entries


def _configure_waves(wave_data, baseline_effect_class, affix_assets):
    """迁移获批的后期混合波，再写精英字段与全局基线并保留总数和其他调优。"""
    baseline = tools.require_unreal_type("ArenaEliteBaselineConfig")()
    baseline.set_editor_property("max_health_multiplier", 2.0)
    baseline.set_editor_property("attack_power_multiplier", 1.2)
    baseline.set_editor_property("defense_bonus", 5.0)

    _, ranged_enemy_class = tools.require_blueprint(
        RANGED_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    waves = list(wave_data.get_editor_property("waves"))
    for wave_index, wave in enumerate(waves):
        entries = list(wave.get_editor_property("enemies"))
        entries = _migrate_legacy_late_wave_mix(
            wave_index,
            entries,
            ranged_enemy_class,
        )
        for entry in entries:
            entry.set_editor_property("elite_count", 0)
            entry.set_editor_property("elite_affix_pool", [])

        for enemy_path, (elite_count, pool_keys) in WAVE_ELITE_ASSIGNMENTS.get(
            wave_index,
            {},
        ).items():
            entry = _find_entry(entries, enemy_path)
            entry.set_editor_property("elite_count", elite_count)
            entry.set_editor_property(
                "elite_affix_pool",
                [affix_assets[key] for key in pool_keys],
            )

        wave.set_editor_property("enemies", entries)
        waves[wave_index] = wave

    wave_data.modify()
    wave_data.set_editor_property("elite_baseline", baseline)
    wave_data.set_editor_property(
        "elite_baseline_effect_class",
        baseline_effect_class,
    )
    wave_data.set_editor_property("waves", waves)
    tools.save_asset(WAVE_DATA_PATH)


def main():
    """执行完整预检、资产生成并写入确定的 W1-W9 精英曲线。"""
    wave_data = _preflight()
    _ensure_destination_folders()
    with unreal.ScopedSlowTask(4, "Setting up elite enemies and affixes") as task:
        task.make_dialog(True)

        baseline_effect_class, frenzy_effect_class = _create_effect_blueprints()
        task.enter_progress_frame(1, "Created elite GameplayEffects")

        _create_gameplay_cues()
        task.enter_progress_frame(1, "Created elite GameplayCues")

        affix_assets = _configure_affixes(frenzy_effect_class)
        task.enter_progress_frame(1, "Created enemy affix DataAssets")

        _configure_waves(wave_data, baseline_effect_class, affix_assets)
        task.enter_progress_frame(1, "Configured the nine-wave elite curve")

    unreal.log("Elite enemy setup completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Elite enemy setup failed: {error}")
        raise
