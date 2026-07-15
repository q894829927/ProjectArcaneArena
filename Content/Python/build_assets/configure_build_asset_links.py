"""把生成的构筑资产连接到 Ability 与 GameMode UpgradePool。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

ABILITY_BINDINGS = [
    {
        "ability_path": "/Game/GAS/GameplayAbility/GA_Fireball",
        "property_name": "burning_effect_class",
        "effect_blueprint_path": "/Game/GAS/GameplayEffect/Status/GE_Status_Burning",
    },
    {
        "ability_path": "/Game/GAS/GameplayAbility/GA_LightningStorm",
        "property_name": "shocked_effect_class",
        "effect_blueprint_path": "/Game/GAS/GameplayEffect/Status/GE_Status_Shocked",
    },
    {
        "ability_path": "/Game/GAS/GameplayAbility/GA_Overload",
        "property_name": "damage_effect_class",
        "effect_blueprint_path": "/Game/GAS/GameplayEffect/GE_Damage",
    },
    {
        "ability_path": "/Game/GAS/GameplayAbility/GA_Overload",
        "property_name": "overload_lockout_effect_class",
        "effect_blueprint_path": "/Game/GAS/GameplayEffect/Status/GE_Status_OverloadLockout",
    },
    {
        "ability_path": "/Game/GAS/GameplayAbility/GA_EnergyOnKill",
        "property_name": "energy_restore_effect_class",
        "effect_blueprint_path": "/Game/GAS/GameplayEffect/Trigger/GE_Trigger_EnergyOnKill",
    },
]

GAME_MODE_PATH = "/Game/GameMode/BP_ArenaGameMode"
PROTOTYPE_WAVE_DATA_PATH = "/Game/Blueprints/DataAsset/DA_Waves_Prototype"
PROTOTYPE_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter"
PROTOTYPE_RANGED_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaRangedEnemy"
UPGRADE_POOL_ASSET_PATHS = [
    "/Game/Data/Upgrade/DA_Upgrade_FireballDamage",
    "/Game/Data/Upgrade/DA_Upgrade_FireballBurning",
    "/Game/Data/Upgrade/DA_Upgrade_LightningStormDamage",
    "/Game/Data/Upgrade/DA_Upgrade_LightningStormShocked",
    "/Game/Data/Upgrade/DA_Upgrade_Overload",
    "/Game/Data/Upgrade/DA_Upgrade_EnergyOnKill",
]


def _require_or_allow_generated(asset_path, generated_asset_paths):
    """允许总入口预检引用稍后生成的资产，其他缺失路径仍立即报错。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return True
    if asset_path in generated_asset_paths:
        return False
    raise RuntimeError(f"Required linked asset was not found: {asset_path}")


def _validate_ability_binding(binding, index, generated_asset_paths):
    """验证 Ability 属性以及状态 GE Class 引用。"""
    required_keys = ("ability_path", "property_name", "effect_blueprint_path")
    missing_keys = [key for key in required_keys if key not in binding]
    if missing_keys:
        raise RuntimeError(
            f"ABILITY_BINDINGS[{index}] is missing keys: {', '.join(missing_keys)}"
        )

    if _require_or_allow_generated(binding["ability_path"], generated_asset_paths):
        _, ability_class = tools.require_blueprint(
            binding["ability_path"],
            unreal.GameplayAbility,
        )
        ability_defaults = unreal.get_default_object(ability_class)
        try:
            ability_defaults.get_editor_property(binding["property_name"])
        except Exception as error:
            raise RuntimeError(
                f"Ability {binding['ability_path']} does not expose "
                f"'{binding['property_name']}': {error}"
            )

    if _require_or_allow_generated(
        binding["effect_blueprint_path"],
        generated_asset_paths,
    ):
        tools.require_blueprint(
            binding["effect_blueprint_path"],
            unreal.GameplayEffect,
        )


