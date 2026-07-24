"""幂等创建 Boss 阶段系统的 Enrage GE、Niagara、GameplayCue 与 Character 配置。

必须先完整编译新增 C++ 类型并重启 Unreal Editor，再通过 Tools > Execute Python Script 执行。
脚本不会修改 BT_ArenaBoss 图，也不会覆盖已存在的手工 Niagara 调整。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_TRANSITION_NIAGARA = "/Game/Boss/VFX/NS_BossCharge_Impact"
SOURCE_ENRAGE_NIAGARA = "/Game/Boss/VFX/NS_BossFireZone_Active"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"

TRANSITION_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossPhase_Transition"
ENRAGE_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossEnraged_Active"
ENRAGE_EFFECT_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Boss_Enrage"
TRANSITION_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossPhase_Transition"
ENRAGE_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossEnraged_Active"


def _ensure_directories():
    """创建阶段系统沿用的 Boss VFX、GameplayEffect 与 GameplayCue 目录。"""
    for directory in (
        "/Game/Boss/VFX",
        "/Game/Boss/GAS/GameplayEffect",
        "/Game/Boss/GAS/GameplayCue",
    ):
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)


def _validate_existing_asset(asset_path, expected_class):
    """目标存在时只验证类型，避免脚本生成后缀副本或覆盖用户调好的资产。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        tools.require_asset(asset_path, expected_class)


def _validate_prerequisites():
    """任何写入前验证最新 DLL、Boss 蓝图、源 Niagara 和已存在目标类型。"""
    required_types = (
        "ArenaBossCharacter",
        "ArenaGameplayEffect_BossEnrage",
        "ArenaGameplayCueNotify_BossEnraged",
    )
    for type_name in required_types:
        tools.require_unreal_type(type_name)

    tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    tools.require_asset(SOURCE_TRANSITION_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_ENRAGE_NIAGARA, unreal.NiagaraSystem)

    for tag_name in (
        "GameplayCue.Boss.Phase.Transition",
        "GameplayCue.Boss.Enraged.Active",
    ):
        tools.make_tag(tag_name)

    for asset_path in (
        TRANSITION_NIAGARA_PATH,
        ENRAGE_NIAGARA_PATH,
    ):
        _validate_existing_asset(asset_path, unreal.NiagaraSystem)

    existing_blueprints = (
        (
            ENRAGE_EFFECT_PATH,
            tools.require_unreal_type("ArenaGameplayEffect_BossEnrage"),
        ),
        (TRANSITION_CUE_PATH, unreal.GameplayCueNotify_Burst),
        (
            ENRAGE_CUE_PATH,
            tools.require_unreal_type("ArenaGameplayCueNotify_BossEnraged"),
        ),
    )
    for asset_path, parent_class in existing_blueprints:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)


def _duplicate_visual_assets():
    """复制现有 Boss Impact 和 Fire Aura，后续调参不会影响旧技能表现。"""
    visual_assets = {
        "transition": tools.duplicate_or_load_asset(
            TRANSITION_NIAGARA_PATH,
            SOURCE_TRANSITION_NIAGARA,
            unreal.NiagaraSystem,
        ),
        "enrage": tools.duplicate_or_load_asset(
            ENRAGE_NIAGARA_PATH,
            SOURCE_ENRAGE_NIAGARA,
            unreal.NiagaraSystem,
        ),
    }
    tools.save_asset(TRANSITION_NIAGARA_PATH)
    tools.save_asset(ENRAGE_NIAGARA_PATH)
    return visual_assets


def _create_runtime_blueprints():
    """创建或复用 Enrage GE、阶段 Burst Cue 和原生持续 Enrage Cue。"""
    _, enrage_effect_class, enrage_effect_path = tools.create_or_load_blueprint(
        "GE_Boss_Enrage",
        "/Game/Boss/GAS/GameplayEffect",
        tools.require_unreal_type("ArenaGameplayEffect_BossEnrage"),
    )
    _, transition_cue_class, transition_cue_path = tools.create_or_load_blueprint(
        "GCN_BossPhase_Transition",
        "/Game/Boss/GAS/GameplayCue",
        unreal.GameplayCueNotify_Burst,
    )
    _, enrage_cue_class, enrage_cue_path = tools.create_or_load_blueprint(
        "GCN_BossEnraged_Active",
        "/Game/Boss/GAS/GameplayCue",
        tools.require_unreal_type("ArenaGameplayCueNotify_BossEnraged"),
    )
    tools.save_asset(enrage_effect_path)
    return {
        "enrage_effect_class": enrage_effect_class,
        "enrage_effect_path": enrage_effect_path,
        "transition_cue_class": transition_cue_class,
        "transition_cue_path": transition_cue_path,
        "enrage_cue_class": enrage_cue_class,
        "enrage_cue_path": enrage_cue_path,
    }


