"""幂等创建 Boss Foundation 资产并在现有普通波次后配置最终 Boss 波。

必须先完成 C++ 编译并重启 Unreal Editor，再通过 Tools > Execute Python Script 执行。
脚本不会加载、保存或切换地图，也不会修改已有普通波的敌人和奖励配置。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_ENEMY_BLUEPRINT = "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter"
SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SOURCE_ANIM_BLUEPRINT = "/Game/Characters/ArenaEnemy/ABP_ArenaEnemy"
SOURCE_SLAM_ANIMATION = "/Game/wukongManny/Q_Slam_MSA"
SOURCE_TELEGRAPH_NIAGARA = "/Game/SlashTrail_SoftTofu/Niagara/Mystic/NS_AuraFX_Mystic"
SOURCE_IMPACT_NIAGARA = "/Game/SlashTrail_SoftTofu/Niagara/Mystic/NS_Hit_Mystic_Once"
LOOPING_CUE_TEMPLATE = "/Game/GAS/GameplayCues/DurationCue/GCN_Shield_Active"
BURST_CUE_TEMPLATE = "/Game/GAS/GameplayCues/InstaneCue/GCN_Hit_Physical"
DAMAGE_EFFECT_PATH = "/Game/GAS/GameplayEffect/GE_Damage"
WAVE_DATA_PATH = "/Game/Blueprints/DataAsset/DA_Waves_Prototype"

BOSS_MESH_PATH = "/Game/Boss/Character/SKM_ArenaBoss"
BOSS_ANIM_BLUEPRINT_PATH = "/Game/Boss/Animation/ABP_ArenaBoss"
BOSS_SLAM_ANIMATION_PATH = "/Game/Boss/Animation/AS_BossGroundSlam"
BOSS_TELEGRAPH_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossGroundSlam_Telegraph"
BOSS_IMPACT_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossGroundSlam_Impact"
BOSS_MONTAGE_PATH = "/Game/Boss/Animation/AM_BossGroundSlam"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
BOSS_ATTRIBUTES_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Init_BossAttributes"
BOSS_COOLDOWN_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Cooldown_BossGroundSlam"
BOSS_ABILITY_PATH = "/Game/Boss/GAS/GameplayAbility/GA_BossGroundSlam"
BOSS_TELEGRAPH_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossGroundSlam_Telegraph"
BOSS_IMPACT_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossGroundSlam_Impact"


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


def _ensure_asset_directories():
    """创建 Boss 统一目录，避免不同编辑器版本不自动建立中间路径。"""
    for directory in (
        "/Game/Boss/Character",
        "/Game/Boss/Animation",
        "/Game/Boss/VFX",
        "/Game/Boss/GAS/GameplayAbility",
        "/Game/Boss/GAS/GameplayEffect",
        "/Game/Boss/GAS/GameplayCue",
    ):
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)


def _validate_existing_asset(asset_path, expected_class):
    """目标资产已存在时只验证类型，不覆盖用户后续做出的正确资产。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        tools.require_asset(asset_path, expected_class)


def _find_boss_wave_indices(waves):
    """查找已有 Boss 波，供预检和幂等更新共享同一识别规则。"""
    return [
        index
        for index, wave in enumerate(waves)
        if bool(_get_editor_property(wave, "boss_wave", "b_boss_wave"))
    ]


