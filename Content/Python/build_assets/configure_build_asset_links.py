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
]

GAME_MODE_PATH = "/Game/GameMode/BP_ArenaGameMode"
UPGRADE_POOL_ASSET_PATHS = [
    "/Game/Data/Upgrade/DA_Upgrade_FireballDamage",
    "/Game/Data/Upgrade/DA_Upgrade_FireballBurning",
    "/Game/Data/Upgrade/DA_Upgrade_LightningStormDamage",
    "/Game/Data/Upgrade/DA_Upgrade_LightningStormShocked",
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
    """验证 Ability 连接、GameMode 属性和升级池引用。"""
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


def run():
    """验证后连接 Ability 状态效果并更新 GameMode 升级池。"""
    validate_configs()
    for binding in ABILITY_BINDINGS:
        _configure_ability_binding(binding)
    _configure_upgrade_pool()


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
