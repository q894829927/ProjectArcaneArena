"""幂等创建 Boss Charge 动画、GAS 与 GameplayCue 资产。

必须先完成 C++ 编译并重启 Unreal Editor，再通过 Tools > Execute Python Script 执行。
脚本不会修改 Behavior Tree 图；Charge 分支按本目录 README 手动连接。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_CHARGE_ANIMATION = "/Game/wukongManny/RMB_Evade_CC"
SOURCE_TELEGRAPH_NIAGARA = "/Game/SlashTrail_SoftTofu/Niagara/Mystic/NS_SlashTrail_Mystic_Loop"
SOURCE_ACTIVE_NIAGARA = "/Game/Niagara/NS_DashAura"
SOURCE_IMPACT_NIAGARA = "/Game/SlashTrail_SoftTofu/Niagara/Mystic/NS_Hit_Mystic_Once"
DAMAGE_EFFECT_PATH = "/Game/GAS/GameplayEffect/GE_Damage"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
BOSS_MESH_PATH = "/Game/Boss/Character/SKM_ArenaBoss"

CHARGE_ANIMATION_PATH = "/Game/Boss/Animation/AS_BossCharge"
CHARGE_MONTAGE_PATH = "/Game/Boss/Animation/AM_BossCharge"
CHARGE_TELEGRAPH_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossCharge_Telegraph"
CHARGE_ACTIVE_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossCharge_Active"
CHARGE_IMPACT_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossCharge_Impact"
CHARGE_ABILITY_PATH = "/Game/Boss/GAS/GameplayAbility/GA_BossCharge"
CHARGE_COOLDOWN_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossCharge"
CHARGE_TELEGRAPH_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossCharge_Telegraph"
CHARGE_ACTIVE_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossCharge_Active"
CHARGE_IMPACT_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossCharge_Impact"


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
    """创建 Charge 使用的既有 Boss 子目录。"""
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
    """目标已存在时只验证类型，绝不创建后缀副本或覆盖用户调整。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        tools.require_asset(asset_path, expected_class)


def _validate_prerequisites():
    """任何写入前验证原生 DLL、源资产、Boss 资产和已存在目标类型。"""
    required_types = (
        "ArenaBossCharacter",
        "ArenaGameplayAbility_BossCharge",
        "ArenaGameplayEffect_BossChargeCooldown",
        "ArenaGameplayCueNotify_BossChargeTelegraph",
    )
    for type_name in required_types:
        tools.require_unreal_type(type_name)

    tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    tools.require_asset(BOSS_MESH_PATH, unreal.SkeletalMesh)
    tools.require_asset(SOURCE_CHARGE_ANIMATION, unreal.AnimSequence)
    tools.require_asset(SOURCE_TELEGRAPH_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_ACTIVE_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_IMPACT_NIAGARA, unreal.NiagaraSystem)
    tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)

    for tag_name in (
        "GameplayCue.Ability.Boss.Charge.Telegraph",
        "GameplayCue.Ability.Boss.Charge.Active",
        "GameplayCue.Ability.Boss.Charge.Impact",
    ):
        tools.make_tag(tag_name)

    expected_assets = (
        (CHARGE_ANIMATION_PATH, unreal.AnimSequence),
        (CHARGE_MONTAGE_PATH, unreal.AnimMontage),
        (CHARGE_TELEGRAPH_NIAGARA_PATH, unreal.NiagaraSystem),
        (CHARGE_ACTIVE_NIAGARA_PATH, unreal.NiagaraSystem),
        (CHARGE_IMPACT_NIAGARA_PATH, unreal.NiagaraSystem),
    )
    for asset_path, expected_class in expected_assets:
        _validate_existing_asset(asset_path, expected_class)

    existing_blueprints = (
        (CHARGE_ABILITY_PATH, tools.require_unreal_type("ArenaGameplayAbility_BossCharge")),
        (CHARGE_COOLDOWN_PATH, tools.require_unreal_type("ArenaGameplayEffect_BossChargeCooldown")),
        (
            CHARGE_TELEGRAPH_CUE_PATH,
            tools.require_unreal_type("ArenaGameplayCueNotify_BossChargeTelegraph"),
        ),
        (CHARGE_ACTIVE_CUE_PATH, unreal.GameplayCueNotify_Looping),
        (CHARGE_IMPACT_CUE_PATH, unreal.GameplayCueNotify_Burst),
    )
    for asset_path, parent_class in existing_blueprints:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)


