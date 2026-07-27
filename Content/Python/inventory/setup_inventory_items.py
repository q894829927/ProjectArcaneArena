"""幂等创建首批背包药水 DataAsset 和可放置 Pickup Blueprint。

必须先完整编译新增 C++ 类型并重启 Unreal Editor，再通过 Output Log 执行：
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/inventory/setup_inventory_items.py"
"""

import importlib
import re

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

DATA_DIRECTORY = "/Game/Items/Inventory/Data"
PICKUP_DIRECTORY = "/Game/Items/Inventory/Pickups"
EFFECT_DIRECTORY = "/Game/Items/Inventory/Effects"
UI_DIRECTORY = "/Game/UI/Inventory"
COOLDOWN_EFFECT_NAME = "GE_Cooldown_ItemConsumable"
INVENTORY_ROOT_DIRECTORY = "/Game/Items/Inventory"
INVENTORY_WIDGET_NAME = "WBP_Inventory"
INVENTORY_SLOT_WIDGET_NAME = "WBP_InventorySlot"
PLAYER_CONTROLLER_PATH = "/Game/Core/BP_ArenaPlayerController"

ITEM_CONFIGS = (
    {
        "asset_name": "DA_HealthPotion",
        "pickup_name": "BP_HealthPotionPickup",
        "item_tag": "Item.Consumable.HealthPotion",
        "item_tags": (
            "Item.Consumable.HealthPotion",
            "Item.Type.Consumable",
            "Item.Effect.Restore.Health",
        ),
        "display_name": "Health Potion",
        "description": "Restores 50 Health. Shared consumable cooldown: 1 second.",
        "max_stack_size": 10,
        "use_effect_type": "ArenaGameplayEffect_HealthRestore",
        "set_by_caller_tag": "SetByCaller.Recovery.Health",
        "use_magnitude": 50.0,
        "icon_path": "/Game/UI/UpgradeIcons/T_Upgrade_MaxHealth_Icon",
    },
    {
        "asset_name": "DA_EnergyPotion",
        "pickup_name": "BP_EnergyPotionPickup",
        "item_tag": "Item.Consumable.EnergyPotion",
        "item_tags": (
            "Item.Consumable.EnergyPotion",
            "Item.Type.Consumable",
            "Item.Effect.Restore.Energy",
        ),
        "display_name": "Energy Potion",
        "description": "Restores 25 Energy. Shared consumable cooldown: 1 second.",
        "max_stack_size": 10,
        "use_effect_type": "ArenaGameplayEffect_EnergyRestore",
        "set_by_caller_tag": "SetByCaller.Recovery.Energy",
        "use_magnitude": 25.0,
        "icon_path": "/Game/UI/UpgradeIcons/T_Upgrade_EnergyOnKill_Icon",
    },
)


def _asset_path(directory, asset_name):
    """拼接稳定包路径，避免重复执行生成带后缀资产。"""
    return f"{directory}/{asset_name}"


