"""幂等创建 Boss FireZone 动画、GAS、Area 与 GameplayCue 资产。

必须先完成 C++ 编译并重启 Unreal Editor，再通过 Tools > Execute Python Script 执行。
脚本不会修改 Behavior Tree 图；FireZone 分支按本目录 README 手动连接。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_CAST_ANIMATION = "/Game/CombatMagicAnims/Animations/AS_SpellAndCastFireball"
SOURCE_TELEGRAPH_NIAGARA = "/Game/Boss/VFX/NS_BossGroundSlam_Telegraph"
SOURCE_ACTIVE_NIAGARA = "/Game/SlashTrail_SoftTofu/Niagara/Fire/NS_AuraFX_Fire"
SOURCE_BOUNDARY_MESH = "/Game/ParagonMuriel/FX/Meshes/Hero_Specific/SM_Knock_Up_Runes_Ring"
DAMAGE_EFFECT_PATH = "/Game/GAS/GameplayEffect/GE_Damage"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
BOSS_MESH_PATH = "/Game/Boss/Character/SKM_ArenaBoss"

FIRE_ZONE_ANIMATION_PATH = "/Game/Boss/Animation/AS_BossFireZone"
FIRE_ZONE_MONTAGE_PATH = "/Game/Boss/Animation/AM_BossFireZone"
FIRE_ZONE_TELEGRAPH_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossFireZone_Telegraph"
FIRE_ZONE_ACTIVE_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossFireZone_Active"
FIRE_ZONE_ABILITY_PATH = "/Game/Boss/GAS/GameplayAbility/GA_BossFireZone"
FIRE_ZONE_COOLDOWN_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossFireZone"
FIRE_ZONE_AREA_PATH = "/Game/Boss/GAS/Area/BP_ArenaBossFireZoneArea"
FIRE_ZONE_TELEGRAPH_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossFireZone_Telegraph"
FIRE_ZONE_ACTIVE_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossFireZone_Active"


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
    """创建 FireZone 使用的 Boss 子目录。"""
    for directory in (
        "/Game/Boss/Animation",
        "/Game/Boss/VFX",
        "/Game/Boss/GAS/GameplayAbility",
        "/Game/Boss/GAS/GameplayEffect",
        "/Game/Boss/GAS/GameplayCue",
        "/Game/Boss/GAS/Area",
    ):
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)


def _validate_existing_asset(asset_path, expected_class):
    """目标已存在时只验证类型，避免生成后缀资产或覆盖用户调整。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        tools.require_asset(asset_path, expected_class)


def _validate_prerequisites():
    """在任何写入前验证最新 DLL、源资产、Boss 资产和既有目标类型。"""
    required_types = (
        "ArenaGameplayAbility_BossFireZone",
        "ArenaGameplayEffect_BossFireZoneCooldown",
        "ArenaBossFireZoneArea",
        "ArenaGameplayCueNotify_BossFireZoneRadius",
        "ArenaBossCharacter",
    )
    for type_name in required_types:
        tools.require_unreal_type(type_name)

    tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    tools.require_asset(BOSS_MESH_PATH, unreal.SkeletalMesh)
    tools.require_asset(SOURCE_CAST_ANIMATION, unreal.AnimSequence)
    tools.require_asset(SOURCE_TELEGRAPH_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_ACTIVE_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_BOUNDARY_MESH, unreal.StaticMesh)
    tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)

    for tag_name in (
        "GameplayCue.Ability.Boss.FireZone.Telegraph",
        "GameplayCue.Ability.Boss.FireZone.Active",
    ):
        tools.make_tag(tag_name)

    expected_assets = (
        (FIRE_ZONE_ANIMATION_PATH, unreal.AnimSequence),
        (FIRE_ZONE_MONTAGE_PATH, unreal.AnimMontage),
        (FIRE_ZONE_TELEGRAPH_NIAGARA_PATH, unreal.NiagaraSystem),
        (FIRE_ZONE_ACTIVE_NIAGARA_PATH, unreal.NiagaraSystem),
    )
    for asset_path, expected_class in expected_assets:
        _validate_existing_asset(asset_path, expected_class)

    existing_blueprints = (
        (
            FIRE_ZONE_ABILITY_PATH,
            tools.require_unreal_type("ArenaGameplayAbility_BossFireZone"),
        ),
        (
            FIRE_ZONE_COOLDOWN_PATH,
            tools.require_unreal_type("ArenaGameplayEffect_BossFireZoneCooldown"),
        ),
        (FIRE_ZONE_AREA_PATH, tools.require_unreal_type("ArenaBossFireZoneArea")),
        (
            FIRE_ZONE_TELEGRAPH_CUE_PATH,
            tools.require_unreal_type("ArenaGameplayCueNotify_BossFireZoneRadius"),
        ),
        (
            FIRE_ZONE_ACTIVE_CUE_PATH,
            tools.require_unreal_type("ArenaGameplayCueNotify_BossFireZoneRadius"),
        ),
    )
    for asset_path, parent_class in existing_blueprints:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)