def _duplicate_direct_assets():
    """复制 Charge 直接引用的动画与三种 Niagara 到 Boss 专属目录。"""
    return {
        "animation": tools.duplicate_or_load_asset(
            CHARGE_ANIMATION_PATH,
            SOURCE_CHARGE_ANIMATION,
            unreal.AnimSequence,
        ),
        "telegraph": tools.duplicate_or_load_asset(
            CHARGE_TELEGRAPH_NIAGARA_PATH,
            SOURCE_TELEGRAPH_NIAGARA,
            unreal.NiagaraSystem,
        ),
        "active": tools.duplicate_or_load_asset(
            CHARGE_ACTIVE_NIAGARA_PATH,
            SOURCE_ACTIVE_NIAGARA,
            unreal.NiagaraSystem,
        ),
        "impact": tools.duplicate_or_load_asset(
            CHARGE_IMPACT_NIAGARA_PATH,
            SOURCE_IMPACT_NIAGARA,
            unreal.NiagaraSystem,
        ),
    }


def _ensure_animation_compatibility(charge_animation):
    """注册 Boss Manny Skeleton 对闪避动画 Skeleton 的兼容关系，并关闭动画 Root Motion。"""
    boss_mesh = tools.require_asset(BOSS_MESH_PATH, unreal.SkeletalMesh)
    boss_skeleton = _get_editor_property(boss_mesh, "skeleton", "Skeleton")
    animation_skeleton = _get_editor_property(charge_animation, "skeleton", "Skeleton")
    if boss_skeleton != animation_skeleton:
        boss_skeleton.modify()
        boss_skeleton.add_compatible_skeleton(animation_skeleton)
        if not unreal.EditorAssetLibrary.save_loaded_asset(boss_skeleton, False):
            raise RuntimeError(
                "Failed to save Boss compatible Skeleton entry: "
                f"{boss_skeleton.get_path_name()} <- {animation_skeleton.get_path_name()}"
            )

    if bool(_get_editor_property(charge_animation, "enable_root_motion", "bEnableRootMotion")):
        _set_editor_property(
            charge_animation,
            False,
            "enable_root_motion",
            "bEnableRootMotion",
        )
        unreal.EditorAssetLibrary.save_loaded_asset(charge_animation, False)


def _create_or_load_montage(charge_animation):
    """从 Boss 专属闪避动画创建 DefaultSlot Montage，重复执行时复用原资产。"""
    if unreal.EditorAssetLibrary.does_asset_exist(CHARGE_MONTAGE_PATH):
        return tools.require_asset(CHARGE_MONTAGE_PATH, unreal.AnimMontage)

    montage_factory = unreal.AnimMontageFactory()
    _set_editor_property(
        montage_factory,
        charge_animation,
        "source_animation",
        "SourceAnimation",
    )
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_BossCharge",
        "/Game/Boss/Animation",
        unreal.AnimMontage,
        montage_factory,
    )
    if montage is None:
        raise RuntimeError(f"Failed to create Charge montage: {CHARGE_MONTAGE_PATH}")
    tools.save_asset(CHARGE_MONTAGE_PATH)
    return montage