def _validate_prerequisites():
    """任何写入前验证最新原生类型、反射属性、GameplayTag 和复用资产均已加载。"""
    item_data_class = tools.require_unreal_type("ArenaItemDataAsset")
    pickup_class = tools.require_unreal_type("ArenaInventoryPickupActor")
    cooldown_effect_class = tools.require_unreal_type(
        "ArenaGameplayEffect_ConsumableCooldown"
    )
    inventory_widget_class = tools.require_unreal_type("ArenaInventoryWidget")
    inventory_slot_widget_class = tools.require_unreal_type(
        "ArenaInventorySlotWidget"
    )
    player_controller_class = tools.require_unreal_type("ArenaPlayerController")
    widget_blueprint_class = tools.require_unreal_type("WidgetBlueprint")
    tools.require_unreal_type("WidgetBlueprintFactory")

    inventory_widget_defaults = unreal.get_default_object(inventory_widget_class)
    try:
        inventory_widget_defaults.get_editor_property(
            "inventory_slot_widget_class"
        )
    except Exception as error:
        raise RuntimeError(
            "ArenaInventoryWidget is missing InventorySlotWidgetClass. "
            "Close Unreal Editor, compile the latest C++ code, and restart."
        ) from error

    _, controller_generated_class = tools.require_blueprint(
        PLAYER_CONTROLLER_PATH,
        player_controller_class,
    )
    controller_defaults = unreal.get_default_object(controller_generated_class)
    try:
        controller_defaults.get_editor_property("inventory_widget_class")
    except Exception as error:
        raise RuntimeError(
            f"{PLAYER_CONTROLLER_PATH} does not expose InventoryWidgetClass."
        ) from error

    cooldown_effect_path = _asset_path(EFFECT_DIRECTORY, COOLDOWN_EFFECT_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(cooldown_effect_path):
        tools.require_blueprint(cooldown_effect_path, cooldown_effect_class)

    widget_paths = (
        (
            _asset_path(UI_DIRECTORY, INVENTORY_SLOT_WIDGET_NAME),
            inventory_slot_widget_class,
        ),
        (
            _asset_path(UI_DIRECTORY, INVENTORY_WIDGET_NAME),
            inventory_widget_class,
        ),
    )
    for widget_path, expected_parent_class in widget_paths:
        if unreal.EditorAssetLibrary.does_asset_exist(widget_path):
            widget_blueprint = tools.require_asset(
                widget_path,
                widget_blueprint_class,
            )
            generated_class = tools.load_blueprint_class(widget_path)
            if not unreal.MathLibrary.class_is_child_of(
                generated_class,
                expected_parent_class,
            ):
                raise RuntimeError(
                    f"Widget Blueprint {widget_path} has the wrong parent class."
                )
            if widget_blueprint is None:
                raise RuntimeError(f"Failed to validate {widget_path}.")

    for config in ITEM_CONFIGS:
        tools.require_unreal_type(config["use_effect_type"])
        tools.require_asset(config["icon_path"], unreal.Texture2D)
        tools.make_tag(config["item_tag"])
        tools.make_tag(config["set_by_caller_tag"])
        for tag_name in config["item_tags"]:
            tools.make_tag(tag_name)

        data_path = _asset_path(DATA_DIRECTORY, config["asset_name"])
        if unreal.EditorAssetLibrary.does_asset_exist(data_path):
            tools.require_asset(data_path, item_data_class)

        pickup_path = _asset_path(PICKUP_DIRECTORY, config["pickup_name"])
        if unreal.EditorAssetLibrary.does_asset_exist(pickup_path):
            tools.require_blueprint(pickup_path, pickup_class)

    return (
        item_data_class,
        pickup_class,
        cooldown_effect_class,
        inventory_widget_class,
        inventory_slot_widget_class,
        player_controller_class,
        widget_blueprint_class,
    )


def _ensure_directories():
    """创建背包物品和 View 目录，已有目录保持不变。"""
    for directory in (
        DATA_DIRECTORY,
        PICKUP_DIRECTORY,
        EFFECT_DIRECTORY,
        UI_DIRECTORY,
    ):
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)


def _create_or_load_widget_blueprint(
    asset_name,
    destination_path,
    parent_class,
    widget_blueprint_class,
):
    """幂等创建真正的 Widget Blueprint，并验证生成类继承指定原生 View。"""
    asset_path = _asset_path(destination_path, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        widget_blueprint = tools.require_asset(
            asset_path,
            widget_blueprint_class,
        )
        generated_class = tools.load_blueprint_class(asset_path)
        if not unreal.MathLibrary.class_is_child_of(
            generated_class,
            parent_class,
        ):
            raise RuntimeError(
                f"Widget Blueprint {asset_path} has the wrong parent class."
            )
        return widget_blueprint, generated_class, asset_path

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    widget_blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        destination_path,
        widget_blueprint_class,
        factory,
    )
    if widget_blueprint is None:
        raise RuntimeError(f"Failed to create Widget Blueprint: {asset_path}")

    generated_class = tools.load_blueprint_class(asset_path)
    if not unreal.MathLibrary.class_is_child_of(
        generated_class,
        parent_class,
    ):
        raise RuntimeError(
            f"Created Widget Blueprint {asset_path} has the wrong parent class."
        )
    return widget_blueprint, generated_class, asset_path