def _validate_prerequisites():
    """写入前验证原生类型、源资产、目标类型和最终 Boss 波边界。"""
    required_types = (
        "ArenaBossCharacter",
        "ArenaGameplayAbility_BossGroundSlam",
        "ArenaGameplayEffect_BossAttributes",
        "ArenaGameplayEffect_BossGroundSlamCooldown",
        "ArenaEnemyCharacter",
        "ArenaWaveDataAsset",
        "ArenaWaveConfig",
        "ArenaWaveEnemyEntry",
    )
    for type_name in required_types:
        tools.require_unreal_type(type_name)

    tools.require_blueprint(
        SOURCE_ENEMY_BLUEPRINT,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    tools.require_asset(SOURCE_MESH, unreal.SkeletalMesh)
    tools.require_asset(SOURCE_ANIM_BLUEPRINT, unreal.AnimBlueprint)
    tools.require_asset(SOURCE_SLAM_ANIMATION, unreal.AnimSequence)
    tools.require_asset(SOURCE_TELEGRAPH_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_IMPACT_NIAGARA, unreal.NiagaraSystem)
    tools.require_blueprint(LOOPING_CUE_TEMPLATE, unreal.GameplayCueNotify_Looping)
    tools.require_blueprint(BURST_CUE_TEMPLATE, unreal.GameplayCueNotify_Burst)
    tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)
    tools.make_tag("GameplayCue.Ability.Boss.GroundSlam.Telegraph")
    tools.make_tag("GameplayCue.Ability.Boss.GroundSlam.Impact")

    expected_assets = (
        (BOSS_MESH_PATH, unreal.SkeletalMesh),
        (BOSS_ANIM_BLUEPRINT_PATH, unreal.AnimBlueprint),
        (BOSS_SLAM_ANIMATION_PATH, unreal.AnimSequence),
        (BOSS_TELEGRAPH_NIAGARA_PATH, unreal.NiagaraSystem),
        (BOSS_IMPACT_NIAGARA_PATH, unreal.NiagaraSystem),
        (BOSS_MONTAGE_PATH, unreal.AnimMontage),
    )
    for asset_path, expected_class in expected_assets:
        _validate_existing_asset(asset_path, expected_class)

    existing_blueprints = (
        (BOSS_CHARACTER_PATH, tools.require_unreal_type("ArenaBossCharacter")),
        (BOSS_ATTRIBUTES_PATH, tools.require_unreal_type("ArenaGameplayEffect_BossAttributes")),
        (BOSS_COOLDOWN_PATH, tools.require_unreal_type("ArenaGameplayEffect_BossGroundSlamCooldown")),
        (BOSS_ABILITY_PATH, tools.require_unreal_type("ArenaGameplayAbility_BossGroundSlam")),
        (BOSS_TELEGRAPH_CUE_PATH, unreal.GameplayCueNotify_Looping),
        (BOSS_IMPACT_CUE_PATH, unreal.GameplayCueNotify_Burst),
    )
    for asset_path, parent_class in existing_blueprints:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            tools.require_blueprint(asset_path, parent_class)

    wave_data = tools.require_asset(
        WAVE_DATA_PATH,
        tools.require_unreal_type("ArenaWaveDataAsset"),
    )
    waves = list(wave_data.get_editor_property("waves"))
    wave_count = len(waves)
    if wave_count < 4:
        raise RuntimeError(f"{WAVE_DATA_PATH} must contain the existing four normal waves.")

    boss_wave_indices = _find_boss_wave_indices(waves)
    if len(boss_wave_indices) > 1:
        raise RuntimeError(
            f"{WAVE_DATA_PATH} contains multiple Boss waves at indices "
            f"{boss_wave_indices}; refusing to choose one implicitly."
        )
    if boss_wave_indices and boss_wave_indices[0] != wave_count - 1:
        raise RuntimeError(
            f"{WAVE_DATA_PATH} contains a Boss wave at index {boss_wave_indices[0]} "
            "followed by additional waves; the Boss Foundation requires the Boss wave to be final."
        )


def _duplicate_direct_assets():
    """把 Boss 直接引用的 Mesh、AnimBP、动画和 Niagara 复制到统一目录。"""
    return {
        "mesh": tools.duplicate_or_load_asset(BOSS_MESH_PATH, SOURCE_MESH, unreal.SkeletalMesh),
        "anim_blueprint": tools.duplicate_or_load_asset(
            BOSS_ANIM_BLUEPRINT_PATH,
            SOURCE_ANIM_BLUEPRINT,
            unreal.AnimBlueprint,
        ),
        "slam_animation": tools.duplicate_or_load_asset(
            BOSS_SLAM_ANIMATION_PATH,
            SOURCE_SLAM_ANIMATION,
            unreal.AnimSequence,
        ),
        "telegraph_niagara": tools.duplicate_or_load_asset(
            BOSS_TELEGRAPH_NIAGARA_PATH,
            SOURCE_TELEGRAPH_NIAGARA,
            unreal.NiagaraSystem,
        ),
        "impact_niagara": tools.duplicate_or_load_asset(
            BOSS_IMPACT_NIAGARA_PATH,
            SOURCE_IMPACT_NIAGARA,
            unreal.NiagaraSystem,
        ),
    }