def _create_runtime_blueprints():
    """创建 Charge Ability、Cooldown 和原生 Telegraph Cue 的蓝图子类。"""
    _, cooldown_class, cooldown_path = tools.create_or_load_blueprint(
        "GE_Cooldown_BossCharge",
        "/Game/Boss/GAS/GameplayEffect",
        tools.require_unreal_type("ArenaGameplayEffect_BossChargeCooldown"),
    )
    _, ability_class, ability_path = tools.create_or_load_blueprint(
        "GA_BossCharge",
        "/Game/Boss/GAS/GameplayAbility",
        tools.require_unreal_type("ArenaGameplayAbility_BossCharge"),
    )
    _, telegraph_cue_class, telegraph_cue_path = tools.create_or_load_blueprint(
        "GCN_BossCharge_Telegraph",
        "/Game/Boss/GAS/GameplayCue",
        tools.require_unreal_type("ArenaGameplayCueNotify_BossChargeTelegraph"),
    )
    # 原生冷却蓝图没有额外字段，但仍需显式保存新建 Package，避免只因 GA 引用而留在 Dirty 状态。
    tools.save_asset(cooldown_path)
    return {
        "cooldown_class": cooldown_class,
        "cooldown_path": cooldown_path,
        "ability_class": ability_class,
        "ability_path": ability_path,
        "telegraph_cue_class": telegraph_cue_class,
        "telegraph_cue_path": telegraph_cue_path,
    }


def _configure_ability(runtime_blueprints, montage):
    """连接 Charge 的伤害、冷却、Montage 和第一版服务器数值。"""
    _, damage_effect_class = tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)
    defaults = unreal.get_default_object(runtime_blueprints["ability_class"])
    defaults.modify()
    defaults.set_editor_property("damage_effect_class", damage_effect_class)
    defaults.set_editor_property(
        "cooldown_gameplay_effect_class",
        runtime_blueprints["cooldown_class"],
    )
    defaults.set_editor_property("minimum_attack_range", 350.0)
    defaults.set_editor_property("attack_range", 900.0)
    defaults.set_editor_property("range_tolerance", 25.0)
    defaults.set_editor_property("target_overshoot_distance", 150.0)
    defaults.set_editor_property("maximum_charge_distance", 1050.0)
    defaults.set_editor_property("telegraph_duration", 0.8)
    defaults.set_editor_property("charge_speed", 1200.0)
    defaults.set_editor_property("hit_radius", 110.0)
    defaults.set_editor_property("base_damage", 25.0)
    defaults.set_editor_property("skill_multiplier", 1.0)
    defaults.set_editor_property("charge_montage", montage)
    defaults.set_editor_property("charge_montage_play_rate", 2.0)
    defaults.set_editor_property("montage_start_section", unreal.Name("Default"))
    tools.save_asset(runtime_blueprints["ability_path"])


def _configure_telegraph_cue(runtime_blueprints, telegraph_niagara):
    """配置原生世界空间 Telegraph Cue 的标签、Niagara、长度基准和可读尺寸。"""
    defaults = unreal.get_default_object(runtime_blueprints["telegraph_cue_class"])
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Ability.Boss.Charge.Telegraph"),
    )
    defaults.set_editor_property("telegraph_system", telegraph_niagara)
    defaults.set_editor_property("reference_length", 100.0)
    defaults.set_editor_property("vertical_offset", 12.0)
    defaults.set_editor_property("width_scale", 3.0)
    defaults.set_editor_property("height_scale", 2.0)
    defaults.set_editor_property("auto_destroy_on_remove", True)
    tools.save_asset(runtime_blueprints["telegraph_cue_path"])


