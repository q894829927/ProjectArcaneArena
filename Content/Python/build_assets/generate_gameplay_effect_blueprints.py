"""根据顶部配置创建原生 GameplayEffect 的 Blueprint 子类。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

EFFECT_BLUEPRINT_CONFIGS = [
    {
        "asset_name": "GE_Status_Burning",
        "destination_path": "/Game/GAS/GameplayEffect/Status",
        "parent_class_name": "ArenaGameplayEffect_Burning",
    },
    {
        "asset_name": "GE_Status_Shocked",
        "destination_path": "/Game/GAS/GameplayEffect/Status",
        "parent_class_name": "ArenaGameplayEffect_Shocked",
    },
    {
        "asset_name": "GE_Status_OverloadLockout",
        "destination_path": "/Game/GAS/GameplayEffect/Status",
        "parent_class_name": "ArenaGameplayEffect_OverloadLockout",
    },
    {
        "asset_name": "GE_Trigger_EnergyOnKill",
        "destination_path": "/Game/GAS/GameplayEffect/Trigger",
        "parent_class_name": "ArenaGameplayEffect_EnergyRestore",
    },
]

REQUIRED_CONFIG_KEYS = (
    "asset_name",
    "destination_path",
    "parent_class_name",
)


def _asset_path(config):
    """根据单项配置生成目标 GameplayEffect Blueprint 路径。"""
    return f"{config['destination_path']}/{config['asset_name']}"


def _resolve_parent_class(config):
    """解析并验证原生 GameplayEffect 父类。"""
    parent_class = tools.require_unreal_type(config["parent_class_name"])
    if not unreal.MathLibrary.class_is_child_of(parent_class, unreal.GameplayEffect):
        raise RuntimeError(
            f"{config['parent_class_name']} is not a GameplayEffect class."
        )
    return parent_class


def validate_configs():
    """在写入前验证配置唯一性、原生父类和现有 Blueprint 父类。"""
    seen_asset_paths = set()
    for index, config in enumerate(EFFECT_BLUEPRINT_CONFIGS):
        missing_keys = [key for key in REQUIRED_CONFIG_KEYS if key not in config]
        if missing_keys:
            raise RuntimeError(
                f"EFFECT_BLUEPRINT_CONFIGS[{index}] is missing keys: "
                f"{', '.join(missing_keys)}"
            )

        asset_path = _asset_path(config)
        if asset_path in seen_asset_paths:
            raise RuntimeError(f"Duplicate GameplayEffect Blueprint path: {asset_path}")
        seen_asset_paths.add(asset_path)

        parent_class = _resolve_parent_class(config)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)


def run():
    """验证后幂等创建全部原生 GameplayEffect 蓝图子类。"""
    validate_configs()
    for config in EFFECT_BLUEPRINT_CONFIGS:
        _, _, asset_path = tools.create_or_load_blueprint(
            config["asset_name"],
            config["destination_path"],
            _resolve_parent_class(config),
        )
        tools.save_asset(asset_path)
        unreal.log(f"Configured GameplayEffect Blueprint: {asset_path}")


def main():
    """提供可从 Unreal Editor 单独执行的 GameplayEffect 入口。"""
    run()
    unreal.log("GameplayEffect Blueprint generation completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"GameplayEffect Blueprint generation failed: {error}")
        raise
