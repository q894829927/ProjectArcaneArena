"""根据顶部配置创建或更新普通/暴击伤害数字 GameplayCue Blueprint。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

DAMAGE_NUMBER_CUE_CONFIGS = [
    {
        "asset_name": "GCN_DamageNumber",
        "destination_path": "/Game/GAS/GameplayCues/InstaneCue",
        "cue_tag": "GameplayCue.Damage.Number",
        "critical_style": False,
    },
    {
        "asset_name": "GCN_DamageCritical",
        "destination_path": "/Game/GAS/GameplayCues/InstaneCue",
        "cue_tag": "GameplayCue.Damage.Critical",
        "critical_style": True,
    },
]

REQUIRED_CONFIG_KEYS = (
    "asset_name",
    "destination_path",
    "cue_tag",
    "critical_style",
)


def _asset_path(config):
    """根据单项配置生成目标伤害数字 GameplayCue 路径。"""
    return f"{config['destination_path']}/{config['asset_name']}"


def _resolve_parent_class():
    """解析项目伤害数字 Cue 原生父类。"""
    parent_class = tools.require_unreal_type("ArenaGameplayCueNotify_DamageNumber")
    if not unreal.MathLibrary.class_is_child_of(
        parent_class,
        unreal.GameplayCueNotify_Static,
    ):
        raise RuntimeError(
            "ArenaGameplayCueNotify_DamageNumber is not a GameplayCueNotify_Static class."
        )
    return parent_class


def _validate_default_properties(generated_class, asset_path):
    """确认目标 Cue 类默认对象暴露标签与暴击样式配置。"""
    defaults = unreal.get_default_object(generated_class)
    for property_name in ("gameplay_cue_tag", "critical_style"):
        try:
            defaults.get_editor_property(property_name)
        except Exception as error:
            raise RuntimeError(
                f"GameplayCue {asset_path} does not expose '{property_name}': {error}"
            )


def validate_configs():
    """在写入前验证配置唯一性、Native Tag 和现有 Blueprint 父类。"""
    parent_class = _resolve_parent_class()
    seen_asset_paths = set()
    seen_cue_tags = set()

    for index, config in enumerate(DAMAGE_NUMBER_CUE_CONFIGS):
        missing_keys = [key for key in REQUIRED_CONFIG_KEYS if key not in config]
        if missing_keys:
            raise RuntimeError(
                f"DAMAGE_NUMBER_CUE_CONFIGS[{index}] is missing keys: "
                f"{', '.join(missing_keys)}"
            )

        asset_path = _asset_path(config)
        if asset_path in seen_asset_paths:
            raise RuntimeError(f"Duplicate damage number GameplayCue path: {asset_path}")
        if config["cue_tag"] in seen_cue_tags:
            raise RuntimeError(f"Duplicate damage number GameplayCue tag: {config['cue_tag']}")
        seen_asset_paths.add(asset_path)
        seen_cue_tags.add(config["cue_tag"])
        tools.make_tag(config["cue_tag"])

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            _, generated_class = tools.require_blueprint(asset_path, parent_class)
            _validate_default_properties(generated_class, asset_path)


def run():
    """验证后幂等创建并配置普通与暴击伤害数字 Cue。"""
    validate_configs()
    parent_class = _resolve_parent_class()
    for config in DAMAGE_NUMBER_CUE_CONFIGS:
        _, generated_class, asset_path = tools.create_or_load_blueprint(
            config["asset_name"],
            config["destination_path"],
            parent_class,
        )
        defaults = unreal.get_default_object(generated_class)
        defaults.modify()
        defaults.set_editor_property("gameplay_cue_tag", tools.make_tag(config["cue_tag"]))
        defaults.set_editor_property("critical_style", bool(config["critical_style"]))
        tools.save_asset(asset_path)
        unreal.log(f"Configured damage number GameplayCue: {asset_path}")


def main():
    """提供可从 Unreal Editor 单独执行的伤害数字 Cue 入口。"""
    run()
    unreal.log("Damage number GameplayCue generation completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Damage number GameplayCue generation failed: {error}")
        raise
