"""创建远程敌人战斗资产并写入四波混合配置。

在完成 C++ 编译并重启 Unreal Editor 后，通过 Tools > Execute Python Script 执行。
脚本不会创建、加载、保存或切换任何地图。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_ANIMATION_PATH = "/Game/CombatMagicAnims/Animations/AS_ManaCastShot"
REFERENCE_MONTAGE_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/Attack/AM_EnemyMeleeAttack"
RANGED_MONTAGE_PATH = "/Game/CombatMagicAnims/Animations/AM_EnemyRangedAttack"
RANGED_MONTAGE_NAME = "AM_EnemyRangedAttack"
RANGED_MONTAGE_FOLDER = "/Game/CombatMagicAnims/Animations"

DAMAGE_EFFECT_PATH = "/Game/GAS/GameplayEffect/GE_Damage"
RANGED_COOLDOWN_PATH = "/Game/GAS/GameplayEffect/GE_Cooldown_EnemyRangedAttack"
RANGED_ABILITY_PATH = "/Game/GAS/GameplayAbility/GA_EnemyRangedAttack"
RANGED_PROJECTILE_PATH = "/Game/GAS/Projectile/BP_ArenaEnemyProjectile"
MELEE_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter"
RANGED_ENEMY_PATH = "/Game/Characters/ArenaEnemy/BP_ArenaRangedEnemy"
PROJECTILE_FX_PATH = "/Game/ParagonMuriel/FX/Particles/Abilities/Primary/FX/P_Muriel_Primary_Projectile"
RANGED_MATERIAL_PATH = "/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New"
WAVE_DATA_PATH = "/Game/Blueprints/DataAsset/DA_Waves_Prototype"

WAVE_MIXES = (
    ((MELEE_ENEMY_PATH, 3),),
    ((MELEE_ENEMY_PATH, 3), (RANGED_ENEMY_PATH, 2)),
    ((MELEE_ENEMY_PATH, 4), (RANGED_ENEMY_PATH, 3)),
    ((MELEE_ENEMY_PATH, 5), (RANGED_ENEMY_PATH, 4)),
)


def _get_editor_property(obj, *property_names):
    """兼容 Python 与 C++ 风格属性名，并返回第一个可读取属性。"""
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
    """兼容 Python 与 C++ 风格属性名，并写入第一个可编辑属性。"""
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


def _ensure_skeleton_compatibility(source_animation, reference_montage):
    """让当前敌人 Skeleton 可安全播放同骨架层级的外部施法动画。"""
    source_skeleton = _get_editor_property(source_animation, "skeleton", "Skeleton")
    reference_skeleton = _get_editor_property(reference_montage, "skeleton", "Skeleton")
    if source_skeleton == reference_skeleton:
        return

    reference_skeleton.modify()
    reference_skeleton.add_compatible_skeleton(source_skeleton)
    if not unreal.EditorAssetLibrary.save_loaded_asset(reference_skeleton, False):
        raise RuntimeError(
            "Failed to save the enemy Compatible Skeleton entry: "
            f"{reference_skeleton.get_path_name()}"
        )

    unreal.log(
        "Registered ranged animation skeleton compatibility: "
        f"{reference_skeleton.get_path_name()} <- {source_skeleton.get_path_name()}"
    )


def _validate_prerequisites():
    """检查反射类型与源资产，并配置敌人播放外部动画所需的骨架兼容。"""
    required_type_names = (
        "ArenaGameplayAbility_EnemyRangedAttack",
        "ArenaGameplayEffect_EnemyRangedCooldown",
        "ArenaEnemyProjectile",
        "ArenaEnemyCharacter",
        "ArenaWaveDataAsset",
        "ArenaWaveConfig",
        "ArenaWaveEnemyEntry",
    )
    for type_name in required_type_names:
        tools.require_unreal_type(type_name)

    source_animation = tools.require_asset(SOURCE_ANIMATION_PATH, unreal.AnimSequence)
    reference_montage = tools.require_asset(REFERENCE_MONTAGE_PATH, unreal.AnimMontage)
    _ensure_skeleton_compatibility(source_animation, reference_montage)

    tools.require_blueprint(DAMAGE_EFFECT_PATH, unreal.GameplayEffect)
    tools.require_blueprint(MELEE_ENEMY_PATH, tools.require_unreal_type("ArenaEnemyCharacter"))
    tools.require_asset(PROJECTILE_FX_PATH, unreal.ParticleSystem)
    tools.require_asset(RANGED_MATERIAL_PATH, unreal.MaterialInterface)

    wave_data = tools.require_asset(
        WAVE_DATA_PATH,
        tools.require_unreal_type("ArenaWaveDataAsset"),
    )
    if len(list(wave_data.get_editor_property("waves"))) < 3:
        raise RuntimeError(f"{WAVE_DATA_PATH} must keep its first three prototype waves.")
    return source_animation


def _create_or_load_montage(source_animation):
    """复用或创建 ManaCastShot 的 DefaultSlot Montage，并避免缺失资产产生误报。"""
    root_motion_enabled = bool(
        _get_editor_property(source_animation, "enable_root_motion", "bEnableRootMotion")
    )
    if root_motion_enabled:
        _set_editor_property(
            source_animation,
            False,
            "enable_root_motion",
            "bEnableRootMotion",
        )
        unreal.EditorAssetLibrary.save_loaded_asset(source_animation, False)

    if unreal.EditorAssetLibrary.does_asset_exist(RANGED_MONTAGE_PATH):
        existing_montage = unreal.EditorAssetLibrary.load_asset(RANGED_MONTAGE_PATH)
        if existing_montage is None:
            raise RuntimeError(f"Failed to load existing montage: {RANGED_MONTAGE_PATH}")
        if not isinstance(existing_montage, unreal.AnimMontage):
            raise RuntimeError(f"{RANGED_MONTAGE_PATH} exists but is not an AnimMontage.")
        return existing_montage

    factory = unreal.AnimMontageFactory()
    _set_editor_property(factory, source_animation, "source_animation", "SourceAnimation")
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        RANGED_MONTAGE_NAME,
        RANGED_MONTAGE_FOLDER,
        unreal.AnimMontage,
        factory,
    )
    if montage is None:
        raise RuntimeError(f"Failed to create montage: {RANGED_MONTAGE_PATH}")
    unreal.EditorAssetLibrary.save_loaded_asset(montage, False)
    return montage


def _create_runtime_blueprints():
    """幂等创建远程冷却、Ability、Projectile 与敌人 Blueprint。"""
    cooldown_parent = tools.require_unreal_type("ArenaGameplayEffect_EnemyRangedCooldown")
    _, cooldown_class, cooldown_path = tools.create_or_load_blueprint(
        "GE_Cooldown_EnemyRangedAttack",
        "/Game/GAS/GameplayEffect",
        cooldown_parent,
    )
    tools.save_asset(cooldown_path)

    projectile_parent = tools.require_unreal_type("ArenaEnemyProjectile")
    _, projectile_class, projectile_path = tools.create_or_load_blueprint(
        "BP_ArenaEnemyProjectile",
        "/Game/GAS/Projectile",
        projectile_parent,
    )

    ability_parent = tools.require_unreal_type("ArenaGameplayAbility_EnemyRangedAttack")
    _, ability_class, ability_path = tools.create_or_load_blueprint(
        "GA_EnemyRangedAttack",
        "/Game/GAS/GameplayAbility",
        ability_parent,
    )

    enemy_parent = tools.require_unreal_type("ArenaEnemyCharacter")
    _, enemy_class, enemy_path = tools.duplicate_or_load_blueprint(
        "BP_ArenaRangedEnemy",
        "/Game/Characters/ArenaEnemy",
        MELEE_ENEMY_PATH,
        enemy_parent,
    )
    return {
        "cooldown_class": cooldown_class,
        "cooldown_path": cooldown_path,
        "projectile_class": projectile_class,
        "projectile_path": projectile_path,
        "ability_class": ability_class,
        "ability_path": ability_path,
        "enemy_class": enemy_class,
        "enemy_path": enemy_path,
    }


def _configure_projectile(projectile_class, projectile_path):
    """写入 Muriel 飞行粒子与首版速度、寿命和碰撞半径。"""
    projectile_defaults = unreal.get_default_object(projectile_class)
    projectile_defaults.modify()
    projectile_defaults.set_editor_property(
        "projectile_effect",
        tools.require_asset(PROJECTILE_FX_PATH, unreal.ParticleSystem),
    )
    projectile_defaults.set_editor_property("initial_speed", 900.0)
    projectile_defaults.set_editor_property("projectile_life_span", 3.0)
    projectile_defaults.set_editor_property("sphere_radius", 16.0)
    tools.save_asset(projectile_path)


def _configure_ability(asset_classes, montage):
    """连接远程攻击资产，并统一加速施法、动画结束发射和 Projectile 尺寸弹道检查。"""
    _, damage_effect_class = tools.require_blueprint(
        DAMAGE_EFFECT_PATH,
        unreal.GameplayEffect,
    )
    ability_defaults = unreal.get_default_object(asset_classes["ability_class"])
    ability_defaults.modify()
    ability_defaults.set_editor_property("damage_effect_class", damage_effect_class)
    ability_defaults.set_editor_property("projectile_class", asset_classes["projectile_class"])
    ability_defaults.set_editor_property(
        "cooldown_gameplay_effect_class",
        asset_classes["cooldown_class"],
    )
    ability_defaults.set_editor_property("attack_montage", montage)
    ability_defaults.set_editor_property("attack_range", 950.0)
    ability_defaults.set_editor_property("range_tolerance", 50.0)
    ability_defaults.set_editor_property("base_damage", 6.0)
    ability_defaults.set_editor_property("skill_multiplier", 1.0)
    ability_defaults.set_editor_property("release_delay", 0.0)
    ability_defaults.set_editor_property("montage_play_rate", 1.5)
    ability_defaults.set_editor_property("montage_start_section", unreal.Name("Default"))
    ability_defaults.set_editor_property(
        "projectile_spawn_offset",
        unreal.Vector(70.0, 0.0, 50.0),
    )
    ability_defaults.set_editor_property(
        "target_aim_offset",
        unreal.Vector(0.0, 0.0, 50.0),
    )
    ability_defaults.set_editor_property("projectile_path_trace_radius", 16.0)
    tools.save_asset(asset_classes["ability_path"])


def _configure_ranged_enemy(asset_classes):
    """让远程敌人只授予主远程攻击，并使用蓝色 Manny 材质区分外观。"""
    enemy_defaults = unreal.get_default_object(asset_classes["enemy_class"])
    enemy_defaults.modify()
    enemy_defaults.set_editor_property(
        "startup_abilities",
        [asset_classes["ability_class"]],
    )

    mesh_component = _get_editor_property(enemy_defaults, "mesh", "Mesh")
    if mesh_component is None:
        raise RuntimeError("BP_ArenaRangedEnemy did not expose its inherited Mesh component.")
    mesh_component.modify()
    mesh_component.set_material(
        0,
        tools.require_asset(RANGED_MATERIAL_PATH, unreal.MaterialInterface),
    )
    tools.save_asset(asset_classes["enemy_path"])


def _enemy_class_key(enemy_class):
    """生成稳定的敌人 Class 键，用于在重建 Entry 时找回精英字段。"""
    return enemy_class.get_path_name().lower() if enemy_class is not None else ""


def _capture_elite_fields(entries):
    """按 EnemyClass 保留已有的 EliteCount 与 EliteAffixPool。"""
    preserved = {}
    for entry in entries:
        enemy_class = entry.get_editor_property("enemy_class")
        preserved[_enemy_class_key(enemy_class)] = (
            int(entry.get_editor_property("elite_count")),
            list(entry.get_editor_property("elite_affix_pool")),
        )
    return preserved


def _make_enemy_entry(enemy_class, count, elite_fields=None):
    """构造敌人配置，并在脚本重跑时保留精英名额与词缀池。"""
    entry = tools.require_unreal_type("ArenaWaveEnemyEntry")()
    entry.set_editor_property("enemy_class", enemy_class)
    entry.set_editor_property("count", count)
    if elite_fields is not None:
        entry.set_editor_property("elite_count", elite_fields[0])
        entry.set_editor_property("elite_affix_pool", elite_fields[1])
    return entry


def _configure_wave_mixes(asset_classes):
    """只替换前四波 Enemies，保留已有间隔、奖励与 Boss 标记。"""
    wave_data = tools.require_asset(
        WAVE_DATA_PATH,
        tools.require_unreal_type("ArenaWaveDataAsset"),
    )
    _, melee_enemy_class = tools.require_blueprint(
        MELEE_ENEMY_PATH,
        tools.require_unreal_type("ArenaEnemyCharacter"),
    )
    enemy_classes = {
        MELEE_ENEMY_PATH: melee_enemy_class,
        RANGED_ENEMY_PATH: asset_classes["enemy_class"],
    }

    waves = list(wave_data.get_editor_property("waves"))
    while len(waves) < len(WAVE_MIXES):
        waves.append(tools.require_unreal_type("ArenaWaveConfig")())

    for wave_index, wave_mix in enumerate(WAVE_MIXES):
        preserved_elite_fields = _capture_elite_fields(
            list(waves[wave_index].get_editor_property("enemies"))
        )
        entries = [
            _make_enemy_entry(
                enemy_classes[enemy_path],
                count,
                preserved_elite_fields.get(
                    _enemy_class_key(enemy_classes[enemy_path])
                ),
            )
            for enemy_path, count in wave_mix
        ]
        waves[wave_index].set_editor_property("enemies", entries)

    wave_data.modify()
    wave_data.set_editor_property("waves", waves)
    tools.save_asset(WAVE_DATA_PATH)


def main():
    """执行安全预检、资产创建、引用连接和四波混合配置。"""
    source_animation = _validate_prerequisites()
    with unreal.ScopedSlowTask(5, "Setting up ranged enemy combat assets") as task:
        task.make_dialog(True)

        montage = _create_or_load_montage(source_animation)
        task.enter_progress_frame(1, "Created ranged attack montage")

        asset_classes = _create_runtime_blueprints()
        task.enter_progress_frame(1, "Created ranged enemy Blueprints")

        _configure_projectile(
            asset_classes["projectile_class"],
            asset_classes["projectile_path"],
        )
        task.enter_progress_frame(1, "Configured projectile")

        _configure_ability(asset_classes, montage)
        _configure_ranged_enemy(asset_classes)
        task.enter_progress_frame(1, "Configured ranged ability and enemy")

        _configure_wave_mixes(asset_classes)
        task.enter_progress_frame(1, "Configured four mixed waves")

    unreal.log("Ranged enemy setup completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Ranged enemy setup failed: {error}")
        raise