def _create_and_connect_inventory_widgets(
    inventory_widget_class,
    inventory_slot_widget_class,
    player_controller_class,
    widget_blueprint_class,
):
    """创建两个背包 WBP，并连接槽位类与玩家 Controller 的 View 类引用。"""
    _, slot_generated_class, slot_path = _create_or_load_widget_blueprint(
        INVENTORY_SLOT_WIDGET_NAME,
        UI_DIRECTORY,
        inventory_slot_widget_class,
        widget_blueprint_class,
    )
    _, inventory_generated_class, inventory_path = (
        _create_or_load_widget_blueprint(
            INVENTORY_WIDGET_NAME,
            UI_DIRECTORY,
            inventory_widget_class,
            widget_blueprint_class,
        )
    )

    inventory_defaults = unreal.get_default_object(inventory_generated_class)
    inventory_defaults.modify()
    inventory_defaults.set_editor_property(
        "inventory_slot_widget_class",
        slot_generated_class,
    )

    controller_blueprint, controller_generated_class = tools.require_blueprint(
        PLAYER_CONTROLLER_PATH,
        player_controller_class,
    )
    controller_blueprint.modify()
    controller_defaults = unreal.get_default_object(controller_generated_class)
    controller_defaults.modify()
    controller_defaults.set_editor_property(
        "inventory_widget_class",
        inventory_generated_class,
    )

    tools.save_asset(slot_path)
    tools.save_asset(inventory_path)
    tools.save_asset(PLAYER_CONTROLLER_PATH)
    return inventory_generated_class, slot_generated_class


def _create_cooldown_effect(native_cooldown_effect_class):
    """创建可由设计师继续调整时长的共享消耗品冷却 GE 蓝图。"""
    _, generated_class, asset_path = tools.create_or_load_blueprint(
        COOLDOWN_EFFECT_NAME,
        EFFECT_DIRECTORY,
        native_cooldown_effect_class,
    )
    tools.save_asset(asset_path)
    return generated_class


def _create_or_load_item_data(config, item_data_class):
    """幂等创建指定 ArenaItemDataAsset。"""
    asset_path = _asset_path(DATA_DIRECTORY, config["asset_name"])
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return tools.require_asset(asset_path, item_data_class), asset_path

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", item_data_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        config["asset_name"],
        DATA_DIRECTORY,
        item_data_class,
        factory,
    )
    if asset is None:
        raise RuntimeError(f"Failed to create inventory item DataAsset: {asset_path}")
    return asset, asset_path


def _configure_item_data(
    item_data,
    config,
    pickup_native_class,
    cooldown_effect_class,
):
    """写入药水数据，并用原生 Pickup 类避免 DataAsset 与专属蓝图形成硬引用环。"""
    item_data.modify()
    item_data.set_editor_property("item_tag", tools.make_tag(config["item_tag"]))
    item_data.set_editor_property(
        "item_tags",
        tools.make_tag_container(config["item_tags"]),
    )
    tools.set_localized_text_property(
        item_data,
        "DisplayName",
        config["display_name"],
    )
    tools.set_localized_text_property(
        item_data,
        "Description",
        config["description"],
    )
    item_data.set_editor_property(
        "icon",
        tools.require_asset(config["icon_path"], unreal.Texture2D),
    )
    item_data.set_editor_property(
        "max_stack_size",
        int(config["max_stack_size"]),
    )
    item_data.set_editor_property(
        "use_gameplay_effect_class",
        tools.require_unreal_type(config["use_effect_type"]),
    )
    item_data.set_editor_property(
        "set_by_caller_magnitude_tag",
        tools.make_tag(config["set_by_caller_tag"]),
    )
    item_data.set_editor_property(
        "use_magnitude",
        float(config["use_magnitude"]),
    )
    item_data.set_editor_property(
        "cooldown_gameplay_effect_class",
        cooldown_effect_class,
    )
    item_data.set_editor_property(
        "world_pickup_class",
        pickup_native_class,
    )