def validate_configs(generated_asset_paths=None):
    """验证 Ability、升级池以及近战/远程原型波次所需引用。"""
    generated_asset_paths = set(generated_asset_paths or ())
    seen_binding_properties = set()
    for index, binding in enumerate(ABILITY_BINDINGS):
        binding_key = (binding.get("ability_path"), binding.get("property_name"))
        if binding_key in seen_binding_properties:
            raise RuntimeError(f"Duplicate Ability binding: {binding_key}")
        seen_binding_properties.add(binding_key)
        _validate_ability_binding(binding, index, generated_asset_paths)

    upgrade_data_class = tools.require_unreal_type("ArenaUpgradeDataAsset")
    seen_upgrade_paths = set()
    for asset_path in UPGRADE_POOL_ASSET_PATHS:
        if asset_path in seen_upgrade_paths:
            raise RuntimeError(f"Duplicate UpgradePool asset path: {asset_path}")
        seen_upgrade_paths.add(asset_path)
        if _require_or_allow_generated(asset_path, generated_asset_paths):
            tools.require_asset(asset_path, upgrade_data_class)

    _, game_mode_class = tools.require_blueprint(GAME_MODE_PATH)
    game_mode_defaults = unreal.get_default_object(game_mode_class)
    try:
        game_mode_defaults.get_editor_property("upgrade_pool")
    except Exception as error:
        raise RuntimeError(
            f"GameMode {GAME_MODE_PATH} does not expose 'upgrade_pool': {error}"
        )

    # 混合波依赖近战原型与可选远程原型；远程尚未生成时不阻断其他构筑资产连接。
    wave_data_class = tools.require_unreal_type("ArenaWaveDataAsset")
    tools.require_unreal_type("ArenaWaveConfig")
    tools.require_unreal_type("ArenaWaveEnemyEntry")
    wave_data = tools.require_asset(PROTOTYPE_WAVE_DATA_PATH, wave_data_class)
    tools.require_blueprint(
        PROTOTYPE_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    if unreal.EditorAssetLibrary.does_asset_exist(PROTOTYPE_RANGED_ENEMY_PATH):
        tools.require_blueprint(
            PROTOTYPE_RANGED_ENEMY_PATH,
            tools.require_unreal_type("ArenaEnemyCharacter"),
        )
    else:
        unreal.log_warning(
            "Ranged enemy asset is not generated yet; build asset setup will "
            "leave the current wave enemy arrays unchanged."
        )
    try:
        wave_data.get_editor_property("waves")
    except Exception as error:
        raise RuntimeError(
            f"Wave data {PROTOTYPE_WAVE_DATA_PATH} does not expose 'waves': {error}"
        )


def _configure_ability_binding(binding):
    """把状态 GE Blueprint Class 写入 Ability 类默认对象。"""
    ability_blueprint, ability_class = tools.require_blueprint(
        binding["ability_path"],
        unreal.GameplayAbility,
    )
    _, effect_class = tools.require_blueprint(
        binding["effect_blueprint_path"],
        unreal.GameplayEffect,
    )
    ability_defaults = unreal.get_default_object(ability_class)
    ability_defaults.modify()
    ability_defaults.set_editor_property(binding["property_name"], effect_class)
    tools.save_asset(binding["ability_path"])
    unreal.log(
        f"Configured Ability binding: {binding['ability_path']}."
        f"{binding['property_name']}"
    )
    return ability_blueprint


def _configure_upgrade_pool():
    """按资产路径去重并向 GameMode UpgradePool 追加构筑升级。"""
    _, game_mode_class = tools.require_blueprint(GAME_MODE_PATH)
    game_mode_defaults = unreal.get_default_object(game_mode_class)
    game_mode_defaults.modify()
    upgrade_pool = list(game_mode_defaults.get_editor_property("upgrade_pool"))
    existing_paths = {
        tools.canonical_asset_path(upgrade)
        for upgrade in upgrade_pool
        if upgrade is not None
    }

    upgrade_data_class = tools.require_unreal_type("ArenaUpgradeDataAsset")
    for asset_path in UPGRADE_POOL_ASSET_PATHS:
        canonical_path = tools.canonical_asset_path(asset_path)
        if canonical_path in existing_paths:
            continue
        upgrade_pool.append(tools.require_asset(asset_path, upgrade_data_class))
        existing_paths.add(canonical_path)

    game_mode_defaults.set_editor_property("upgrade_pool", upgrade_pool)
    tools.save_asset(GAME_MODE_PATH)
    unreal.log(f"Configured GameMode UpgradePool: {GAME_MODE_PATH}")


def _make_wave_enemy_entry(enemy_class, count):
    """构造一条确定顺序的敌人类型与数量配置。"""
    enemy_entry = tools.require_unreal_type("ArenaWaveEnemyEntry")()
    enemy_entry.set_editor_property("enemy_class", enemy_class)
    enemy_entry.set_editor_property("count", count)
    return enemy_entry


def _configure_prototype_enemy_mixes():
    """只替换前四波 Enemies，避免后续构筑脚本把混合波覆盖回纯近战。"""
    if not unreal.EditorAssetLibrary.does_asset_exist(PROTOTYPE_RANGED_ENEMY_PATH):
        unreal.log_warning(
            f"Skipped mixed wave configuration because {PROTOTYPE_RANGED_ENEMY_PATH} is missing."
        )
        return

    wave_data = tools.require_asset(
        PROTOTYPE_WAVE_DATA_PATH,
        tools.require_unreal_type("ArenaWaveDataAsset"),
    )
    _, melee_enemy_class = tools.require_blueprint(
        PROTOTYPE_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    _, ranged_enemy_class = tools.require_blueprint(
        PROTOTYPE_RANGED_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )

    wave_mixes = (
        ((melee_enemy_class, 3),),
        ((melee_enemy_class, 3), (ranged_enemy_class, 2)),
        ((melee_enemy_class, 4), (ranged_enemy_class, 3)),
        ((melee_enemy_class, 5), (ranged_enemy_class, 4)),
    )

    waves = list(wave_data.get_editor_property("waves"))
    if len(waves) < 3:
        raise RuntimeError(
            f"{PROTOTYPE_WAVE_DATA_PATH} must keep its first three prototype waves."
        )
    while len(waves) < len(wave_mixes):
        waves.append(tools.require_unreal_type("ArenaWaveConfig")())

    for wave_index, wave_mix in enumerate(wave_mixes):
        entries = [
            _make_wave_enemy_entry(enemy_class, count)
            for enemy_class, count in wave_mix
        ]
        waves[wave_index].set_editor_property("enemies", entries)

    wave_data.modify()
    wave_data.set_editor_property("waves", waves)
    tools.save_asset(PROTOTYPE_WAVE_DATA_PATH)
    unreal.log(f"Configured prototype melee/ranged wave mixes: {PROTOTYPE_WAVE_DATA_PATH}")


def run():
    """验证后连接 Ability/UpgradePool，并保持前四波混合敌人配置。"""
    validate_configs()
    for binding in ABILITY_BINDINGS:
        _configure_ability_binding(binding)
    _configure_upgrade_pool()
    _configure_prototype_enemy_mixes()


def main():
    """提供可从 Unreal Editor 单独执行的构筑资产连接入口。"""
    run()
    unreal.log("Build asset link configuration completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Build asset link configuration failed: {error}")
        raise
