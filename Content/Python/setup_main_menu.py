"""幂等创建 Project Arcane Arena 主菜单资产并安全切换默认启动地图。

先完整编译新增 C++ 类型并重启 Unreal Editor，再在 Output Log 执行：
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/setup_main_menu.py"

脚本只在 Lvl_MainMenu 保存并通过类型检查后修改 DefaultEngine.ini 的
GameDefaultMap；EditorStartupMap 与 GlobalDefaultGameMode 保持不变。
"""

import importlib
import os
import re

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

MENU_DIRECTORY = "/Game/UI/MainMenu"
MENU_LEVEL_PATH = f"{MENU_DIRECTORY}/Lvl_MainMenu"
MENU_WIDGET_PATH = f"{MENU_DIRECTORY}/WBP_MainMenu"
MENU_CONTROLLER_PATH = f"{MENU_DIRECTORY}/BP_ArenaMainMenuPlayerController"
MENU_GAME_MODE_PATH = f"{MENU_DIRECTORY}/BP_ArenaMainMenuGameMode"
GAMEPLAY_MAP_PACKAGE = "/Game/TopDown/Lvl_TopDown"
GAME_DEFAULT_MAP_OBJECT_PATH = f"{MENU_LEVEL_PATH}.Lvl_MainMenu"


def _to_class_reference(unreal_type_or_class):
    """把原生 Python 包装类型统一转换为 TSubclassOf 可稳定设置和比较的 UClass 引用。"""
    static_class = getattr(unreal_type_or_class, "static_class", None)
    return static_class() if callable(static_class) else unreal_type_or_class


def _class_path(class_reference):
    """返回类引用的稳定对象路径，验证失败时同时展示实际值和预期值。"""
    resolved_class = _to_class_reference(class_reference)
    get_path_name = getattr(resolved_class, "get_path_name", None)
    return get_path_name() if callable(get_path_name) else str(resolved_class)


def _validate_prerequisites():
    """任何写入前验证最新 DLL、编辑器工厂、正式地图和已有目标资产类型。"""
    widget_parent = tools.require_unreal_type("ArenaMainMenuWidget")
    controller_parent = tools.require_unreal_type("ArenaMainMenuPlayerController")
    game_mode_parent = tools.require_unreal_type("ArenaMainMenuGameMode")
    lobby_player_state_class = _to_class_reference(
        tools.require_unreal_type("ArenaLobbyPlayerState")
    )
    menu_game_state_class = _to_class_reference(
        tools.require_unreal_type("ArenaMainMenuGameState")
    )
    widget_blueprint_class = tools.require_unreal_type("WidgetBlueprint")
    tools.require_unreal_type("WidgetBlueprintFactory")
    tools.require_unreal_type("WorldFactory")

    if not unreal.EditorAssetLibrary.does_asset_exist(GAMEPLAY_MAP_PACKAGE):
        raise RuntimeError(
            f"Gameplay map was not found: {GAMEPLAY_MAP_PACKAGE}"
        )

    if unreal.EditorAssetLibrary.does_asset_exist(MENU_WIDGET_PATH):
        widget_blueprint = tools.require_asset(
            MENU_WIDGET_PATH,
            widget_blueprint_class,
        )
        widget_class = tools.load_blueprint_class(MENU_WIDGET_PATH)
        if widget_blueprint is None or not unreal.MathLibrary.class_is_child_of(
            widget_class,
            widget_parent,
        ):
            raise RuntimeError(
                f"Existing Widget Blueprint has the wrong parent: {MENU_WIDGET_PATH}"
            )

    for asset_path, parent_class in (
        (MENU_CONTROLLER_PATH, controller_parent),
        (MENU_GAME_MODE_PATH, game_mode_parent),
    ):
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)

    if unreal.EditorAssetLibrary.does_asset_exist(MENU_LEVEL_PATH):
        tools.require_asset(MENU_LEVEL_PATH, unreal.World)

    return (
        widget_parent,
        controller_parent,
        game_mode_parent,
        lobby_player_state_class,
        menu_game_state_class,
        widget_blueprint_class,
    )