def _duplicate_direct_assets():
    """复制 FireZone 直接引用的动画、圆形预警和火焰持续 Niagara。"""
    direct_assets = {
        "animation": tools.duplicate_or_load_asset(
            FIRE_ZONE_ANIMATION_PATH,
            SOURCE_CAST_ANIMATION,
            unreal.AnimSequence,
        ),
        "telegraph": tools.duplicate_or_load_asset(
            FIRE_ZONE_TELEGRAPH_NIAGARA_PATH,
            SOURCE_TELEGRAPH_NIAGARA,
            unreal.NiagaraSystem,
        ),
        "active": tools.duplicate_or_load_asset(
            FIRE_ZONE_ACTIVE_NIAGARA_PATH,
            SOURCE_ACTIVE_NIAGARA,
            unreal.NiagaraSystem,
        ),
    }
    # EditorAssetLibrary.duplicate_asset 只创建并标脏 Package；立即保存可避免关闭编辑器时遗漏直接资源。
    for asset_path in (
        FIRE_ZONE_ANIMATION_PATH,
        FIRE_ZONE_TELEGRAPH_NIAGARA_PATH,
        FIRE_ZONE_ACTIVE_NIAGARA_PATH,
    ):
        tools.save_asset(asset_path)
    return direct_assets


def _ensure_animation_compatibility(cast_animation):
    """注册 Boss Skeleton 对施法动画骨架的兼容关系，并关闭动画 Root Motion。"""
    boss_mesh = tools.require_asset(BOSS_MESH_PATH, unreal.SkeletalMesh)
    boss_skeleton = _get_editor_property(boss_mesh, "skeleton", "Skeleton")
    animation_skeleton = _get_editor_property(cast_animation, "skeleton", "Skeleton")
    if boss_skeleton != animation_skeleton:
        boss_skeleton.modify()
        boss_skeleton.add_compatible_skeleton(animation_skeleton)
        if not unreal.EditorAssetLibrary.save_loaded_asset(boss_skeleton, False):
            raise RuntimeError(
                "Failed to save Boss compatible Skeleton entry: "
                f"{boss_skeleton.get_path_name()} <- {animation_skeleton.get_path_name()}"
            )

    if bool(_get_editor_property(cast_animation, "enable_root_motion", "bEnableRootMotion")):
        _set_editor_property(
            cast_animation,
            False,
            "enable_root_motion",
            "bEnableRootMotion",
        )
        unreal.EditorAssetLibrary.save_loaded_asset(cast_animation, False)


