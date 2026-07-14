"""根据顶部配置创建或更新构筑升级 DataAsset。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

# 省略 icon_path 会保留现有图标；显式 None 会清空图标或授予项。
UPGRADE_CONFIGS = [
    {
        "asset_name": "DA_Upgrade_FireballDamage",
        "destination_path": "/Game/Data/Upgrade",
        "upgrade_id": "Upgrade.Fireball.Damage",
        "display_name": "火球增幅",
        "description": "火球术伤害提高 20%，最多叠加 3 层",
        "rarity": "COMMON",
        "upgrade_tags": ["Build.Fire", "Upgrade.Fireball.Damage"],
        "required_tags": [],
        "blocked_tags": [],
        "target_ability_tag": "Ability.Fireball",
        "trigger_event_tag": None,
        "damage_type_tag": "Damage.Fire",
        "numeric_value": 0.20,
        "max_stacks": 3,
        "stackable": True,
        "granted_gameplay_effect_path": None,
        "granted_ability_path": None,
    },
    {
        "asset_name": "DA_Upgrade_FireballBurning",
        "destination_path": "/Game/Data/Upgrade",
        "upgrade_id": "Upgrade.Fireball.Burning",
        "display_name": "点燃",
        "description": "火球术命中时施加燃烧，燃烧最多叠加 3 层",
        "rarity": "RARE",
        "upgrade_tags": ["Build.Fire", "Upgrade.Fireball.Burning"],
        "required_tags": ["Build.Fire"],
        "blocked_tags": [],
        "target_ability_tag": "Ability.Fireball",
        "trigger_event_tag": None,
        "damage_type_tag": "Damage.Fire",
        "numeric_value": 5.0,
        "max_stacks": 1,
        "stackable": False,
        "granted_gameplay_effect_path": None,
        "granted_ability_path": None,
    },
    {
        "asset_name": "DA_Upgrade_LightningStormDamage",
        "destination_path": "/Game/Data/Upgrade",
        "upgrade_id": "Upgrade.LightningStorm.Damage",
        "display_name": "雷暴增幅",
        "description": "闪电风暴伤害提高 20%，最多叠加 3 层",
        "rarity": "COMMON",
        "upgrade_tags": ["Build.Lightning", "Upgrade.LightningStorm.Damage"],
        "required_tags": [],
        "blocked_tags": [],
        "target_ability_tag": "Ability.LightningStorm",
        "trigger_event_tag": None,
        "damage_type_tag": "Damage.Lightning",
        "numeric_value": 0.20,
        "max_stacks": 3,
        "stackable": True,
        "granted_gameplay_effect_path": None,
        "granted_ability_path": None,
    },
    {
        "asset_name": "DA_Upgrade_LightningStormShocked",
        "destination_path": "/Game/Data/Upgrade",
        "upgrade_id": "Upgrade.LightningStorm.Shocked",
        "display_name": "感电",
        "description": "闪电风暴命中时施加 4 秒感电，感电目标受到的闪电伤害提高 20%",
        "rarity": "RARE",
        "upgrade_tags": ["Build.Lightning", "Upgrade.LightningStorm.Shocked"],
        "required_tags": ["Build.Lightning"],
        "blocked_tags": [],
        "target_ability_tag": "Ability.LightningStorm",
        "trigger_event_tag": None,
        "damage_type_tag": "Damage.Lightning",
        "numeric_value": 0.20,
        "max_stacks": 1,
        "stackable": False,
        "granted_gameplay_effect_path": None,
        "granted_ability_path": None,
    },
    {
        "asset_name": "DA_Upgrade_Overload",
        "destination_path": "/Game/Data/Upgrade",
        "upgrade_id": "Upgrade.Combo.Overload",
        "display_name": "元素过载",
        "description": "闪电伤害命中燃烧目标时引发范围爆炸，同一来源对同一目标每秒最多触发一次",
        "rarity": "LEGENDARY",
        "upgrade_tags": [
            "Build.Fire",
            "Build.Lightning",
            "Upgrade.Combo.Overload",
        ],
        "required_tags": ["Build.Fire", "Build.Lightning"],
        "blocked_tags": [],
        "target_ability_tag": "Ability.Passive.Overload",
        "trigger_event_tag": "Trigger.OnDamageDealt.Lightning",
        "damage_type_tag": "Damage.Lightning",
        "numeric_value": 20.0,
        "max_stacks": 1,
        "stackable": False,
        "icon_path": "/Game/UI/UpgradeIcons/T_Upgrade_Overload_Icon",
        "icon_template_path": "/Game/UI/UpgradeIcons/T_Upgrade_LightningStormShocked_Icon",
        "granted_gameplay_effect_path": None,
        "granted_ability_path": "/Game/GAS/GameplayAbility/GA_Overload",
    },
    {
        "asset_name": "DA_Upgrade_EnergyOnKill",
        "destination_path": "/Game/Data/Upgrade",
        "upgrade_id": "Upgrade.Trigger.EnergyOnKill",
        "display_name": "能量收割",
        "description": "击杀敌人时恢复 10 点能量，最多叠加 3 层",
        "rarity": "COMMON",
        "upgrade_tags": ["Upgrade.Trigger.EnergyOnKill"],
        "required_tags": [],
        "blocked_tags": [],
        "target_ability_tag": "Ability.Passive.EnergyOnKill",
        "trigger_event_tag": "Trigger.OnKill",
        "damage_type_tag": None,
        "numeric_value": 10.0,
        "max_stacks": 3,
        "stackable": True,
        "icon_path": "/Game/UI/UpgradeIcons/T_Upgrade_EnergyOnKill_Icon",
        "icon_template_path": "/Game/UI/UpgradeIcons/T_Upgrade_LightningStormShocked_Icon",
        "granted_gameplay_effect_path": None,
        "granted_ability_path": "/Game/GAS/GameplayAbility/GA_EnergyOnKill",
    },
]

REQUIRED_CONFIG_KEYS = (
    "asset_name",
    "destination_path",
    "upgrade_id",
    "display_name",
    "description",
    "rarity",
    "upgrade_tags",
    "required_tags",
    "blocked_tags",
    "target_ability_tag",
    "trigger_event_tag",
    "damage_type_tag",
    "numeric_value",
    "max_stacks",
    "stackable",
    "granted_gameplay_effect_path",
    "granted_ability_path",
)


def _asset_path(config):
    """根据单项配置生成目标资产路径。"""
    return f"{config['destination_path']}/{config['asset_name']}"


def _validate_config_shape(config, index):
    """验证升级配置字段、唯一标识和基本数值边界。"""
    missing_keys = [key for key in REQUIRED_CONFIG_KEYS if key not in config]
    if missing_keys:
        raise RuntimeError(
            f"UPGRADE_CONFIGS[{index}] is missing keys: {', '.join(missing_keys)}"
        )
    if not config["asset_name"] or not config["destination_path"]:
        raise RuntimeError(f"UPGRADE_CONFIGS[{index}] has an empty asset path.")
    if not config["upgrade_id"]:
        raise RuntimeError(f"UPGRADE_CONFIGS[{index}] has an empty upgrade_id.")
    if int(config["max_stacks"]) < 1:
        raise RuntimeError(f"UPGRADE_CONFIGS[{index}] max_stacks must be at least 1.")


def _validate_references(config, generated_asset_paths):
    """验证升级配置中的 Tag、枚举和可选资产/Class 引用。"""
    rarity_type = tools.require_unreal_type("ArenaUpgradeRarity")
    tools.resolve_enum_value(rarity_type, config["rarity"])

    for tag_name in (
        list(config["upgrade_tags"])
        + list(config["required_tags"])
        + list(config["blocked_tags"])
    ):
        tools.make_tag(tag_name)
    tools.make_tag(config["target_ability_tag"])
    tools.make_optional_tag(config["trigger_event_tag"])
    tools.make_optional_tag(config["damage_type_tag"])

    if "icon_path" in config and config["icon_path"]:
        if unreal.EditorAssetLibrary.does_asset_exist(config["icon_path"]):
            tools.require_asset(config["icon_path"], unreal.Texture2D)
        elif config.get("icon_template_path"):
            tools.require_asset(config["icon_template_path"], unreal.Texture2D)
        else:
            raise RuntimeError(f"Required upgrade icon was not found: {config['icon_path']}")
    if config["granted_gameplay_effect_path"]:
        effect_path = config["granted_gameplay_effect_path"]
        if effect_path not in generated_asset_paths:
            tools.resolve_optional_blueprint_class(effect_path, unreal.GameplayEffect)
    if config["granted_ability_path"]:
        ability_path = config["granted_ability_path"]
        if ability_path not in generated_asset_paths:
            tools.resolve_optional_blueprint_class(ability_path, unreal.GameplayAbility)


def validate_configs(generated_asset_paths=None):
    """在写入前验证所有升级配置、引用和现有资产类型。"""
    generated_asset_paths = set(generated_asset_paths or ())
    upgrade_data_class = tools.require_unreal_type("ArenaUpgradeDataAsset")
    seen_asset_paths = set()
    seen_upgrade_ids = set()

    for index, config in enumerate(UPGRADE_CONFIGS):
        _validate_config_shape(config, index)
        asset_path = _asset_path(config)
        if asset_path in seen_asset_paths:
            raise RuntimeError(f"Duplicate upgrade asset path: {asset_path}")
        if config["upgrade_id"] in seen_upgrade_ids:
            raise RuntimeError(f"Duplicate upgrade ID: {config['upgrade_id']}")
        seen_asset_paths.add(asset_path)
        seen_upgrade_ids.add(config["upgrade_id"])

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_asset(asset_path, upgrade_data_class)
        _validate_references(config, generated_asset_paths)


def _ensure_configured_icon(config):
    """首次缺失时从模板复制独立图标资产，后续运行保留用户替换后的内容。"""
    icon_path = config.get("icon_path")
    icon_template_path = config.get("icon_template_path")
    if icon_path and icon_template_path:
        tools.duplicate_or_load_asset(icon_path, icon_template_path, unreal.Texture2D)
        tools.save_asset(icon_path)


def _create_or_load_upgrade(config):
    """幂等创建或加载单个 ArenaUpgradeDataAsset。"""
    upgrade_data_class = tools.require_unreal_type("ArenaUpgradeDataAsset")
    asset_path = _asset_path(config)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return tools.require_asset(asset_path, upgrade_data_class), asset_path

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", upgrade_data_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        config["asset_name"],
        config["destination_path"],
        upgrade_data_class,
        factory,
    )
    if asset is None:
        raise RuntimeError(f"Failed to create upgrade DataAsset: {asset_path}")
    return asset, asset_path


def _configure_upgrade(asset, config):
    """把配置值写入升级资产，并保留未提供的可选图标字段。"""
    asset.modify()
    asset.set_editor_property("upgrade_id", unreal.Name(config["upgrade_id"]))
    tools.set_localized_text_property(asset, "UpgradeName", config["display_name"])
    tools.set_localized_text_property(asset, "Description", config["description"])
    asset.set_editor_property(
        "rarity",
        tools.resolve_enum_value(
            tools.require_unreal_type("ArenaUpgradeRarity"),
            config["rarity"],
        ),
    )
    asset.set_editor_property(
        "upgrade_tags",
        tools.make_tag_container(config["upgrade_tags"]),
    )
    asset.set_editor_property(
        "required_tags",
        tools.make_tag_container(config["required_tags"]),
    )
    asset.set_editor_property(
        "blocked_tags",
        tools.make_tag_container(config["blocked_tags"]),
    )
    if "icon_path" in config:
        asset.set_editor_property(
            "icon",
            tools.resolve_optional_asset(config["icon_path"], unreal.Texture2D),
        )
    asset.set_editor_property(
        "granted_gameplay_effect",
        tools.resolve_optional_blueprint_class(
            config["granted_gameplay_effect_path"],
            unreal.GameplayEffect,
        ),
    )
    asset.set_editor_property(
        "granted_ability",
        tools.resolve_optional_blueprint_class(
            config["granted_ability_path"],
            unreal.GameplayAbility,
        ),
    )
    asset.set_editor_property(
        "target_ability_tag",
        tools.make_tag(config["target_ability_tag"]),
    )
    asset.set_editor_property(
        "trigger_event_tag",
        tools.make_optional_tag(config["trigger_event_tag"]),
    )
    asset.set_editor_property(
        "damage_type_tag",
        tools.make_optional_tag(config["damage_type_tag"]),
    )
    asset.set_editor_property("numeric_value", float(config["numeric_value"]))
    asset.set_editor_property("max_stacks", int(config["max_stacks"]))
    asset.set_editor_property("stackable", bool(config["stackable"]))


def run():
    """验证后创建或更新全部升级 DataAsset。"""
    validate_configs()
    for config in UPGRADE_CONFIGS:
        _ensure_configured_icon(config)
        asset, asset_path = _create_or_load_upgrade(config)
        _configure_upgrade(asset, config)
        tools.save_asset(asset_path)
        unreal.log(f"Configured upgrade DataAsset: {asset_path}")


def main():
    """提供可从 Unreal Editor 单独执行的升级资产入口。"""
    run()
    unreal.log("Upgrade DataAsset generation completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Upgrade DataAsset generation failed: {error}")
        raise