def _ensure_skeleton_compatibility(skeletal_mesh, slam_animation):
    """确保复制后的 Manny Mesh 可以播放 Wukong Slam，差异骨架只注册兼容关系。"""
    mesh_skeleton = _get_editor_property(skeletal_mesh, "skeleton", "Skeleton")
    animation_skeleton = _get_editor_property(slam_animation, "skeleton", "Skeleton")
    if mesh_skeleton == animation_skeleton:
        return

    mesh_skeleton.modify()
    mesh_skeleton.add_compatible_skeleton(animation_skeleton)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh_skeleton, False):
        raise RuntimeError(
            "Failed to save Boss compatible Skeleton entry: "
            f"{mesh_skeleton.get_path_name()} <- {animation_skeleton.get_path_name()}"
        )


def _create_or_load_montage(slam_animation):
    """为复制动画创建 DefaultSlot Montage，并关闭动画资源上的 Root Motion。"""
    if bool(_get_editor_property(slam_animation, "enable_root_motion", "bEnableRootMotion")):
        _set_editor_property(
            slam_animation,
            False,
            "enable_root_motion",
            "bEnableRootMotion",
        )
        unreal.EditorAssetLibrary.save_loaded_asset(slam_animation, False)

    if unreal.EditorAssetLibrary.does_asset_exist(BOSS_MONTAGE_PATH):
        return tools.require_asset(BOSS_MONTAGE_PATH, unreal.AnimMontage)

    montage_factory = unreal.AnimMontageFactory()
    _set_editor_property(
        montage_factory,
        slam_animation,
        "source_animation",
        "SourceAnimation",
    )
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_BossGroundSlam",
        "/Game/Boss/Animation",
        unreal.AnimMontage,
        montage_factory,
    )
    if montage is None:
        raise RuntimeError(f"Failed to create Boss montage: {BOSS_MONTAGE_PATH}")
    tools.save_asset(BOSS_MONTAGE_PATH)
    return montage


def _create_runtime_blueprints():
    """创建 Boss Character、Ability 和两个原生 GameplayEffect 的蓝图子类。"""
    _, boss_attributes_class, boss_attributes_path = tools.create_or_load_blueprint(
        "GE_Init_BossAttributes",
        "/Game/Boss/GAS/GameplayEffect",
        tools.require_unreal_type("ArenaGameplayEffect_BossAttributes"),
    )
    _, boss_cooldown_class, boss_cooldown_path = tools.create_or_load_blueprint(
        "GE_Cooldown_BossGroundSlam",
        "/Game/Boss/GAS/GameplayEffect",
        tools.require_unreal_type("ArenaGameplayEffect_BossGroundSlamCooldown"),
    )
    _, boss_ability_class, boss_ability_path = tools.create_or_load_blueprint(
        "GA_BossGroundSlam",
        "/Game/Boss/GAS/GameplayAbility",
        tools.require_unreal_type("ArenaGameplayAbility_BossGroundSlam"),
    )
    _, boss_character_class, boss_character_path = tools.create_or_load_blueprint(
        "BP_ArenaBossCharacter",
        "/Game/Boss/Character",
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    return {
        "attributes_class": boss_attributes_class,
        "attributes_path": boss_attributes_path,
        "cooldown_class": boss_cooldown_class,
        "cooldown_path": boss_cooldown_path,
        "ability_class": boss_ability_class,
        "ability_path": boss_ability_path,
        "character_class": boss_character_class,
        "character_path": boss_character_path,
    }


def _configure_ability(runtime_blueprints, montage):
    """连接 GroundSlam 的伤害、冷却和动画资产，并写入第一阶段默认值。"""
    _, damage_effect_class = tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)
    defaults = unreal.get_default_object(runtime_blueprints["ability_class"])
    defaults.modify()
    defaults.set_editor_property("damage_effect_class", damage_effect_class)
    defaults.set_editor_property(
        "cooldown_gameplay_effect_class",
        runtime_blueprints["cooldown_class"],
    )
    defaults.set_editor_property("attack_montage", montage)
    defaults.set_editor_property("attack_range", 280.0)
    defaults.set_editor_property("range_tolerance", 25.0)
    defaults.set_editor_property("damage_radius", 300.0)
    defaults.set_editor_property("base_damage", 20.0)
    defaults.set_editor_property("skill_multiplier", 1.0)
    defaults.set_editor_property("montage_play_rate", 1.0)
    defaults.set_editor_property("hit_delay", 1.2)
    defaults.set_editor_property("montage_start_section", unreal.Name("Default"))
    tools.save_asset(runtime_blueprints["ability_path"])