def _create_or_load_montage(cast_animation):
    """从 Boss 专属施法动画创建 DefaultSlot Montage，重复执行时复用原资产。"""
    if unreal.EditorAssetLibrary.does_asset_exist(FIRE_ZONE_MONTAGE_PATH):
        return tools.require_asset(FIRE_ZONE_MONTAGE_PATH, unreal.AnimMontage)

    montage_factory = unreal.AnimMontageFactory()
    _set_editor_property(
        montage_factory,
        cast_animation,
        "source_animation",
        "SourceAnimation",
    )
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_BossFireZone",
        "/Game/Boss/Animation",
        unreal.AnimMontage,
        montage_factory,
    )
    if montage is None:
        raise RuntimeError(f"Failed to create FireZone montage: {FIRE_ZONE_MONTAGE_PATH}")
    tools.save_asset(FIRE_ZONE_MONTAGE_PATH)
    return montage


def _create_runtime_blueprints():
    """创建或复用 FireZone Ability、Cooldown、Area 和两个原生半径 Cue 子类。"""
    _, cooldown_class, cooldown_path = tools.create_or_load_blueprint(
        "GE_Cooldown_BossFireZone",
        "/Game/Boss/GAS/GameplayEffect",
        tools.require_unreal_type("ArenaGameplayEffect_BossFireZoneCooldown"),
    )
    _, area_class, area_path = tools.create_or_load_blueprint(
        "BP_ArenaBossFireZoneArea",
        "/Game/Boss/GAS/Area",
        tools.require_unreal_type("ArenaBossFireZoneArea"),
    )
    _, ability_class, ability_path = tools.create_or_load_blueprint(
        "GA_BossFireZone",
        "/Game/Boss/GAS/GameplayAbility",
        tools.require_unreal_type("ArenaGameplayAbility_BossFireZone"),
    )
    _, telegraph_cue_class, telegraph_cue_path = tools.create_or_load_blueprint(
        "GCN_BossFireZone_Telegraph",
        "/Game/Boss/GAS/GameplayCue",
        tools.require_unreal_type("ArenaGameplayCueNotify_BossFireZoneRadius"),
    )
    _, active_cue_class, active_cue_path = tools.create_or_load_blueprint(
        "GCN_BossFireZone_Active",
        "/Game/Boss/GAS/GameplayCue",
        tools.require_unreal_type("ArenaGameplayCueNotify_BossFireZoneRadius"),
    )
    tools.save_asset(cooldown_path)
    tools.save_asset(area_path)
    return {
        "cooldown_class": cooldown_class,
        "ability_class": ability_class,
        "ability_path": ability_path,
        "area_class": area_class,
        "telegraph_cue_class": telegraph_cue_class,
        "telegraph_cue_path": telegraph_cue_path,
        "active_cue_class": active_cue_class,
        "active_cue_path": active_cue_path,
    }


def _configure_ability(runtime_blueprints, montage):
    """连接 FireZone 的伤害、冷却、Area、Montage 和第一版服务器数值。"""
    _, damage_effect_class = tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)
    defaults = unreal.get_default_object(runtime_blueprints["ability_class"])
    defaults.modify()
    defaults.set_editor_property("damage_effect_class", damage_effect_class)
    defaults.set_editor_property("fire_zone_area_class", runtime_blueprints["area_class"])
    defaults.set_editor_property(
        "cooldown_gameplay_effect_class",
        runtime_blueprints["cooldown_class"],
    )
    defaults.set_editor_property("attack_montage", montage)
    defaults.set_editor_property("minimum_attack_range", 600.0)
    defaults.set_editor_property("attack_range", 1200.0)
    defaults.set_editor_property("range_tolerance", 25.0)
    defaults.set_editor_property("telegraph_duration", 1.0)
    defaults.set_editor_property("zone_radius", 300.0)
    defaults.set_editor_property("damage_half_height", 180.0)
    defaults.set_editor_property("zone_duration", 5.0)
    defaults.set_editor_property("damage_tick_interval", 0.5)
    defaults.set_editor_property("base_damage", 5.0)
    defaults.set_editor_property("skill_multiplier", 1.0)
    defaults.set_editor_property("ground_trace_start_height", 80.0)
    defaults.set_editor_property("ground_trace_distance", 1200.0)
    defaults.set_editor_property("montage_play_rate", 1.25)
    defaults.set_editor_property("montage_start_section", unreal.Name("Default"))
    tools.save_asset(runtime_blueprints["ability_path"])


