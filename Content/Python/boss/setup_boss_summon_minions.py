"""幂等创建 Boss 召唤物动画、GAS 与 GameplayCue 资产。

必须先完成 C++ 编译并重启 Unreal Editor，再通过 Tools > Execute Python Script 执行。
脚本不会修改 Behavior Tree 图；SummonMinions 分支按本目录 README 手动连接。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_SUMMON_ANIMATION = "/Game/Boss/Animation/AS_BossFireZone"
SOURCE_CAST_NIAGARA = "/Game/Boss/VFX/NS_BossPhase_Transition"
SOURCE_SPAWN_NIAGARA = "/Game/Boss/VFX/NS_BossGroundSlam_Impact"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
MELEE_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter"
RANGED_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaRangedEnemy"

SUMMON_ANIMATION_PATH = "/Game/Boss/Animation/AS_BossSummonMinions"
SUMMON_MONTAGE_PATH = "/Game/Boss/Animation/AM_BossSummonMinions"
SUMMON_CAST_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossSummon_Cast"
SUMMON_SPAWN_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossSummon_Spawn"
SUMMON_ABILITY_PATH = "/Game/Boss/GAS/GameplayAbility/GA_BossSummonMinions"
SUMMON_COOLDOWN_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossSummonMinions"
SUMMON_CAST_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossSummon_Cast"
SUMMON_SPAWN_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossSummon_Spawn"


def _get_editor_property(obj, *property_names):
    """兼容 UE Python 的下划线命名和少量原生属性别名。"""
    last_error = None
    for property_name in property_names:
        try:
            return obj.get_editor_property(property_name)
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not read any property {property_names} from {obj}: {last_error}"
    )


def _set_editor_property(obj, value, *property_names):
    """按候选名称写入第一个可编辑属性，全部失败时给出明确错误。"""
    last_error = None
    for property_name in property_names:
        try:
            obj.set_editor_property(property_name, value)
            return
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not set any property {property_names} on {obj}: {last_error}"
    )


def _ensure_directories():
    """创建召唤技能使用的 Boss 资产目录。"""
    for directory in (
        "/Game/Boss/Animation",
        "/Game/Boss/VFX",
        "/Game/Boss/GAS/GameplayAbility",
        "/Game/Boss/GAS/GameplayEffect",
        "/Game/Boss/GAS/GameplayCue",
    ):
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)


def _validate_existing_asset(asset_path, expected_class):
    """目标已存在时只验证类型，避免创建后缀资产或覆盖人工调整。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        tools.require_asset(asset_path, expected_class)


def _validate_prerequisites():
    """在任何写入前验证原生类型、源表现、Boss 和两类召唤敌人。"""
    required_types = (
        "ArenaBossCharacter",
        "ArenaEnemyCharacter",
        "ArenaGameplayAbility_BossSummonMinions",
        "ArenaGameplayEffect_BossSummonMinionsCooldown",
    )
    for type_name in required_types:
        tools.require_unreal_type(type_name)

    boss_class = tools.require_unreal_type("ArenaBossCharacter")
    enemy_class = tools.require_unreal_type("ArenaEnemyCharacter")
    tools.require_blueprint(BOSS_CHARACTER_PATH, boss_class)
    tools.require_blueprint(MELEE_ENEMY_PATH, enemy_class)
    tools.require_blueprint(RANGED_ENEMY_PATH, enemy_class)
    tools.require_asset(SOURCE_SUMMON_ANIMATION, unreal.AnimSequence)
    tools.require_asset(SOURCE_CAST_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_SPAWN_NIAGARA, unreal.NiagaraSystem)

    for tag_name in (
        "GameplayCue.Ability.Boss.Summon.Cast",
        "GameplayCue.Ability.Boss.Summon.Spawn",
    ):
        tools.make_tag(tag_name)

    for asset_path, expected_class in (
        (SUMMON_ANIMATION_PATH, unreal.AnimSequence),
        (SUMMON_MONTAGE_PATH, unreal.AnimMontage),
        (SUMMON_CAST_NIAGARA_PATH, unreal.NiagaraSystem),
        (SUMMON_SPAWN_NIAGARA_PATH, unreal.NiagaraSystem),
    ):
        _validate_existing_asset(asset_path, expected_class)

    ability_parent = tools.require_unreal_type("ArenaGameplayAbility_BossSummonMinions")
    cooldown_parent = tools.require_unreal_type(
        "ArenaGameplayEffect_BossSummonMinionsCooldown"
    )
    for asset_path, parent_class in (
        (SUMMON_ABILITY_PATH, ability_parent),
        (SUMMON_COOLDOWN_PATH, cooldown_parent),
        (SUMMON_CAST_CUE_PATH, unreal.GameplayCueNotify_Burst),
        (SUMMON_SPAWN_CUE_PATH, unreal.GameplayCueNotify_Burst),
    ):
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)