def _copy_enemy_mesh_transform(destination_mesh_component):
    """复用现有敌人 Manny 的相对位置与旋转，并仅放大 Boss 可见 Mesh。"""
    _, source_enemy_class = tools.require_blueprint(
        SOURCE_ENEMY_BLUEPRINT,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    source_defaults = unreal.get_default_object(source_enemy_class)
    source_mesh_component = _get_editor_property(source_defaults, "mesh", "Mesh")
    for property_name in ("relative_location", "relative_rotation"):
        destination_mesh_component.set_editor_property(
            property_name,
            source_mesh_component.get_editor_property(property_name),
        )

    source_scale = source_mesh_component.get_editor_property("relative_scale3d")
    destination_mesh_component.set_editor_property(
        "relative_scale3d",
        unreal.Vector(source_scale.x * 1.35, source_scale.y * 1.35, source_scale.z * 1.35),
    )
    return source_defaults


def _configure_boss_character(runtime_blueprints, direct_assets):
    """连接 Boss 默认属性、唯一主攻击、复制 Manny 表现和现有伤害数字配置。"""
    boss_defaults = unreal.get_default_object(runtime_blueprints["character_class"])
    boss_defaults.modify()
    boss_defaults.set_editor_property("default_attribute_effect", runtime_blueprints["attributes_class"])
    boss_defaults.set_editor_property("startup_abilities", [runtime_blueprints["ability_class"]])
    boss_defaults.set_editor_property("death_life_span", 3.0)
    _set_editor_property(
        boss_defaults,
        False,
        "show_world_health_bar",
        "b_show_world_health_bar",
    )
    # BossDisplayName 继承原生构造函数中的 NSLOCTEXT 默认值；继承 CDO 的文本身份在 Python 中只读。

    mesh_component = _get_editor_property(boss_defaults, "mesh", "Mesh")
    mesh_component.modify()
    source_enemy_defaults = _copy_enemy_mesh_transform(mesh_component)
    _set_editor_property(
        mesh_component,
        direct_assets["mesh"],
        "skeletal_mesh_asset",
        "skeletal_mesh",
    )
    _set_editor_property(
        mesh_component,
        unreal.AnimationMode.ANIMATION_BLUEPRINT,
        "animation_mode",
    )
    _set_editor_property(
        mesh_component,
        tools.load_blueprint_class(BOSS_ANIM_BLUEPRINT_PATH),
        "anim_class",
    )

    # 复用现有本地伤害数字表现；死亡动画保持为后续 Blueprint 演出扩展点。
    boss_defaults.set_editor_property(
        "damage_number_actor_class",
        source_enemy_defaults.get_editor_property("damage_number_actor_class"),
    )
    boss_defaults.set_editor_property(
        "damage_number_spawn_offset",
        source_enemy_defaults.get_editor_property("damage_number_spawn_offset"),
    )
    tools.save_asset(runtime_blueprints["character_path"])


def _configure_telegraph_cue(direct_assets):
    """创建不附着 Boss 的持续预警 Cue，使固定圆心不会跟随移动。"""
    _, cue_class, cue_path = tools.duplicate_or_load_blueprint(
        "GCN_BossGroundSlam_Telegraph",
        "/Game/Boss/GAS/GameplayCue",
        LOOPING_CUE_TEMPLATE,
        unreal.GameplayCueNotify_Looping,
    )
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Ability.Boss.GroundSlam.Telegraph"),
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
            scale_override=unreal.Vector(3.0, 3.0, 1.0),
        ),
    )
    defaults.set_editor_property(
        "looping_effects",
        unreal.GameplayCueNotify_LoopingEffects(
            looping_particles=[
                unreal.GameplayCueNotify_ParticleInfo(
                    niagara_system=direct_assets["telegraph_niagara"],
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ]
        ),
    )
    tools.save_asset(cue_path)


def _configure_impact_cue(direct_assets):
    """创建固定世界位置的一次性 Impact Cue，客户端不参与伤害判定。"""
    _, cue_class, cue_path = tools.duplicate_or_load_blueprint(
        "GCN_BossGroundSlam_Impact",
        "/Game/Boss/GAS/GameplayCue",
        BURST_CUE_TEMPLATE,
        unreal.GameplayCueNotify_Burst,
    )
    defaults = unreal.get_default_object(cue_class)
    defaults.modify()
    defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Ability.Boss.GroundSlam.Impact"),
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
            scale_override=unreal.Vector(3.0, 3.0, 3.0),
        ),
    )
    defaults.set_editor_property(
        "burst_effects",
        unreal.GameplayCueNotify_BurstEffects(
            burst_particles=[
                unreal.GameplayCueNotify_ParticleInfo(
                    niagara_system=direct_assets["impact_niagara"],
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ]
        ),
    )
    tools.save_asset(cue_path)


def _configure_final_boss_wave(boss_character_class):
    """保留全部普通波，并在末尾幂等追加或更新唯一最终 Boss 波。"""
    wave_data = tools.require_asset(
        WAVE_DATA_PATH,
        tools.require_unreal_type("ArenaWaveDataAsset"),
    )
    waves = list(wave_data.get_editor_property("waves"))
    boss_entry = tools.require_unreal_type("ArenaWaveEnemyEntry")()
    boss_entry.set_editor_property("enemy_class", boss_character_class)
    boss_entry.set_editor_property("count", 1)

    boss_wave = tools.require_unreal_type("ArenaWaveConfig")()
    boss_wave.set_editor_property("enemies", [boss_entry])
    boss_wave.set_editor_property("spawn_interval", 0.5)
    _set_editor_property(boss_wave, True, "boss_wave", "b_boss_wave")
    boss_wave.set_editor_property("reward_count", 0)

    boss_wave_indices = _find_boss_wave_indices(waves)
    if not boss_wave_indices:
        waves.append(boss_wave)
    else:
        waves[boss_wave_indices[0]] = boss_wave

    wave_data.modify()
    wave_data.set_editor_property("waves", waves)
    tools.save_asset(WAVE_DATA_PATH)


def main():
    """按预检、复制、创建、连接和波次配置顺序生成 Boss Foundation 资产。"""
    _validate_prerequisites()
    _ensure_asset_directories()
    with unreal.ScopedSlowTask(6, "Setting up Boss Foundation assets") as task:
        task.make_dialog(True)

        direct_assets = _duplicate_direct_assets()
        _ensure_skeleton_compatibility(direct_assets["mesh"], direct_assets["slam_animation"])
        task.enter_progress_frame(1, "Copied Boss direct assets")

        montage = _create_or_load_montage(direct_assets["slam_animation"])
        task.enter_progress_frame(1, "Created GroundSlam montage")

        runtime_blueprints = _create_runtime_blueprints()
        task.enter_progress_frame(1, "Created Boss gameplay Blueprints")

        _configure_ability(runtime_blueprints, montage)
        _configure_boss_character(runtime_blueprints, direct_assets)
        task.enter_progress_frame(1, "Configured Boss character and ability")

        _configure_telegraph_cue(direct_assets)
        _configure_impact_cue(direct_assets)
        task.enter_progress_frame(1, "Configured Boss GameplayCues")

        _configure_final_boss_wave(runtime_blueprints["character_class"])
        task.enter_progress_frame(1, "Configured final Boss wave")

    unreal.log("Boss Foundation setup completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss Foundation setup failed: {error}")
        raise
