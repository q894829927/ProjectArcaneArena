"""创建并连接阶段六 A 使用的 Overlay、CameraShake 和伤害反馈资源。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

DESTINATION_PATH = "/Game/GAS/DamageFeedback"
OVERLAY_MATERIAL_PATH = f"{DESTINATION_PATH}/M_ArenaHitFlashOverlay"

CHARACTER_PATHS = (
    "/Game/Characters/ArenaPlayer/BP_ArenaPlayerCharacter",
    "/Game/Characters/ArenaEnemy/BP_ArenaEnemyCharacter",
    "/Game/Characters/ArenaEnemy/BP_ArenaRangedEnemy",
    "/Game/Boss/Character/BP_ArenaBossCharacter",
    "/Game/Tests/Overload/BP_ArenaEnemy_OverloadDummy",
)

RESULT_CUE_PATHS = (
    "/Game/GAS/GameplayCues/InstaneCue/GCN_ShieldHit",
    "/Game/GAS/GameplayCues/InstaneCue/GCN_ShieldBreak",
    "/Game/GAS/GameplayCues/InstaneCue/GCN_HealthHit",
    "/Game/GAS/GameplayCues/InstaneCue/GCN_ShieldBreakHealthHit",
)

DAMAGE_NUMBER_ACTOR_PATH = "/Game/UI/BP_ArenaDamageNumberActor"
BASIC_ATTACK_ACTIVATION_CUE_PATH = (
    "/Game/GAS/GameplayCues/InstaneCue/GCN_BasicAttack_Activate"
)
BASIC_ATTACK_SWING_SOUND_PATH = (
    "/Game/SlashTrail_SoftTofu/Resource/Audio/SharpSlash/SC_Basic_Slash_Cue"
)
HEALTH_HIT_SOUND_PATH = "/Game/SlashTrail_SoftTofu/Resource/Audio/Hit/SC_Hit_Cue"
SHIELD_HIT_SOUND_PATH = "/Game/SlashTrail_SoftTofu/Resource/Audio/scifi/SC_Scifi_Slash"
SHIELD_BREAK_SOUND_PATH = (
    "/Game/SlashTrail_SoftTofu/Resource/Audio/Cyberpunk/"
    "SC_Slash_Cyberpunk_Cue"
)
HIT_FEEDBACK_ATTENUATION_PATH = (
    "/Game/GAS/DamageFeedback/SA_ArenaHitFeedback"
)

CAMERA_SHAKE_CONFIGS = (
    {
        "asset_name": "CS_DamageLight",
        "duration": 0.12,
        "blend_in": 0.01,
        "blend_out": 0.05,
        "location_amplitude": 0.35,
        "location_frequency": 24.0,
        "rotation_amplitude": 0.06,
        "rotation_frequency": 22.0,
    },
    {
        "asset_name": "CS_DamageMedium",
        "duration": 0.18,
        "blend_in": 0.01,
        "blend_out": 0.07,
        "location_amplitude": 0.65,
        "location_frequency": 22.0,
        "rotation_amplitude": 0.12,
        "rotation_frequency": 20.0,
    },
    {
        "asset_name": "CS_DamageHeavy",
        "duration": 0.24,
        "blend_in": 0.015,
        "blend_out": 0.09,
        "location_amplitude": 1.0,
        "location_frequency": 20.0,
        "rotation_amplitude": 0.2,
        "rotation_frequency": 18.0,
    },
    {
        "asset_name": "CS_ShieldBreak",
        "duration": 0.2,
        "blend_in": 0.01,
        "blend_out": 0.08,
        "location_amplitude": 0.8,
        "location_frequency": 28.0,
        "rotation_amplitude": 0.15,
        "rotation_frequency": 25.0,
    },
)


def _resolve_enum_member(enum_type, *candidate_names):
    """兼容不同 UE Python 版本导出的枚举成员命名。"""
    for candidate_name in candidate_names:
        if hasattr(enum_type, candidate_name):
            return getattr(enum_type, candidate_name)
    raise RuntimeError(
        f"None of {candidate_names} exists on Unreal enum {enum_type}."
    )


def _camera_shake_path(config):
    """返回一项 CameraShake 配置对应的资产路径。"""
    return f"{DESTINATION_PATH}/{config['asset_name']}"


def _validate_component_properties(hit_reaction_component, character_path):
    """确认最新 C++ 反射数据包含阶段六 A 需要写入的组件属性。"""
    required_properties = (
        "hit_flash_overlay_material",
        "shield_hit_sound",
        "shield_break_sound",
        "health_hit_sound",
        "hit_feedback_attenuation_settings",
        "light_damage_camera_shake_class",
        "medium_damage_camera_shake_class",
        "heavy_damage_camera_shake_class",
        "shield_break_camera_shake_class",
        "shield_hit_camera_shake_scale",
        "shield_break_base_shake_scale",
        "health_damage_shake_multiplier",
        "max_camera_shake_scale",
        "third_person_camera_shake_scale_multiplier",
    )
    for property_name in required_properties:
        try:
            hit_reaction_component.get_editor_property(property_name)
        except Exception as error:
            raise RuntimeError(
                f"{character_path}.HitReactionComponent does not expose "
                f"'{property_name}'. Compile the latest C++ code and restart Unreal Editor: "
                f"{error}"
            )


def _validate_prerequisites():
    """在写入前验证全部 Python 类型、现有资产和角色组件。"""
    required_type_names = (
        "MaterialFactoryNew",
        "MaterialEditingLibrary",
        "MaterialExpressionVectorParameter",
        "MaterialExpressionScalarParameter",
        "MaterialExpressionConstant",
        "MaterialExpressionMultiply",
        "CameraShakeBase",
        "PerlinNoiseCameraShakePattern",
        "PerlinNoiseShaker",
        "GameplayCueNotify_SoundInfo",
        "SoundAttenuation",
    )
    for type_name in required_type_names:
        tools.require_unreal_type(type_name)

    for method_name in ("get_root_shake_pattern", "set_root_shake_pattern"):
        if not hasattr(unreal.CameraShakeBase, method_name):
            raise RuntimeError(
                "This Unreal Python build does not expose "
                f"CameraShakeBase.{method_name}(). No project asset has been changed."
            )

    character_parent = tools.require_unreal_type("ArenaCharacterBase")
    damage_number_parent = tools.require_unreal_type("ArenaDamageNumberActor")
    tools.require_blueprint(DAMAGE_NUMBER_ACTOR_PATH, damage_number_parent)

    for cue_path in RESULT_CUE_PATHS:
        tools.require_blueprint(cue_path, unreal.GameplayCueNotify_Burst)
    tools.require_blueprint(
        BASIC_ATTACK_ACTIVATION_CUE_PATH,
        unreal.GameplayCueNotify_Burst,
    )

    for sound_path in (
        BASIC_ATTACK_SWING_SOUND_PATH,
        HEALTH_HIT_SOUND_PATH,
        SHIELD_HIT_SOUND_PATH,
        SHIELD_BREAK_SOUND_PATH,
    ):
        tools.require_asset(sound_path, unreal.SoundBase)
    tools.require_asset(
        HIT_FEEDBACK_ATTENUATION_PATH,
        unreal.SoundAttenuation,
    )

    for character_path in CHARACTER_PATHS:
        _, character_class = tools.require_blueprint(character_path, character_parent)
        character_defaults = unreal.get_default_object(character_class)
        hit_reaction_component = character_defaults.get_editor_property(
            "hit_reaction_component"
        )
        if hit_reaction_component is None:
            raise RuntimeError(
                f"Character {character_path} has no HitReactionComponent."
            )
        _validate_component_properties(hit_reaction_component, character_path)

    # 先在 Transient Package 验证 CameraShake Pattern 的关键属性，避免创建资产后才发现 API 不可用。
    transient_pattern = unreal.new_object(
        unreal.PerlinNoiseCameraShakePattern,
        name="ArenaDamageFeedbackPreflightPattern",
    )
    for property_name in (
        "duration",
        "blend_in_time",
        "blend_out_time",
        "location_amplitude_multiplier",
        "rotation_amplitude_multiplier",
        "x",
        "y",
        "z",
        "pitch",
        "yaw",
        "roll",
    ):
        try:
            transient_pattern.get_editor_property(property_name)
        except Exception as error:
            raise RuntimeError(
                "This Unreal Python build cannot configure Perlin CameraShake "
                f"property '{property_name}'. No project asset has been changed: {error}"
            )


def _create_or_load_material():
    """幂等创建项目自有的受击 Overlay Material。"""
    if unreal.EditorAssetLibrary.does_asset_exist(OVERLAY_MATERIAL_PATH):
        return tools.require_asset(OVERLAY_MATERIAL_PATH, unreal.Material)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_ArenaHitFlashOverlay",
        DESTINATION_PATH,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError(
            f"Failed to create Overlay material: {OVERLAY_MATERIAL_PATH}"
        )
    return material


def _configure_overlay_material(material):
    """重建简洁的 Unlit Translucent Overlay 图，并暴露统一颜色和强度参数。"""
    material.modify()
    material.set_editor_property(
        "blend_mode",
        _resolve_enum_member(unreal.BlendMode, "BLEND_TRANSLUCENT", "TRANSLUCENT"),
    )
    material.set_editor_property(
        "shading_model",
        _resolve_enum_member(
            unreal.MaterialShadingModel,
            "MSM_UNLIT",
            "UNLIT",
        ),
    )
    material.set_editor_property("two_sided", True)
    try:
        material.set_editor_property("automatically_set_usage_in_editor", True)
    except Exception:
        unreal.log_warning(
            "M_ArenaHitFlashOverlay does not expose automatically_set_usage_in_editor; "
            "the editor will compile skeletal usage when the material is assigned."
        )

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    color_parameter = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -650,
        -160,
    )
    color_parameter.set_editor_property("parameter_name", unreal.Name("HitFlashColor"))
    color_parameter.set_editor_property(
        "default_value",
        unreal.LinearColor(1.0, 0.12, 0.08, 1.0),
    )

    intensity_parameter = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -650,
        40,
    )
    intensity_parameter.set_editor_property(
        "parameter_name",
        unreal.Name("HitFlashIntensity"),
    )
    intensity_parameter.set_editor_property("default_value", 0.0)

    emissive_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -360,
        -120,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        color_parameter,
        "",
        emissive_multiply,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        intensity_parameter,
        "",
        emissive_multiply,
        "B",
    )

    opacity_scale = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        -620,
        210,
    )
    opacity_scale.set_editor_property("r", 0.35)
    opacity_multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -360,
        140,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        intensity_parameter,
        "",
        opacity_multiply,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        opacity_scale,
        "",
        opacity_multiply,
        "B",
    )

    unreal.MaterialEditingLibrary.connect_material_property(
        emissive_multiply,
        "",
        _resolve_enum_member(
            unreal.MaterialProperty,
            "MP_EMISSIVE_COLOR",
            "EMISSIVE_COLOR",
        ),
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity_multiply,
        "",
        _resolve_enum_member(
            unreal.MaterialProperty,
            "MP_OPACITY",
            "OPACITY",
        ),
    )
    unreal.MaterialEditingLibrary.recompile_material(material)


def _configure_camera_shake(config):
    """创建或更新一个克制的 Perlin CameraShake Blueprint。"""
    _, generated_class, asset_path = tools.create_or_load_blueprint(
        config["asset_name"],
        DESTINATION_PATH,
        unreal.CameraShakeBase,
    )
    defaults = unreal.get_default_object(generated_class)
    defaults.modify()

    pattern = defaults.get_root_shake_pattern()
    if not isinstance(pattern, unreal.PerlinNoiseCameraShakePattern):
        pattern = unreal.new_object(
            unreal.PerlinNoiseCameraShakePattern,
            outer=defaults,
            name="ArenaDamagePerlinPattern",
        )
        defaults.set_root_shake_pattern(pattern)

    pattern.modify()
    pattern.set_editor_property("duration", float(config["duration"]))
    pattern.set_editor_property("blend_in_time", float(config["blend_in"]))
    pattern.set_editor_property("blend_out_time", float(config["blend_out"]))
    pattern.set_editor_property("location_amplitude_multiplier", 1.0)
    pattern.set_editor_property("location_frequency_multiplier", 1.0)
    pattern.set_editor_property("rotation_amplitude_multiplier", 1.0)
    pattern.set_editor_property("rotation_frequency_multiplier", 1.0)

    location_amplitude = float(config["location_amplitude"])
    location_frequency = float(config["location_frequency"])
    rotation_amplitude = float(config["rotation_amplitude"])
    rotation_frequency = float(config["rotation_frequency"])
    pattern.set_editor_property(
        "x",
        unreal.PerlinNoiseShaker(
            amplitude=location_amplitude * 0.35,
            frequency=location_frequency,
        ),
    )
    pattern.set_editor_property(
        "y",
        unreal.PerlinNoiseShaker(
            amplitude=location_amplitude,
            frequency=location_frequency,
        ),
    )
    pattern.set_editor_property(
        "z",
        unreal.PerlinNoiseShaker(
            amplitude=location_amplitude * 0.5,
            frequency=location_frequency,
        ),
    )
    pattern.set_editor_property(
        "pitch",
        unreal.PerlinNoiseShaker(
            amplitude=rotation_amplitude,
            frequency=rotation_frequency,
        ),
    )
    pattern.set_editor_property(
        "yaw",
        unreal.PerlinNoiseShaker(
            amplitude=rotation_amplitude * 0.7,
            frequency=rotation_frequency,
        ),
    )
    pattern.set_editor_property(
        "roll",
        unreal.PerlinNoiseShaker(
            amplitude=rotation_amplitude * 0.35,
            frequency=rotation_frequency,
        ),
    )
    tools.save_asset(asset_path)
    return generated_class


def _configure_character_components(overlay_material, camera_shake_classes):
    """把统一 Overlay、竞技场衰减、音效和 CameraShake 写入四类角色的公共反馈组件。"""
    health_hit_sound = tools.require_asset(HEALTH_HIT_SOUND_PATH, unreal.SoundBase)
    shield_hit_sound = tools.require_asset(SHIELD_HIT_SOUND_PATH, unreal.SoundBase)
    shield_break_sound = tools.require_asset(
        SHIELD_BREAK_SOUND_PATH,
        unreal.SoundBase,
    )
    hit_feedback_attenuation = tools.require_asset(
        HIT_FEEDBACK_ATTENUATION_PATH,
        unreal.SoundAttenuation,
    )
    character_parent = tools.require_unreal_type("ArenaCharacterBase")

    for character_path in CHARACTER_PATHS:
        _, character_class = tools.require_blueprint(character_path, character_parent)
        defaults = unreal.get_default_object(character_class)
        component = defaults.get_editor_property("hit_reaction_component")
        component.modify()
        component.set_editor_property(
            "hit_flash_overlay_material",
            overlay_material,
        )
        component.set_editor_property("health_hit_sound", health_hit_sound)
        component.set_editor_property("shield_hit_sound", shield_hit_sound)
        component.set_editor_property("shield_break_sound", shield_break_sound)
        component.set_editor_property(
            "hit_feedback_attenuation_settings",
            hit_feedback_attenuation,
        )
        component.set_editor_property(
            "light_damage_camera_shake_class",
            camera_shake_classes["CS_DamageLight"],
        )
        component.set_editor_property(
            "medium_damage_camera_shake_class",
            camera_shake_classes["CS_DamageMedium"],
        )
        component.set_editor_property(
            "heavy_damage_camera_shake_class",
            camera_shake_classes["CS_DamageHeavy"],
        )
        component.set_editor_property(
            "shield_break_camera_shake_class",
            camera_shake_classes["CS_ShieldBreak"],
        )
        component.set_editor_property("shield_hit_camera_shake_scale", 0.1)
        component.set_editor_property("shield_break_base_shake_scale", 0.35)
        component.set_editor_property("health_damage_shake_multiplier", 3.0)
        component.set_editor_property("max_camera_shake_scale", 1.0)
        component.set_editor_property(
            "third_person_camera_shake_scale_multiplier",
            0.65,
        )
        tools.save_asset(character_path)
        unreal.log(f"Configured DamageFeedback polish: {character_path}")


def _configure_basic_attack_activation_sound():
    """向普通攻击激活 Cue 追加唯一挥击音，同时保留已有粒子和其他 Burst 配置。"""
    swing_sound = tools.require_asset(
        BASIC_ATTACK_SWING_SOUND_PATH,
        unreal.SoundBase,
    )
    _, cue_class = tools.require_blueprint(
        BASIC_ATTACK_ACTIVATION_CUE_PATH,
        unreal.GameplayCueNotify_Burst,
    )
    cue_defaults = unreal.get_default_object(cue_class)
    cue_defaults.modify()
    burst_effects = cue_defaults.get_editor_property("burst_effects")
    burst_sounds = list(burst_effects.get_editor_property("burst_sounds"))
    has_swing_sound = any(
        sound_info.get_editor_property("sound") == swing_sound
        for sound_info in burst_sounds
    )
    if not has_swing_sound:
        burst_sounds.append(
            unreal.GameplayCueNotify_SoundInfo(
                sound=swing_sound,
                override_spawn_condition=False,
                override_placement_info=False,
                use_sound_parameter_interface=False,
            )
        )
        burst_effects.set_editor_property("burst_sounds", burst_sounds)
        cue_defaults.set_editor_property("burst_effects", burst_effects)
        tools.save_asset(BASIC_ATTACK_ACTIVATION_CUE_PATH)
        unreal.log("Configured BasicAttack activation swing sound.")
    else:
        unreal.log("BasicAttack activation swing sound is already configured.")


def _verify_saved_configuration():
    """重新加载保存结果，确认核心引用没有在编译或保存阶段丢失。"""
    overlay_material = tools.require_asset(OVERLAY_MATERIAL_PATH, unreal.Material)
    hit_feedback_attenuation = tools.require_asset(
        HIT_FEEDBACK_ATTENUATION_PATH,
        unreal.SoundAttenuation,
    )
    character_parent = tools.require_unreal_type("ArenaCharacterBase")
    for character_path in CHARACTER_PATHS:
        _, character_class = tools.require_blueprint(character_path, character_parent)
        component = unreal.get_default_object(character_class).get_editor_property(
            "hit_reaction_component"
        )
        saved_overlay = component.get_editor_property("hit_flash_overlay_material")
        if saved_overlay != overlay_material:
            raise RuntimeError(
                f"Saved Overlay reference verification failed: {character_path}"
            )
        saved_attenuation = component.get_editor_property(
            "hit_feedback_attenuation_settings"
        )
        if saved_attenuation != hit_feedback_attenuation:
            raise RuntimeError(
                f"Saved hit-feedback attenuation verification failed: {character_path}"
            )
        for property_name in (
            "health_hit_sound",
            "shield_hit_sound",
            "shield_break_sound",
            "hit_feedback_attenuation_settings",
            "light_damage_camera_shake_class",
            "medium_damage_camera_shake_class",
            "heavy_damage_camera_shake_class",
            "shield_break_camera_shake_class",
        ):
            if component.get_editor_property(property_name) is None:
                raise RuntimeError(
                    f"Saved DamageFeedback property is empty: "
                    f"{character_path}.{property_name}"
                )

    swing_sound = tools.require_asset(
        BASIC_ATTACK_SWING_SOUND_PATH,
        unreal.SoundBase,
    )
    _, cue_class = tools.require_blueprint(
        BASIC_ATTACK_ACTIVATION_CUE_PATH,
        unreal.GameplayCueNotify_Burst,
    )
    burst_effects = unreal.get_default_object(cue_class).get_editor_property(
        "burst_effects"
    )
    matching_sounds = [
        sound_info
        for sound_info in burst_effects.get_editor_property("burst_sounds")
        if sound_info.get_editor_property("sound") == swing_sound
    ]
    if len(matching_sounds) != 1:
        raise RuntimeError(
            "BasicAttack activation Cue must contain exactly one configured "
            "SC_Basic_Slash_Cue sound."
        )


def run():
    """预检后创建表现资产、连接角色组件并验证保存结果。"""
    _validate_prerequisites()
    overlay_material = _create_or_load_material()
    _configure_overlay_material(overlay_material)
    tools.save_asset(OVERLAY_MATERIAL_PATH)

    camera_shake_classes = {}
    for config in CAMERA_SHAKE_CONFIGS:
        camera_shake_classes[config["asset_name"]] = _configure_camera_shake(
            config
        )

    _configure_basic_attack_activation_sound()
    _configure_character_components(overlay_material, camera_shake_classes)
    _verify_saved_configuration()


def main():
    """提供 Unreal Editor 控制台可直接执行的阶段六 A 资产入口。"""
    run()
    unreal.log(
        "Damage feedback polish setup completed successfully. "
        "Run it again to verify idempotency."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(
            "Damage feedback polish setup failed. No suffixed duplicate assets are "
            "created by this script. If CameraShake Python types are unavailable, "
            "manually create four CameraShakeBase assets under /Game/GAS/DamageFeedback, "
            "use a Perlin root pattern, then assign them on each HitReactionComponent. "
            f"Details: {error}"
        )
        raise