def _duplicate_direct_assets():
    """复制召唤动画与两种一次性 Niagara 到 Boss 专属目录。"""
    assets = {
        "animation": tools.duplicate_or_load_asset(
            SUMMON_ANIMATION_PATH,
            SOURCE_SUMMON_ANIMATION,
            unreal.AnimSequence,
        ),
        "cast": tools.duplicate_or_load_asset(
            SUMMON_CAST_NIAGARA_PATH,
            SOURCE_CAST_NIAGARA,
            unreal.NiagaraSystem,
        ),
        "spawn": tools.duplicate_or_load_asset(
            SUMMON_SPAWN_NIAGARA_PATH,
            SOURCE_SPAWN_NIAGARA,
            unreal.NiagaraSystem,
        ),
    }
    for asset_path in (
        SUMMON_ANIMATION_PATH,
        SUMMON_CAST_NIAGARA_PATH,
        SUMMON_SPAWN_NIAGARA_PATH,
    ):
        tools.save_asset(asset_path)
    return assets


def _disable_animation_root_motion(animation):
    """关闭召唤动画 Root Motion，Boss 在施法时由服务器移动状态保持原地。"""
    if bool(_get_editor_property(animation, "enable_root_motion", "bEnableRootMotion")):
        animation.modify()
        _set_editor_property(
            animation,
            False,
            "enable_root_motion",
            "bEnableRootMotion",
        )
        unreal.EditorAssetLibrary.save_loaded_asset(animation, False)


def _create_or_load_montage(animation):
    """从 Boss 兼容动画创建 DefaultSlot Montage，重复执行时复用原资产。"""
    if unreal.EditorAssetLibrary.does_asset_exist(SUMMON_MONTAGE_PATH):
        return tools.require_asset(SUMMON_MONTAGE_PATH, unreal.AnimMontage)

    montage_factory = unreal.AnimMontageFactory()
    _set_editor_property(
        montage_factory,
        animation,
        "source_animation",
        "SourceAnimation",
    )
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_BossSummonMinions",
        "/Game/Boss/Animation",
        unreal.AnimMontage,
        montage_factory,
    )
    if montage is None:
        raise RuntimeError(
            f"Failed to create summon montage: {SUMMON_MONTAGE_PATH}"
        )
    tools.save_asset(SUMMON_MONTAGE_PATH)
    return montage


def _create_runtime_blueprints():
    """创建召唤 Ability、Cooldown 和两个原生 Burst GameplayCue。"""
    _, cooldown_class, cooldown_path = tools.create_or_load_blueprint(
        "GE_Cooldown_BossSummonMinions",
        "/Game/Boss/GAS/GameplayEffect",
        tools.require_unreal_type("ArenaGameplayEffect_BossSummonMinionsCooldown"),
    )
    _, ability_class, ability_path = tools.create_or_load_blueprint(
        "GA_BossSummonMinions",
        "/Game/Boss/GAS/GameplayAbility",
        tools.require_unreal_type("ArenaGameplayAbility_BossSummonMinions"),
    )
    _, cast_cue_class, cast_cue_path = tools.create_or_load_blueprint(
        "GCN_BossSummon_Cast",
        "/Game/Boss/GAS/GameplayCue",
        unreal.GameplayCueNotify_Burst,
    )
    _, spawn_cue_class, spawn_cue_path = tools.create_or_load_blueprint(
        "GCN_BossSummon_Spawn",
        "/Game/Boss/GAS/GameplayCue",
        unreal.GameplayCueNotify_Burst,
    )
    tools.save_asset(cooldown_path)
    return {
        "cooldown_class": cooldown_class,
        "cooldown_path": cooldown_path,
        "ability_class": ability_class,
        "ability_path": ability_path,
        "cast_cue_class": cast_cue_class,
        "cast_cue_path": cast_cue_path,
        "spawn_cue_class": spawn_cue_class,
        "spawn_cue_path": spawn_cue_path,
    }