def _configure_pickup_defaults(pickup_generated_class, item_data):
    """让生成的 Pickup Blueprint 可直接拖入关卡并默认提供一个对应药水。"""
    pickup_defaults = unreal.get_default_object(pickup_generated_class)
    pickup_defaults.modify()
    pickup_defaults.set_editor_property("item_data", item_data)
    pickup_defaults.set_editor_property("quantity", 1)


def _require_postcondition(condition, message):
    """统一抛出创建后配置错误，避免脚本只报告“执行完成”却留下不可用资产。"""
    if not condition:
        raise RuntimeError(f"Inventory asset verification failed: {message}")


def _export_struct(value):
    """把 GameplayTag 等结构导出为稳定文本，供编辑器版本无关的资产自检使用。"""
    export_text = getattr(value, "export_text", None)
    return export_text() if callable(export_text) else str(value)


def _unreal_reference_path(value):
    """把 UObject、UClass 或 Python 原生反射类型统一为可稳定比较的对象路径。"""
    if value is None:
        return ""

    get_path_name = getattr(value, "get_path_name", None)
    if callable(get_path_name):
        try:
            return get_path_name()
        except TypeError:
            pass

    static_class = getattr(value, "static_class", None)
    if callable(static_class):
        reflected_class = static_class()
        if reflected_class is not None:
            reflected_path_getter = getattr(reflected_class, "get_path_name", None)
            if callable(reflected_path_getter):
                return reflected_path_getter()

    return _export_struct(value)


def _verify_no_duplicate_assets():
    """拒绝同名 `_1/_2` 资产，确保重复执行仍保持唯一规范路径。"""
    expected_names = {
        COOLDOWN_EFFECT_NAME,
        INVENTORY_WIDGET_NAME,
        INVENTORY_SLOT_WIDGET_NAME,
        *(config["asset_name"] for config in ITEM_CONFIGS),
        *(config["pickup_name"] for config in ITEM_CONFIGS),
    }
    duplicate_paths = []
    for root_directory in (INVENTORY_ROOT_DIRECTORY, UI_DIRECTORY):
        for listed_path in unreal.EditorAssetLibrary.list_assets(
            root_directory,
            recursive=True,
            include_folder=False,
        ):
            asset_path = tools.canonical_asset_path(listed_path)
            asset_name = asset_path.rsplit("/", 1)[-1]
            if any(
                re.fullmatch(rf"{re.escape(expected_name)}_\d+", asset_name)
                for expected_name in expected_names
            ):
                duplicate_paths.append(asset_path)

    _require_postcondition(
        not duplicate_paths,
        f"duplicate suffixed assets exist: {sorted(duplicate_paths)}",
    )