def _ensure_directory():
    """创建唯一主菜单目录，重复执行时复用现有目录。"""
    if not unreal.EditorAssetLibrary.does_directory_exist(MENU_DIRECTORY):
        unreal.EditorAssetLibrary.make_directory(MENU_DIRECTORY)


def _create_or_load_widget_blueprint(parent_class, widget_blueprint_class):
    """创建继承原生菜单 View 的 WBP 空壳，具体布局可由原生 fallback 或 Designer 提供。"""
    if unreal.EditorAssetLibrary.does_asset_exist(MENU_WIDGET_PATH):
        widget_blueprint = tools.require_asset(
            MENU_WIDGET_PATH,
            widget_blueprint_class,
        )
        generated_class = tools.load_blueprint_class(MENU_WIDGET_PATH)
        return widget_blueprint, generated_class

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    widget_blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WBP_MainMenu",
        MENU_DIRECTORY,
        widget_blueprint_class,
        factory,
    )
    if widget_blueprint is None:
        raise RuntimeError(f"Failed to create Widget Blueprint: {MENU_WIDGET_PATH}")

    generated_class = tools.load_blueprint_class(MENU_WIDGET_PATH)
    if not unreal.MathLibrary.class_is_child_of(generated_class, parent_class):
        raise RuntimeError(
            f"Created Widget Blueprint has the wrong parent: {MENU_WIDGET_PATH}"
        )
    return widget_blueprint, generated_class


def _create_runtime_blueprints(controller_parent, game_mode_parent):
    """创建菜单 Controller 与 GameMode 蓝图，并返回其 GeneratedClass。"""
    _, controller_class, controller_path = tools.create_or_load_blueprint(
        "BP_ArenaMainMenuPlayerController",
        MENU_DIRECTORY,
        controller_parent,
    )
    _, game_mode_class, game_mode_path = tools.create_or_load_blueprint(
        "BP_ArenaMainMenuGameMode",
        MENU_DIRECTORY,
        game_mode_parent,
    )
    return (
        controller_class,
        controller_path,
        game_mode_class,
        game_mode_path,
    )


def _configure_blueprint_defaults(
    widget_class,
    controller_class,
    controller_path,
    game_mode_class,
    game_mode_path,
    lobby_player_state_class,
    menu_game_state_class,
):
    """连接菜单 MVC 类与 Seamless Lobby，并保持菜单关卡不生成任何 Pawn 或 HUD。"""
    controller_defaults = unreal.get_default_object(controller_class)
    controller_defaults.modify()
    controller_defaults.set_editor_property("main_menu_widget_class", widget_class)
    controller_defaults.set_editor_property(
        "gameplay_map_name",
        unreal.Name(GAMEPLAY_MAP_PACKAGE),
    )

    game_mode_defaults = unreal.get_default_object(game_mode_class)
    game_mode_defaults.modify()
    game_mode_defaults.set_editor_property(
        "player_controller_class",
        controller_class,
    )
    game_mode_defaults.set_editor_property("default_pawn_class", None)
    game_mode_defaults.set_editor_property("spectator_class", None)
    game_mode_defaults.set_editor_property("hud_class", None)
    game_mode_defaults.set_editor_property(
        "player_state_class",
        lobby_player_state_class,
    )
    game_mode_defaults.set_editor_property(
        "game_state_class",
        menu_game_state_class,
    )
    game_mode_defaults.set_editor_property("use_seamless_travel", True)

    tools.save_asset(MENU_WIDGET_PATH)
    tools.save_asset(controller_path)
    tools.save_asset(game_mode_path)


