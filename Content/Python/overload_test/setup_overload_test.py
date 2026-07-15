"""幂等创建 Overload 起始构筑、静止木桩和独立测试关卡。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

TEST_ROOT = "/Game/Tests/Overload"
TEST_GAME_MODE_PATH = f"{TEST_ROOT}/BP_ArenaGameMode_OverloadTest"
TEST_DUMMY_PATH = f"{TEST_ROOT}/BP_ArenaEnemy_OverloadDummy"
TEST_ATTRIBUTE_EFFECT_PATH = f"{TEST_ROOT}/GE_Test_OverloadDummyAttributes"
TEST_LEVEL_PATH = f"{TEST_ROOT}/Lvl_OverloadTest"

GAME_MODE_TEMPLATE_PATH = "/Game/GameMode/BP_ArenaGameMode"
DUMMY_TEMPLATE_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter"
STARTING_UPGRADE_PATHS = (
    "/Game/Data/Upgrade/DA_Upgrade_FireballDamage",
    "/Game/Data/Upgrade/DA_Upgrade_FireballBurning",
    "/Game/Data/Upgrade/DA_Upgrade_LightningStormDamage",
    "/Game/Data/Upgrade/DA_Upgrade_Overload",
)

DUMMY_TAG = unreal.Name("OverloadTestDummy")
DUMMY_LAYOUT = (
    ("OverloadDummy_Primary", 0.0),
    ("OverloadDummy_Inside250", 250.0),
    ("OverloadDummy_Outside350", 350.0),
)
GROUND_TRACE_UP_DISTANCE = 5000.0
GROUND_TRACE_DOWN_DISTANCE = 10000.0


def _require_native_types():
    """确认测试入口和木桩属性 GE 已完成编译并被当前编辑器加载。"""
    return (
        tools.require_unreal_type("ArenaGameMode"),
        tools.require_unreal_type("ArenaEnemyCharacter"),
        tools.require_unreal_type("ArenaGameplayEffect_OverloadTestAttributes"),
    )


def _configure_test_game_mode(arena_game_mode_class):
    """复制正式 GameMode 并仅在测试子类中关闭波次、掉落并配置起始升级。"""
    _, generated_class, asset_path = tools.duplicate_or_load_blueprint(
        "BP_ArenaGameMode_OverloadTest",
        TEST_ROOT,
        GAME_MODE_TEMPLATE_PATH,
        arena_game_mode_class,
    )
    defaults = unreal.get_default_object(generated_class)
    defaults.modify()
    defaults.set_editor_property("wave_data", None)
    defaults.set_editor_property("pickup_drop_table", None)
    defaults.set_editor_property("enable_debug_starting_upgrades", True)
    defaults.set_editor_property(
        "debug_starting_upgrades",
        [tools.require_asset(path) for path in STARTING_UPGRADE_PATHS],
    )
    tools.save_asset(asset_path)
    return generated_class


def _configure_test_attribute_effect(test_effect_class):
    """创建可在 Details 中检查的木桩属性 GE 蓝图子类。"""
    _, generated_class, asset_path = tools.create_or_load_blueprint(
        "GE_Test_OverloadDummyAttributes",
        TEST_ROOT,
        test_effect_class,
    )
    tools.save_asset(asset_path)
    return generated_class


def _configure_test_dummy(arena_enemy_class, attribute_effect_class):
    """复制正式敌人表现，但移除 AI、启动攻击并切换到高血量测试属性。"""
    _, generated_class, asset_path = tools.duplicate_or_load_blueprint(
        "BP_ArenaEnemy_OverloadDummy",
        TEST_ROOT,
        DUMMY_TEMPLATE_PATH,
        arena_enemy_class,
    )
    defaults = unreal.get_default_object(generated_class)
    defaults.modify()
    defaults.set_editor_property("ai_controller_class", None)
    defaults.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.DISABLED)
    defaults.set_editor_property("startup_abilities", [])
    defaults.set_editor_property("default_attribute_effect", attribute_effect_class)
    tools.save_asset(asset_path)
    return generated_class


def _find_player_start(actors):
    """选择测试关卡中的第一个 PlayerStart 作为木桩布局原点。"""
    for actor in actors:
        if isinstance(actor, unreal.PlayerStart):
            return actor
    raise RuntimeError(f"No PlayerStart exists in {TEST_LEVEL_PATH}.")


def _spawn_grounded_dummy(editor_actor_subsystem, dummy_class, label, desired_location):
    """在目标 XY 上方生成木桩，再用根胶囊向下 Sweep 直到其底部接触地面。"""
    spawn_location = unreal.Vector(
        desired_location.x,
        desired_location.y,
        desired_location.z + GROUND_TRACE_UP_DISTANCE,
    )
    sweep_end = unreal.Vector(
        desired_location.x,
        desired_location.y,
        desired_location.z - GROUND_TRACE_DOWN_DISTANCE,
    )
    dummy = editor_actor_subsystem.spawn_actor_from_class(
        dummy_class,
        spawn_location,
        unreal.Rotator(0.0, 0.0, 0.0),
        False,
    )
    if dummy is None:
        raise RuntimeError(f"Failed to spawn test dummy: {label}")

    dummy.set_actor_label(label)
    dummy.set_editor_property("tags", [DUMMY_TAG])
    dummy.set_actor_location(sweep_end, True, True)
    grounded_location = dummy.get_actor_location()
    if grounded_location.z <= sweep_end.z + 1.0:
        editor_actor_subsystem.destroy_actor(dummy)
        raise RuntimeError(
            "No blocking floor was found below overload dummy location "
            f"X={desired_location.x:.2f}, Y={desired_location.y:.2f}."
        )
    return dummy


def _replace_test_dummies(editor_actor_subsystem, dummy_class):
    """替换专用测试木桩，并让 Actor 根胶囊 Sweep 到 0/250/350 三个地面位置。"""
    actors = editor_actor_subsystem.get_all_level_actors()
    player_start = _find_player_start(actors)
    for actor in actors:
        if DUMMY_TAG in actor.get_editor_property("tags"):
            editor_actor_subsystem.destroy_actor(actor)

    origin = player_start.get_actor_location() + player_start.get_actor_forward_vector() * 800.0
    right_vector = player_start.get_actor_right_vector()
    for label, right_offset in DUMMY_LAYOUT:
        desired_location = origin + right_vector * right_offset
        _spawn_grounded_dummy(
            editor_actor_subsystem,
            dummy_class,
            label,
            desired_location,
        )


def _configure_test_level(game_mode_class, dummy_class):
    """仅在用户已经手动打开测试关卡时设置 GameMode，并生成三个范围边界木桩。"""
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor_subsystem.get_editor_world()
    if world is None:
        raise RuntimeError("Failed to resolve the current editor World.")
    current_level_path = tools.canonical_asset_path(world.get_path_name())
    if current_level_path != TEST_LEVEL_PATH:
        unreal.log_warning(
            "Overload test assets are ready. Use File > Save Current Level As to create "
            f"{TEST_LEVEL_PATH}, keep that level open, then run this script again."
        )
        return False

    world_settings = world.get_world_settings()
    world_settings.modify()
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    _replace_test_dummies(editor_actor_subsystem, dummy_class)

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.save_current_level():
        raise RuntimeError(f"Failed to save test level: {TEST_LEVEL_PATH}")
    return True


def run():
    """按依赖顺序创建并连接全部 Overload 测试资产。"""
    arena_game_mode_class, arena_enemy_class, test_effect_class = _require_native_types()
    game_mode_class = _configure_test_game_mode(arena_game_mode_class)
    attribute_effect_class = _configure_test_attribute_effect(test_effect_class)
    dummy_class = _configure_test_dummy(arena_enemy_class, attribute_effect_class)
    if _configure_test_level(game_mode_class, dummy_class):
        unreal.log(f"Overload test setup completed successfully: {TEST_LEVEL_PATH}")


def main():
    """提供可从 Unreal Editor 控制台单独执行的测试资产入口。"""
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Overload test setup failed: {error}")
        raise