def _configure_ability(runtime_blueprints, montage):
    """连接近战、远程 Class、冷却、Montage 和第一版召唤数值。"""
    _, melee_enemy_class = tools.require_blueprint(
        MELEE_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    _, ranged_enemy_class = tools.require_blueprint(
        RANGED_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    defaults = unreal.get_default_object(runtime_blueprints["ability_class"])
    defaults.modify()
    defaults.set_editor_property(
        "cooldown_gameplay_effect_class",
        runtime_blueprints["cooldown_class"],
    )
    defaults.set_editor_property(
        "summon_classes",
        [melee_enemy_class, ranged_enemy_class],
    )
    defaults.set_editor_property("summons_per_cast", 2)
    defaults.set_editor_property("attack_range", 1200.0)
    defaults.set_editor_property("range_tolerance", 25.0)
    defaults.set_editor_property("cast_duration", 0.9)
    defaults.set_editor_property("spawn_radius", 320.0)
    defaults.set_editor_property("spawn_candidate_count", 16)
    defaults.set_editor_property("spawn_candidate_ring_step", 100.0)
    defaults.set_editor_property("spawn_point_separation", 160.0)
    defaults.set_editor_property("spawn_capsule_radius", 50.0)
    defaults.set_editor_property("spawn_capsule_half_height", 90.0)
    defaults.set_editor_property("summon_montage", montage)
    defaults.set_editor_property("montage_play_rate", 1.25)
    tools.save_asset(runtime_blueprints["ability_path"])


def _configure_burst_cue(
    cue_class,
    cue_path,
    tag_name,
    niagara_system,
    attach_to_target,
    scale,
):
    """配置一次性 Niagara Cue，并明确选择附着 Boss 或固定世界位置。"""
    attach_policy = (
        unreal.GameplayCueNotify_AttachPolicy.ATTACH_TO_TARGET
        if attach_to_target
        else unreal.GameplayCueNotify_AttachPolicy.DO_NOT_ATTACH
    )
    attachment_rule = (
        unreal.AttachmentRule.KEEP_RELATIVE
        if attach_to_target
        else unreal.AttachmentRule.KEEP_WORLD
    )
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property("gameplay_cue_tag", tools.make_tag(tag_name))
    defaults.set_editor_property(
        "default_placement_info",
        unreal.GameplayCueNotify_PlacementInfo(
            socket_name=unreal.Name("None"),
            attach_policy=attach_policy,
            attachment_rule=attachment_rule,
            override_rotation=False,
            override_scale=True,
            rotation_override=unreal.Rotator(0.0, 0.0, 0.0),
            scale_override=scale,
        ),
    )
    defaults.set_editor_property(
        "burst_effects",
        unreal.GameplayCueNotify_BurstEffects(
            burst_particles=[
                unreal.GameplayCueNotify_ParticleInfo(
                    niagara_system=niagara_system,
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ]
        ),
    )
    tools.save_asset(cue_path)


def _append_summon_to_boss(ability_class):
    """保留 Boss 现有技能顺序，仅在缺失时追加一份召唤 Ability 并设置数量上限。"""
    _, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    startup_abilities = list(boss_defaults.get_editor_property("startup_abilities"))
    summon_class_path = ability_class.get_path_name()
    matching_indices = [
        index
        for index, startup_class in enumerate(startup_abilities)
        if startup_class and startup_class.get_path_name() == summon_class_path
    ]
    if not matching_indices:
        startup_abilities.append(ability_class)
    elif len(matching_indices) > 1:
        first_index = matching_indices[0]
        startup_abilities = [
            startup_class
            for index, startup_class in enumerate(startup_abilities)
            if startup_class.get_path_name() != summon_class_path
            or index == first_index
        ]

    boss_defaults.modify()
    boss_defaults.set_editor_property("startup_abilities", startup_abilities)
    boss_defaults.set_editor_property("max_active_summons", 4)
    tools.save_asset(BOSS_CHARACTER_PATH)


def main():
    """按预检、复制、创建、配置和 Boss 追加顺序生成召唤资产。"""
    _validate_prerequisites()
    _ensure_directories()
    with unreal.ScopedSlowTask(6, "Setting up Boss summon minions assets") as task:
        task.make_dialog(True)

        direct_assets = _duplicate_direct_assets()
        _disable_animation_root_motion(direct_assets["animation"])
        task.enter_progress_frame(1, "Copied summon animation and Niagara assets")

        montage = _create_or_load_montage(direct_assets["animation"])
        task.enter_progress_frame(1, "Created summon montage")

        runtime_blueprints = _create_runtime_blueprints()
        task.enter_progress_frame(1, "Created summon gameplay Blueprints")

        _configure_ability(runtime_blueprints, montage)
        task.enter_progress_frame(1, "Configured summon ability")

        _configure_burst_cue(
            runtime_blueprints["cast_cue_class"],
            runtime_blueprints["cast_cue_path"],
            "GameplayCue.Ability.Boss.Summon.Cast",
            direct_assets["cast"],
            True,
            unreal.Vector(1.6, 1.6, 1.6),
        )
        _configure_burst_cue(
            runtime_blueprints["spawn_cue_class"],
            runtime_blueprints["spawn_cue_path"],
            "GameplayCue.Ability.Boss.Summon.Spawn",
            direct_assets["spawn"],
            False,
            unreal.Vector(1.4, 1.4, 1.4),
        )
        task.enter_progress_frame(1, "Configured summon GameplayCues")

        _append_summon_to_boss(runtime_blueprints["ability_class"])
        task.enter_progress_frame(1, "Appended summon ability to Boss")

    unreal.log(
        "Boss summon minions setup completed. The Behavior Tree graph was not "
        "modified; add the SummonMinions branch manually by following "
        "Content/Python/boss/README.md."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss summon minions setup failed: {error}")
        raise