def _create_or_load_menu_level(game_mode_class):
    """通过 WorldFactory 创建无 PlayerStart 的空关卡，并写入菜单专用 GameMode Override。"""
    if unreal.EditorAssetLibrary.does_asset_exist(MENU_LEVEL_PATH):
        world = tools.require_asset(MENU_LEVEL_PATH, unreal.World)
    else:
        world_factory = unreal.WorldFactory()
        for property_name in ("create_world_partition", "enable_world_partition_streaming"):
            try:
                world_factory.set_editor_property(property_name, False)
            except Exception:
                pass

        world = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "Lvl_MainMenu",
            MENU_DIRECTORY,
            unreal.World,
            world_factory,
        )
        if world is None:
            raise RuntimeError(f"Failed to create menu level: {MENU_LEVEL_PATH}")

    world_settings = world.get_world_settings()
    if world_settings is None:
        raise RuntimeError(f"Menu level has no WorldSettings: {MENU_LEVEL_PATH}")
    world_settings.modify()
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    tools.save_asset(MENU_LEVEL_PATH)

    saved_world = tools.require_asset(MENU_LEVEL_PATH, unreal.World)
    saved_game_mode = saved_world.get_world_settings().get_editor_property(
        "default_game_mode"
    )
    if saved_game_mode != game_mode_class:
        raise RuntimeError(
            "Lvl_MainMenu was saved, but its GameMode Override was not persisted."
        )
    return saved_world


def _update_game_default_map_after_level_save():
    """仅在菜单地图保存成功后定点修改 GameDefaultMap，并保持编辑器启动图与全局模式不变。"""
    if not unreal.EditorAssetLibrary.does_asset_exist(MENU_LEVEL_PATH):
        raise RuntimeError(
            "Refusing to update GameDefaultMap because Lvl_MainMenu is not saved."
        )

    config_path = os.path.join(
        unreal.Paths.project_config_dir(),
        "DefaultEngine.ini",
    )
    with open(config_path, "r", encoding="utf-8-sig", newline="") as config_file:
        original_text = config_file.read()

    section_pattern = re.compile(
        r"(?ms)^\[/Script/EngineSettings\.GameMapsSettings\]\s*\r?\n"
        r"(?P<body>.*?)(?=^\[|\Z)"
    )
    section_match = section_pattern.search(original_text)
    if not section_match:
        raise RuntimeError(
            "DefaultEngine.ini is missing GameMapsSettings; no config was changed."
        )

    section_text = section_match.group(0)
    editor_startup_match = re.search(
        r"(?m)^EditorStartupMap=(?P<value>[^\r\n]+)",
        section_text,
    )
    global_mode_match = re.search(
        r"(?m)^GlobalDefaultGameMode=(?P<value>[^\r\n]+)",
        section_text,
    )
    editor_startup_before = (
        editor_startup_match.group("value") if editor_startup_match else None
    )
    global_mode_before = (
        global_mode_match.group("value") if global_mode_match else None
    )

    replacement_line = f"GameDefaultMap={GAME_DEFAULT_MAP_OBJECT_PATH}"
    if re.search(r"(?m)^GameDefaultMap=", section_text):
        updated_section = re.sub(
            r"(?m)^GameDefaultMap=[^\r\n]*",
            replacement_line,
            section_text,
            count=1,
        )
    else:
        newline = "\r\n" if "\r\n" in original_text else "\n"
        header_end = section_text.find(newline) + len(newline)
        updated_section = (
            section_text[:header_end]
            + replacement_line
            + newline
            + section_text[header_end:]
        )

    updated_text = (
        original_text[: section_match.start()]
        + updated_section
        + original_text[section_match.end() :]
    )
    if updated_text != original_text:
        with open(config_path, "w", encoding="utf-8", newline="") as config_file:
            config_file.write(updated_text)

    verify_section = section_pattern.search(updated_text).group(0)
    if f"GameDefaultMap={GAME_DEFAULT_MAP_OBJECT_PATH}" not in verify_section:
        raise RuntimeError("GameDefaultMap verification failed after writing config.")
    if editor_startup_before and (
        f"EditorStartupMap={editor_startup_before}" not in verify_section
    ):
        raise RuntimeError("EditorStartupMap changed unexpectedly.")
    if global_mode_before and (
        f"GlobalDefaultGameMode={global_mode_before}" not in verify_section
    ):
        raise RuntimeError("GlobalDefaultGameMode changed unexpectedly.")


