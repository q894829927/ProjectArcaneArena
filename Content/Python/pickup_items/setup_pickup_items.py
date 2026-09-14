"""幂等创建拾取物 Blueprint、默认掉落表并连接 GameMode。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

PICKUP_CONFIGS = [
    {
        "asset_name": "BP_HealthPickup",
        "destination_path": "/Game/Items/Pickups",
        "pickup_type": "HEALTH",
        "restore_amount": 25.0,
        "life_span": 15.0,
    },
    {
        "asset_name": "BP_EnergyPickup",
        "destination_path": "/Game/Items/Pickups",
        "pickup_type": "ENERGY",
        "restore_amount": 20.0,
        "life_span": 15.0,
    },
]

DROP_TABLE_CONFIG = {
    "asset_name": "DA_PickupDropTable_Default",
    "destination_path": "/Game/Data/Pickup",
    "drop_chance": 0.25,
    "entries": [
        {"pickup_asset_name": "BP_HealthPickup", "weight": 1.0},
        {"pickup_asset_name": "BP_EnergyPickup", "weight": 1.0},
    ],
}

GAME_MODE_PATH = "/Game/GameMode/BP_ArenaGameMode"


def _pickup_asset_path(config):
    """根据拾取物配置生成稳定资产路径。"""
    return f"{config['destination_path']}/{config['asset_name']}"


def _drop_table_asset_path():
    """返回默认掉落表资产路径。"""
    return (
        f"{DROP_TABLE_CONFIG['destination_path']}/"
        f"{DROP_TABLE_CONFIG['asset_name']}"
    )


def validate_configs():
    """写入前验证反射类型、配置唯一性、数值边界和现有资产类型。"""
    pickup_actor_class = tools.require_unreal_type("ArenaPickupActor")
    pickup_type_enum = tools.require_unreal_type("ArenaPickupType")
    drop_table_class = tools.require_unreal_type("ArenaPickupDropTableDataAsset")
    tools.require_unreal_type("ArenaPickupDropEntry")

    pickup_names = set()
    pickup_paths = set()
    for index, config in enumerate(PICKUP_CONFIGS):
        required_keys = (
            "asset_name",
            "destination_path",
            "pickup_type",
            "restore_amount",
            "life_span",
        )
        missing_keys = [key for key in required_keys if key not in config]
        if missing_keys:
            raise RuntimeError(
                f"PICKUP_CONFIGS[{index}] is missing keys: "
                f"{', '.join(missing_keys)}"
            )
        if config["asset_name"] in pickup_names:
            raise RuntimeError(
                f"Duplicate pickup asset name: {config['asset_name']}"
            )
        if float(config["restore_amount"]) <= 0.0:
            raise RuntimeError(
                f"PICKUP_CONFIGS[{index}] restore_amount must be positive."
            )
        if float(config["life_span"]) <= 0.0:
            raise RuntimeError(
                f"PICKUP_CONFIGS[{index}] life_span must be positive."
            )

        tools.resolve_enum_value(pickup_type_enum, config["pickup_type"])
        asset_path = _pickup_asset_path(config)
        pickup_names.add(config["asset_name"])
        pickup_paths.add(asset_path)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, pickup_actor_class)

    drop_chance = float(DROP_TABLE_CONFIG["drop_chance"])
    if drop_chance < 0.0 or drop_chance > 1.0:
        raise RuntimeError("DROP_TABLE_CONFIG drop_chance must be in [0, 1].")
    for index, entry in enumerate(DROP_TABLE_CONFIG["entries"]):
        pickup_name = entry.get("pickup_asset_name")
        if pickup_name not in pickup_names:
            raise RuntimeError(
                f"DROP_TABLE_CONFIG entries[{index}] references unknown pickup: "
                f"{pickup_name}"
            )
        if float(entry.get("weight", 0.0)) <= 0.0:
            raise RuntimeError(
                f"DROP_TABLE_CONFIG entries[{index}] weight must be positive."
            )

    drop_table_path = _drop_table_asset_path()
    if unreal.EditorAssetLibrary.does_asset_exist(drop_table_path):
        tools.require_asset(drop_table_path, drop_table_class)

    _, game_mode_class = tools.require_blueprint(GAME_MODE_PATH)
    game_mode_defaults = unreal.get_default_object(game_mode_class)
    try:
        game_mode_defaults.get_editor_property("pickup_drop_table")
    except Exception as error:
        raise RuntimeError(
            f"GameMode {GAME_MODE_PATH} does not expose 'pickup_drop_table': "
            f"{error}"
        )


def _create_or_update_pickup(config):
    """幂等创建拾取物 Blueprint，并更新类默认的类型、恢复量和生命期。"""
    _, generated_class, asset_path = tools.create_or_load_blueprint(
        config["asset_name"],
        config["destination_path"],
        tools.require_unreal_type("ArenaPickupActor"),
    )
    pickup_defaults = unreal.get_default_object(generated_class)
    pickup_defaults.modify()
    pickup_defaults.set_editor_property(
        "pickup_type",
        tools.resolve_enum_value(
            tools.require_unreal_type("ArenaPickupType"),
            config["pickup_type"],
        ),
    )
    pickup_defaults.set_editor_property(
        "restore_amount",
        float(config["restore_amount"]),
    )
    pickup_defaults.set_editor_property(
        "pickup_life_span",
        float(config["life_span"]),
    )
    tools.save_asset(asset_path)
    unreal.log(f"Configured pickup Blueprint: {asset_path}")
    return generated_class


def _create_or_load_drop_table():
    """幂等创建默认掉落表 DataAsset。"""
    drop_table_class = tools.require_unreal_type("ArenaPickupDropTableDataAsset")
    asset_path = _drop_table_asset_path()
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return tools.require_asset(asset_path, drop_table_class), asset_path

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", drop_table_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        DROP_TABLE_CONFIG["asset_name"],
        DROP_TABLE_CONFIG["destination_path"],
        drop_table_class,
        factory,
    )
    if asset is None:
        raise RuntimeError(f"Failed to create pickup drop table: {asset_path}")
    return asset, asset_path


def _configure_drop_table(pickup_classes):
    """使用 Blueprint Class 和正权重重建默认掉落表条目。"""
    drop_table, asset_path = _create_or_load_drop_table()
    entry_type = tools.require_unreal_type("ArenaPickupDropEntry")
    entries = []
    for entry_config in DROP_TABLE_CONFIG["entries"]:
        entry = entry_type()
        entry.set_editor_property(
            "pickup_class",
            pickup_classes[entry_config["pickup_asset_name"]],
        )
        entry.set_editor_property("weight", float(entry_config["weight"]))
        entries.append(entry)

    drop_table.modify()
    drop_table.set_editor_property(
        "drop_chance",
        float(DROP_TABLE_CONFIG["drop_chance"]),
    )
    drop_table.set_editor_property("entries", entries)
    tools.save_asset(asset_path)
    unreal.log(f"Configured pickup drop table: {asset_path}")
    return drop_table


def _configure_game_mode(drop_table):
    """将默认掉落表连接到 GameMode 类默认对象。"""
    _, game_mode_class = tools.require_blueprint(GAME_MODE_PATH)
    game_mode_defaults = unreal.get_default_object(game_mode_class)
    game_mode_defaults.modify()
    game_mode_defaults.set_editor_property("pickup_drop_table", drop_table)
    tools.save_asset(GAME_MODE_PATH)
    unreal.log(f"Configured GameMode pickup drop table: {GAME_MODE_PATH}")


def run():
    """先完成全量预检，再按 Pickup、DropTable、GameMode 顺序写入资产。"""
    validate_configs()
    pickup_classes = {
        config["asset_name"]: _create_or_update_pickup(config)
        for config in PICKUP_CONFIGS
    }
    drop_table = _configure_drop_table(pickup_classes)
    _configure_game_mode(drop_table)


def main():
    """提供可从 Unreal Editor 执行的拾取资产总入口。"""
    run()
    unreal.log("Pickup item asset setup completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Pickup item asset setup failed: {error}")
        raise