def _configure_active_cue(active_niagara):
    """直接从原生 Looping 类创建 Active Cue，避免先以模板旧 Tag 注册到 CueManager。"""
    _, cue_class, cue_path = tools.create_or_load_blueprint(
        "GCN_BossCharge_Active",
        "/Game/Boss/GAS/GameplayCue",
        unreal.GameplayCueNotify_Looping,
    )
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Ability.Boss.Charge.Active"),
    )
    defaults.set_editor_property(
        "default_placement_info",
        unreal.GameplayCueNotify_PlacementInfo(
            socket_name=unreal.Name("None"),
            attach_policy=unreal.GameplayCueNotify_AttachPolicy.ATTACH_TO_TARGET,
            attachment_rule=unreal.AttachmentRule.KEEP_RELATIVE,
            override_rotation=False,
            override_scale=True,
            rotation_override=unreal.Rotator(0.0, 0.0, 0.0),
            scale_override=unreal.Vector(1.5, 1.5, 1.5),
        ),
    )
    defaults.set_editor_property(
        "looping_effects",
        unreal.GameplayCueNotify_LoopingEffects(
            looping_particles=[
                unreal.GameplayCueNotify_ParticleInfo(
                    niagara_system=active_niagara,
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ]
        ),
    )
    tools.save_asset(cue_path)


def _configure_impact_cue(impact_niagara):
    """直接从原生 Burst 类创建 Impact Cue，避免先以 Physical 模板 Tag 注册。"""
    _, cue_class, cue_path = tools.create_or_load_blueprint(
        "GCN_BossCharge_Impact",
        "/Game/Boss/GAS/GameplayCue",
        unreal.GameplayCueNotify_Burst,
    )
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Ability.Boss.Charge.Impact"),
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
                    niagara_system=impact_niagara,
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ]
        ),
    )
    tools.save_asset(cue_path)


def _append_charge_to_boss(ability_class):
    """保留 Boss 现有技能顺序，仅在缺失时追加一份 Charge Ability。"""
    _, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    startup_abilities = list(boss_defaults.get_editor_property("startup_abilities"))
    charge_class_path = ability_class.get_path_name()
    matching_indices = [
        index
        for index, startup_class in enumerate(startup_abilities)
        if startup_class and startup_class.get_path_name() == charge_class_path
    ]
    if not matching_indices:
        startup_abilities.append(ability_class)
    elif len(matching_indices) > 1:
        first_index = matching_indices[0]
        startup_abilities = [
            startup_class
            for index, startup_class in enumerate(startup_abilities)
            if startup_class.get_path_name() != charge_class_path or index == first_index
        ]

    boss_defaults.modify()
    boss_defaults.set_editor_property("startup_abilities", startup_abilities)
    tools.save_asset(BOSS_CHARACTER_PATH)


def main():
    """按预检、复制、创建、连接和 Boss 追加顺序生成 Charge 资产。"""
    _validate_prerequisites()
    _ensure_directories()
    with unreal.ScopedSlowTask(6, "Setting up Boss Charge assets") as task:
        task.make_dialog(True)

        direct_assets = _duplicate_direct_assets()
        _ensure_animation_compatibility(direct_assets["animation"])
        task.enter_progress_frame(1, "Copied Charge animation and Niagara assets")

        montage = _create_or_load_montage(direct_assets["animation"])
        task.enter_progress_frame(1, "Created Charge montage")

        runtime_blueprints = _create_runtime_blueprints()
        task.enter_progress_frame(1, "Created Charge gameplay Blueprints")

        _configure_ability(runtime_blueprints, montage)
        task.enter_progress_frame(1, "Configured Charge ability")

        _configure_telegraph_cue(runtime_blueprints, direct_assets["telegraph"])
        _configure_active_cue(direct_assets["active"])
        _configure_impact_cue(direct_assets["impact"])
        task.enter_progress_frame(1, "Configured Charge GameplayCues")

        _append_charge_to_boss(runtime_blueprints["ability_class"])
        task.enter_progress_frame(1, "Appended Charge to Boss StartupAbilities")

    unreal.log(
        "Boss Charge setup completed. The Behavior Tree graph was not modified; "
        "add the Charge branch manually by following Content/Python/boss/README.md."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss Charge setup failed: {error}")
        raise