def _verify_menu_assets(
    widget_parent,
    controller_parent,
    game_mode_parent,
    widget_class,
    controller_class,
    game_mode_class,
    lobby_player_state_class,
    menu_game_state_class,
):
    """重新加载并核对父类、CDO、关卡 Override 和唯一资产集合，防止部分保存被误报成功。"""
    reloaded_widget_class = tools.load_blueprint_class(MENU_WIDGET_PATH)
    if not unreal.MathLibrary.class_is_child_of(
        reloaded_widget_class,
        widget_parent,
    ):
        raise RuntimeError("WBP_MainMenu parent verification failed.")

    _, reloaded_controller_class = tools.require_blueprint(
        MENU_CONTROLLER_PATH,
        controller_parent,
    )
    controller_defaults = unreal.get_default_object(reloaded_controller_class)
    if controller_defaults.get_editor_property("main_menu_widget_class") != widget_class:
        raise RuntimeError("MainMenuWidgetClass verification failed.")
    if str(controller_defaults.get_editor_property("gameplay_map_name")) != GAMEPLAY_MAP_PACKAGE:
        raise RuntimeError("GameplayMapName verification failed.")

    _, reloaded_game_mode_class = tools.require_blueprint(
        MENU_GAME_MODE_PATH,
        game_mode_parent,
    )
    game_mode_defaults = unreal.get_default_object(reloaded_game_mode_class)
    if game_mode_defaults.get_editor_property("player_controller_class") != controller_class:
        raise RuntimeError("Menu PlayerControllerClass verification failed.")
    for property_name in (
        "default_pawn_class",
        "spectator_class",
        "hud_class",
    ):
        if game_mode_defaults.get_editor_property(property_name) is not None:
            raise RuntimeError(
                f"Menu GameMode unexpectedly configures {property_name}."
            )
    actual_player_state_class = game_mode_defaults.get_editor_property(
        "player_state_class"
    )
    if _class_path(actual_player_state_class) != _class_path(lobby_player_state_class):
        raise RuntimeError(
            "Menu Lobby PlayerStateClass verification failed: "
            f"actual={_class_path(actual_player_state_class)}, "
            f"expected={_class_path(lobby_player_state_class)}."
        )
    actual_game_state_class = game_mode_defaults.get_editor_property(
        "game_state_class"
    )
    if _class_path(actual_game_state_class) != _class_path(menu_game_state_class):
        raise RuntimeError(
            "Menu Lobby GameStateClass verification failed: "
            f"actual={_class_path(actual_game_state_class)}, "
            f"expected={_class_path(menu_game_state_class)}."
        )
    if not game_mode_defaults.get_editor_property("use_seamless_travel"):
        raise RuntimeError("Menu GameMode must enable seamless travel for Lobby players.")

    saved_world = tools.require_asset(MENU_LEVEL_PATH, unreal.World)
    saved_game_mode = saved_world.get_world_settings().get_editor_property(
        "default_game_mode"
    )
    if saved_game_mode != game_mode_class:
        raise RuntimeError("Lvl_MainMenu GameMode Override verification failed.")

    expected_asset_paths = {
        MENU_LEVEL_PATH,
        MENU_WIDGET_PATH,
        MENU_CONTROLLER_PATH,
        MENU_GAME_MODE_PATH,
    }
    actual_asset_paths = {
        tools.canonical_asset_path(asset_path)
        for asset_path in unreal.EditorAssetLibrary.list_assets(
            MENU_DIRECTORY,
            recursive=False,
            include_folder=False,
        )
    }
    unexpected_assets = sorted(actual_asset_paths - expected_asset_paths)
    missing_assets = sorted(expected_asset_paths - actual_asset_paths)
    if missing_assets or unexpected_assets:
        raise RuntimeError(
            "Main menu asset set verification failed. "
            f"Missing={missing_assets}, Unexpected={unexpected_assets}"
        )