def _configure_radius_cue(
    cue_class,
    cue_path,
    tag_name,
    niagara_system,
    height_scale,
    boundary_system=None,
    boundary_mesh=None,
):
    """配置固定世界位置、按 RawMagnitude 缩放的主体与持续伤害边界。"""
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property("gameplay_cue_tag", tools.make_tag(tag_name))
    defaults.set_editor_property("zone_system", niagara_system)
    defaults.set_editor_property("boundary_system", boundary_system)
    defaults.set_editor_property("boundary_mesh", boundary_mesh)
    defaults.set_editor_property("reference_radius", 100.0)
    defaults.set_editor_property("vertical_offset", 10.0)
    defaults.set_editor_property("boundary_vertical_offset", 2.0)
    defaults.set_editor_property("boundary_mesh_thickness_scale", 0.1)
    defaults.set_editor_property("height_scale", height_scale)
    tools.save_asset(cue_path)


def _append_fire_zone_to_boss(ability_class):
    """保留 Boss 现有技能顺序，仅在缺失时追加一份 FireZone Ability。"""
    _, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    startup_abilities = list(boss_defaults.get_editor_property("startup_abilities"))
    fire_zone_class_path = ability_class.get_path_name()
    matching_indices = [
        index
        for index, startup_class in enumerate(startup_abilities)
        if startup_class and startup_class.get_path_name() == fire_zone_class_path
    ]
    if not matching_indices:
        startup_abilities.append(ability_class)
    elif len(matching_indices) > 1:
        first_index = matching_indices[0]
        startup_abilities = [
            startup_class
            for index, startup_class in enumerate(startup_abilities)
            if startup_class.get_path_name() != fire_zone_class_path or index == first_index
        ]

    boss_defaults.modify()
    boss_defaults.set_editor_property("startup_abilities", startup_abilities)
    tools.save_asset(BOSS_CHARACTER_PATH)


def main():
    """按预检、复制、创建、连接和 Boss 追加顺序生成 FireZone 资产。"""
    _validate_prerequisites()
    _ensure_directories()
    with unreal.ScopedSlowTask(6, "Setting up Boss FireZone assets") as task:
        task.make_dialog(True)

        direct_assets = _duplicate_direct_assets()
        _ensure_animation_compatibility(direct_assets["animation"])
        task.enter_progress_frame(1, "Copied FireZone animation and Niagara assets")

        montage = _create_or_load_montage(direct_assets["animation"])
        task.enter_progress_frame(1, "Created FireZone montage")

        runtime_blueprints = _create_runtime_blueprints()
        task.enter_progress_frame(1, "Created FireZone gameplay Blueprints")

        _configure_ability(runtime_blueprints, montage)
        task.enter_progress_frame(1, "Configured FireZone ability")

        _configure_radius_cue(
            runtime_blueprints["telegraph_cue_class"],
            runtime_blueprints["telegraph_cue_path"],
            "GameplayCue.Ability.Boss.FireZone.Telegraph",
            direct_assets["telegraph"],
            1.0,
        )
        _configure_radius_cue(
            runtime_blueprints["active_cue_class"],
            runtime_blueprints["active_cue_path"],
            "GameplayCue.Ability.Boss.FireZone.Active",
            direct_assets["active"],
            1.5,
            direct_assets["telegraph"],
            tools.require_asset(SOURCE_BOUNDARY_MESH, unreal.StaticMesh),
        )
        task.enter_progress_frame(1, "Configured FireZone GameplayCues")

        _append_fire_zone_to_boss(runtime_blueprints["ability_class"])
        task.enter_progress_frame(1, "Appended FireZone to Boss StartupAbilities")

    unreal.log(
        "Boss FireZone setup completed. The Behavior Tree graph was not modified; "
        "add the FireZone branch manually by following Content/Python/boss/README.md."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss FireZone setup failed: {error}")
        raise