def _verify_configured_assets(
    item_data_class,
    pickup_native_class,
    native_cooldown_effect_class,
    inventory_widget_class,
    inventory_slot_widget_class,
    player_controller_class,
    widget_blueprint_class,
):
    """保存后重新加载资产并核对物品、GE、Pickup、WBP 与 Controller 引用。"""
    cooldown_path = _asset_path(EFFECT_DIRECTORY, COOLDOWN_EFFECT_NAME)
    _, cooldown_generated_class = tools.require_blueprint(
        cooldown_path,
        native_cooldown_effect_class,
    )
    verified_lines = [f"Verified {cooldown_path}"]

    for config in ITEM_CONFIGS:
        item_path = _asset_path(DATA_DIRECTORY, config["asset_name"])
        pickup_path = _asset_path(PICKUP_DIRECTORY, config["pickup_name"])
        item_data = tools.require_asset(item_path, item_data_class)
        _, pickup_generated_class = tools.require_blueprint(
            pickup_path,
            pickup_native_class,
        )
        pickup_defaults = unreal.get_default_object(pickup_generated_class)

        _require_postcondition(
            _export_struct(item_data.get_editor_property("item_tag"))
            == _export_struct(tools.make_tag(config["item_tag"])),
            f"{item_path}.ItemTag does not equal {config['item_tag']}",
        )
        serialized_item_tags = _export_struct(
            item_data.get_editor_property("item_tags")
        )
        for expected_tag in config["item_tags"]:
            _require_postcondition(
                expected_tag in serialized_item_tags,
                f"{item_path}.ItemTags is missing {expected_tag}",
            )
        _require_postcondition(
            int(item_data.get_editor_property("max_stack_size"))
            == int(config["max_stack_size"]),
            f"{item_path}.MaxStackSize is incorrect",
        )
        _require_postcondition(
            abs(
                float(item_data.get_editor_property("use_magnitude"))
                - float(config["use_magnitude"])
            )
            <= 0.001,
            f"{item_path}.UseMagnitude is incorrect",
        )
        _require_postcondition(
            _export_struct(
                item_data.get_editor_property("set_by_caller_magnitude_tag")
            )
            == _export_struct(tools.make_tag(config["set_by_caller_tag"])),
            f"{item_path}.SetByCallerMagnitudeTag is incorrect",
        )

        expected_use_effect = tools.require_unreal_type(config["use_effect_type"])
        actual_use_effect = item_data.get_editor_property(
            "use_gameplay_effect_class"
        )
        _require_postcondition(
            actual_use_effect is not None
            and unreal.MathLibrary.class_is_child_of(
                actual_use_effect,
                expected_use_effect,
            ),
            f"{item_path}.UseGameplayEffectClass is incorrect",
        )
        actual_cooldown_effect = item_data.get_editor_property(
            "cooldown_gameplay_effect_class"
        )
        _require_postcondition(
            actual_cooldown_effect is not None
            and unreal.MathLibrary.class_is_child_of(
                actual_cooldown_effect,
                native_cooldown_effect_class,
            )
            and _unreal_reference_path(actual_cooldown_effect)
            == _unreal_reference_path(cooldown_generated_class),
            f"{item_path}.CooldownGameplayEffectClass is not {cooldown_path}",
        )
        actual_pickup_class = item_data.get_editor_property("world_pickup_class")
        _require_postcondition(
            actual_pickup_class is not None
            and unreal.MathLibrary.class_is_child_of(
                actual_pickup_class,
                pickup_native_class,
            )
            and _unreal_reference_path(actual_pickup_class)
            == _unreal_reference_path(pickup_native_class),
            f"{item_path}.WorldPickupClass must remain the native pickup class",
        )
        actual_icon = item_data.get_editor_property("icon")
        _require_postcondition(
            actual_icon is not None
            and config["icon_path"] in _export_struct(actual_icon),
            f"{item_path}.Icon does not reference {config['icon_path']}",
        )
        _require_postcondition(
            _unreal_reference_path(
                pickup_defaults.get_editor_property("item_data")
            )
            == _unreal_reference_path(item_data),
            f"{pickup_path} default ItemData does not reference {item_path}",
        )
        _require_postcondition(
            int(pickup_defaults.get_editor_property("quantity")) == 1,
            f"{pickup_path} default Quantity is not 1",
        )

        verified_lines.append(
            f"Verified {item_path}: {config['item_tag']}, "
            f"MaxStack={config['max_stack_size']}, "
            f"Magnitude={config['use_magnitude']}"
        )
        verified_lines.append(
            f"Verified {pickup_path}: ItemData={item_path}, Quantity=1"
        )

    inventory_path = _asset_path(UI_DIRECTORY, INVENTORY_WIDGET_NAME)
    slot_path = _asset_path(UI_DIRECTORY, INVENTORY_SLOT_WIDGET_NAME)
    tools.require_asset(inventory_path, widget_blueprint_class)
    tools.require_asset(slot_path, widget_blueprint_class)
    inventory_generated_class = tools.load_blueprint_class(inventory_path)
    slot_generated_class = tools.load_blueprint_class(slot_path)
    _require_postcondition(
        unreal.MathLibrary.class_is_child_of(
            inventory_generated_class,
            inventory_widget_class,
        ),
        f"{inventory_path} is not based on ArenaInventoryWidget",
    )
    _require_postcondition(
        unreal.MathLibrary.class_is_child_of(
            slot_generated_class,
            inventory_slot_widget_class,
        ),
        f"{slot_path} is not based on ArenaInventorySlotWidget",
    )
    inventory_defaults = unreal.get_default_object(inventory_generated_class)
    _require_postcondition(
        _unreal_reference_path(
            inventory_defaults.get_editor_property(
                "inventory_slot_widget_class"
            )
        )
        == _unreal_reference_path(slot_generated_class),
        f"{inventory_path}.InventorySlotWidgetClass does not reference {slot_path}",
    )

    _, controller_generated_class = tools.require_blueprint(
        PLAYER_CONTROLLER_PATH,
        player_controller_class,
    )
    controller_defaults = unreal.get_default_object(controller_generated_class)
    _require_postcondition(
        _unreal_reference_path(
            controller_defaults.get_editor_property("inventory_widget_class")
        )
        == _unreal_reference_path(inventory_generated_class),
        f"{PLAYER_CONTROLLER_PATH}.InventoryWidgetClass does not reference "
        f"{inventory_path}",
    )
    verified_lines.append(
        f"Verified {slot_path}: Parent=ArenaInventorySlotWidget"
    )
    verified_lines.append(
        f"Verified {inventory_path}: SlotClass={slot_path}"
    )
    verified_lines.append(
        f"Verified {PLAYER_CONTROLLER_PATH}: InventoryWidgetClass={inventory_path}"
    )

    _verify_no_duplicate_assets()
    for line in verified_lines:
        unreal.log(line)