def _configure_transition_cue(cue_class, cue_path, transition_niagara):
    """配置不附着目标的一次性阶段爆发，RawMagnitude 继续由 C++ 携带阶段编号。"""
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Boss.Phase.Transition"),
    )
    defaults.set_editor_property(
        "default_placement_info",
        unreal.GameplayCueNotify_PlacementInfo(
            socket_name=unreal.Name("None"),
            attach_policy=unreal.GameplayCueNotify_AttachPolicy.DO_NOT_ATTACH,
            attachment_rule=unreal.AttachmentRule.KEEP_WORLD,
            override_rotation=False,
            override_scale=True,
            rotation_override=unreal.Rotator(0.0, 0.0, 0.0),
            scale_override=unreal.Vector(2.0, 2.0, 2.0),
        ),
    )
    defaults.set_editor_property(
        "burst_effects",
        unreal.GameplayCueNotify_BurstEffects(
            burst_particles=[
                unreal.GameplayCueNotify_ParticleInfo(
                    niagara_system=transition_niagara,
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ]
        ),
    )
    tools.save_asset(cue_path)


def _configure_enrage_cue(cue_class, cue_path, enrage_niagara):
    """配置附着 Boss 的持续火焰，并开启原生低频重播兜底。"""
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Boss.Enraged.Active"),
    )
    defaults.set_editor_property("enrage_system", enrage_niagara)
    defaults.set_editor_property("enrage_scale", unreal.Vector(1.5, 1.5, 1.5))
    defaults.set_editor_property("vertical_offset", 0.0)
    defaults.set_editor_property("restart_system_while_active", True)
    defaults.set_editor_property("system_replay_interval", 0.8)
    defaults.set_editor_property("auto_destroy_on_remove", True)
    tools.save_asset(cue_path)


def _configure_boss_character(enrage_effect_class):
    """写入阶段阈值和 Enrage GE 类，不修改 StartupAbilities 或 Behavior Tree。"""
    _, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    boss_defaults.modify()
    boss_defaults.set_editor_property("phase_two_health_ratio", 0.70)
    boss_defaults.set_editor_property("phase_three_health_ratio", 0.35)
    boss_defaults.set_editor_property("enrage_effect_class", enrage_effect_class)
    tools.save_asset(BOSS_CHARACTER_PATH)


def main():
    """按预检、复制、创建和连接顺序幂等生成 Boss 阶段资产。"""
    _validate_prerequisites()
    _ensure_directories()
    with unreal.ScopedSlowTask(4, "Setting up Boss phase system assets") as task:
        task.make_dialog(True)

        visual_assets = _duplicate_visual_assets()
        task.enter_progress_frame(1, "Copied phase transition and enrage Niagara")

        runtime_blueprints = _create_runtime_blueprints()
        task.enter_progress_frame(1, "Created phase GameplayEffect and GameplayCues")

        _configure_transition_cue(
            runtime_blueprints["transition_cue_class"],
            runtime_blueprints["transition_cue_path"],
            visual_assets["transition"],
        )
        _configure_enrage_cue(
            runtime_blueprints["enrage_cue_class"],
            runtime_blueprints["enrage_cue_path"],
            visual_assets["enrage"],
        )
        task.enter_progress_frame(1, "Configured phase GameplayCues")

        _configure_boss_character(runtime_blueprints["enrage_effect_class"])
        task.enter_progress_frame(1, "Configured Boss phase thresholds and Enrage effect")

    unreal.log(
        "Boss phase system setup completed. BT_ArenaBoss was not modified. "
        "Add optional BossPhaseText to WBP_PlayerHUD only if a separate phase label is desired."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss phase system setup failed: {error}")
        raise
