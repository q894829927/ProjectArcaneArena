"""幂等创建 Boss 多人人数缩放 GE，并连接到 Boss Character 默认配置。

必须先完整编译新增 C++ 类型并重启 Unreal Editor，再通过 Tools > Execute Python Script 执行。
脚本不会修改 Behavior Tree、StartupAbilities 或波次数据。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
SCALING_EFFECT_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Boss_PlayerCountScaling"


def _validate_prerequisites():
    """任何资产写入前确认最新原生类型和 Boss Character 蓝图均已加载。"""
    boss_class = tools.require_unreal_type("ArenaBossCharacter")
    scaling_effect_class = tools.require_unreal_type(
        "ArenaGameplayEffect_BossPlayerCountScaling"
    )
    tools.require_blueprint(BOSS_CHARACTER_PATH, boss_class)

    if unreal.EditorAssetLibrary.does_asset_exist(SCALING_EFFECT_PATH):
        tools.require_blueprint(SCALING_EFFECT_PATH, scaling_effect_class)

    tools.make_tag("SetByCaller.Boss.MaxHealthDelta")
    return scaling_effect_class


def _ensure_directories():
    """创建 Boss GameplayEffect 目录，已存在时不产生额外修改。"""
    effect_directory = "/Game/Boss/GAS/GameplayEffect"
    if not unreal.EditorAssetLibrary.does_directory_exist(effect_directory):
        unreal.EditorAssetLibrary.make_directory(effect_directory)


def _create_scaling_effect(scaling_effect_class):
    """创建或复用原生人数缩放 GE 的蓝图子类。"""
    _, generated_class, asset_path = tools.create_or_load_blueprint(
        "GE_Boss_PlayerCountScaling",
        "/Game/Boss/GAS/GameplayEffect",
        scaling_effect_class,
    )
    tools.save_asset(asset_path)
    return generated_class


def _configure_boss_character(scaling_effect_class):
    """写入单人和双人倍率及缩放 GE，不触碰 Boss 的技能与 AI 配置。"""
    _, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    boss_defaults.modify()
    boss_defaults.set_editor_property("single_player_health_multiplier", 1.0)
    boss_defaults.set_editor_property("two_player_health_multiplier", 1.75)
    boss_defaults.set_editor_property(
        "player_count_scaling_effect_class",
        scaling_effect_class,
    )
    tools.save_asset(BOSS_CHARACTER_PATH)


def main():
    """按预检、创建和连接顺序幂等配置 Boss 多人血量缩放。"""
    native_scaling_effect_class = _validate_prerequisites()
    _ensure_directories()

    with unreal.ScopedSlowTask(2, "Setting up Boss player-count scaling") as task:
        task.make_dialog(True)

        scaling_effect_class = _create_scaling_effect(
            native_scaling_effect_class
        )
        task.enter_progress_frame(1, "Created Boss player-count scaling effect")

        _configure_boss_character(scaling_effect_class)
        task.enter_progress_frame(1, "Configured Boss health multipliers")

    unreal.log(
        "Boss player-count scaling setup completed. "
        "Single-player uses 1.0x and two-player uses 1.75x MaxHealth. "
        "Behavior Tree, StartupAbilities, and wave data were not modified."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss player-count scaling setup failed: {error}")
        raise