def main():
    """按预检、创建、交叉连接、保存和重新加载自检顺序生成首批背包资产。"""
    (
        item_data_class,
        pickup_native_class,
        native_cooldown_effect_class,
        inventory_widget_class,
        inventory_slot_widget_class,
        player_controller_class,
        widget_blueprint_class,
    ) = _validate_prerequisites()
    _ensure_directories()

    with unreal.ScopedSlowTask(
        len(ITEM_CONFIGS) + 2,
        "Setting up inventory consumables",
    ) as task:
        task.make_dialog(True)

        _create_and_connect_inventory_widgets(
            inventory_widget_class,
            inventory_slot_widget_class,
            player_controller_class,
            widget_blueprint_class,
        )
        task.enter_progress_frame(1, "Configured inventory Widget Blueprints")

        cooldown_effect_class = _create_cooldown_effect(
            native_cooldown_effect_class
        )
        task.enter_progress_frame(1, "Configured shared consumable cooldown")

        for config in ITEM_CONFIGS:
            pickup_blueprint, pickup_generated_class, pickup_path = (
                tools.create_or_load_blueprint(
                    config["pickup_name"],
                    PICKUP_DIRECTORY,
                    pickup_native_class,
                )
            )
            item_data, item_data_path = _create_or_load_item_data(
                config,
                item_data_class,
            )
            _configure_item_data(
                item_data,
                config,
                pickup_native_class,
                cooldown_effect_class,
            )
            _configure_pickup_defaults(pickup_generated_class, item_data)

            tools.save_asset(item_data_path)
            tools.save_asset(pickup_path)
            task.enter_progress_frame(
                1,
                f"Configured {config['display_name']}",
            )

    _verify_configured_assets(
        item_data_class,
        pickup_native_class,
        native_cooldown_effect_class,
        inventory_widget_class,
        inventory_slot_widget_class,
        player_controller_class,
        widget_blueprint_class,
    )
    unreal.log(
        "Inventory item setup and post-save verification completed. "
        "WBP_Inventory is connected to BP_ArenaPlayerController. "
        "Place BP_HealthPotionPickup and BP_EnergyPotionPickup in a level, "
        "then use G to collect them."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Inventory item setup failed: {error}")
        raise