def _validate_gameplay_player_starts():
    """只读取正式地图并核对四个 PlayerStart；安全位置和 NavMesh 仍由关卡设计者手动确认。"""
    gameplay_world = tools.require_asset(GAMEPLAY_MAP_PACKAGE, unreal.World)
    try:
        persistent_level = gameplay_world.get_editor_property("persistent_level")
        actors = persistent_level.get_editor_property("actors")
        player_start_count = sum(
            1 for actor in actors if actor and isinstance(actor, unreal.PlayerStart)
        )
    except Exception as error:
        unreal.log_warning(
            "Could not inspect Lvl_TopDown PlayerStarts through this Unreal Python "
            f"build. Manually verify at least four non-overlapping PlayerStarts on "
            f"NavMesh. Details: {error}"
        )
        return

    if player_start_count < 4:
        unreal.log_warning(
            f"Lvl_TopDown currently exposes {player_start_count} PlayerStart actor(s). "
            "Direct IP four-player play requires at least four non-overlapping "
            "PlayerStarts placed on navigable ground. The script does not move or "
            "create gameplay spawn points automatically."
        )
    else:
        unreal.log(
            f"Lvl_TopDown PlayerStart validation passed: {player_start_count} found."
        )


def main():
    """按预检、创建、连接、保存、切换默认地图的顺序生成主菜单资产。"""
    (
        widget_parent,
        controller_parent,
        game_mode_parent,
        lobby_player_state_class,
        menu_game_state_class,
        widget_blueprint_class,
    ) = _validate_prerequisites()
    _ensure_directory()

    with unreal.ScopedSlowTask(6, "Setting up Project Arcane Arena main menu") as task:
        task.make_dialog(True)

        _, widget_class = _create_or_load_widget_blueprint(
            widget_parent,
            widget_blueprint_class,
        )
        task.enter_progress_frame(1, "Created WBP_MainMenu")

        (
            controller_class,
            controller_path,
            game_mode_class,
            game_mode_path,
        ) = _create_runtime_blueprints(controller_parent, game_mode_parent)
        task.enter_progress_frame(1, "Created menu Controller and GameMode")

        _configure_blueprint_defaults(
            widget_class,
            controller_class,
            controller_path,
            game_mode_class,
            game_mode_path,
            lobby_player_state_class,
            menu_game_state_class,
        )
        task.enter_progress_frame(1, "Connected menu Blueprint defaults")

        _create_or_load_menu_level(game_mode_class)
        task.enter_progress_frame(1, "Saved Lvl_MainMenu")

        _update_game_default_map_after_level_save()
        task.enter_progress_frame(1, "Updated GameDefaultMap safely")

        _verify_menu_assets(
            widget_parent,
            controller_parent,
            game_mode_parent,
            widget_class,
            controller_class,
            game_mode_class,
            lobby_player_state_class,
            menu_game_state_class,
        )
        _validate_gameplay_player_starts()
        task.enter_progress_frame(1, "Verified menu assets and class defaults")

    unreal.log(
        "Main menu setup completed: /Game/UI/MainMenu/Lvl_MainMenu is now "
        "GameDefaultMap; EditorStartupMap and GlobalDefaultGameMode were preserved."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(
            "Main menu setup failed. Existing assets were not duplicated; "
            "GameDefaultMap is only considered configured after verification. "
            f"Details: {error}"
        )
        raise
