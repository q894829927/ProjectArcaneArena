"""Configure GCN_Shield_Active to use the converted Muriel shield Niagara system.

Run this file from Unreal Editor via Tools > Execute Python Script.
"""

import unreal


SHIELD_SYSTEM_PATH = "/Game/Niagara/NS_Shield_Muriel"
SHIELD_CUE_PATH = "/Game/GAS/GameplayCues/DurationCue/GCN_Shield_Active"
SHIELD_SCALE = 1.0


def require_asset(asset_path, expected_class=None):
    """加载必需资产，并在缺失或类型不匹配时停止写入。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Required asset was not found: {asset_path}")

    if expected_class is not None and not isinstance(asset, expected_class):
        raise RuntimeError(
            f"Asset {asset_path} is {asset.get_class().get_name()}, "
            f"expected {expected_class.__name__}."
        )
    return asset


def get_editor_property(obj, *property_names):
    """兼容 Python 风格和 C++ 风格属性名。"""
    last_error = None
    for property_name in property_names:
        try:
            return obj.get_editor_property(property_name)
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not read any property {property_names} from {obj}: {last_error}"
    )


def set_editor_property(obj, value, *property_names):
    """写入第一个可编辑的候选属性，兼容不同 UE Python 命名形式。"""
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


def find_enum_value(enum_type, *candidate_names):
    """查找 UE Python 枚举值，处理不同版本的枚举命名差异。"""
    for candidate_name in candidate_names:
        if hasattr(enum_type, candidate_name):
            return getattr(enum_type, candidate_name)
    raise RuntimeError(
        f"Could not find any enum value {candidate_names} on {enum_type}."
    )


def configure_default_placement(cue_defaults):
    """让护盾从目标根组件原点生成，并继承 C++ 传入的附着组件。"""
    placement = get_editor_property(
        cue_defaults,
        "default_placement_info",
        "DefaultPlacementInfo",
    )

    attach_to_target = find_enum_value(
        unreal.GameplayCueNotifyAttachPolicy,
        "ATTACH_TO_TARGET",
        "AttachToTarget",
    )
    snap_to_target = find_enum_value(
        unreal.AttachmentRule,
        "SNAP_TO_TARGET",
        "SnapToTarget",
    )

    set_editor_property(placement, unreal.Name("None"), "socket_name", "SocketName")
    set_editor_property(placement, attach_to_target, "attach_policy", "AttachPolicy")
    set_editor_property(
        placement,
        snap_to_target,
        "attachment_rule",
        "AttachmentRule",
    )
    set_editor_property(
        placement,
        False,
        "override_rotation",
        "bOverrideRotation",
    )
    set_editor_property(
        placement,
        True,
        "override_scale",
        "bOverrideScale",
    )
    set_editor_property(
        placement,
        unreal.Vector(SHIELD_SCALE, SHIELD_SCALE, SHIELD_SCALE),
        "scale_override",
        "ScaleOverride",
    )
    set_editor_property(
        cue_defaults,
        placement,
        "default_placement_info",
        "DefaultPlacementInfo",
    )


def configure_looping_particle(cue_defaults, shield_system):
    """用 Muriel Niagara 替换旧护盾粒子，并让它使用 Cue 默认附着规则。"""
    particle_info = unreal.GameplayCueNotifyParticleInfo()
    set_editor_property(
        particle_info,
        shield_system,
        "niagara_system",
        "NiagaraSystem",
    )
    set_editor_property(
        particle_info,
        False,
        "override_spawn_condition",
        "bOverrideSpawnCondition",
    )
    set_editor_property(
        particle_info,
        False,
        "override_placement_info",
        "bOverridePlacementInfo",
    )
    set_editor_property(particle_info, False, "cast_shadow", "bCastShadow")

    looping_effects = get_editor_property(
        cue_defaults,
        "looping_effects",
        "LoopingEffects",
    )
    set_editor_property(
        looping_effects,
        [particle_info],
        "looping_particles",
        "LoopingParticles",
    )
    set_editor_property(
        cue_defaults,
        looping_effects,
        "looping_effects",
        "LoopingEffects",
    )


def main():
    """配置并保存持续护盾 GameplayCue。"""
    shield_system = require_asset(SHIELD_SYSTEM_PATH, unreal.NiagaraSystem)
    shield_cue_blueprint = require_asset(SHIELD_CUE_PATH, unreal.Blueprint)
    shield_cue_class = unreal.EditorAssetLibrary.load_blueprint_class(SHIELD_CUE_PATH)
    if shield_cue_class is None:
        raise RuntimeError(f"Could not load generated class for {SHIELD_CUE_PATH}")

    cue_defaults = unreal.get_default_object(shield_cue_class)
    configure_default_placement(cue_defaults)
    configure_looping_particle(cue_defaults, shield_system)

    if not unreal.EditorAssetLibrary.save_loaded_asset(shield_cue_blueprint, False):
        raise RuntimeError(f"Failed to save {SHIELD_CUE_PATH}")

    unreal.log(
        "Muriel shield cue setup completed: "
        f"cue={SHIELD_CUE_PATH}, system={SHIELD_SYSTEM_PATH}, scale={SHIELD_SCALE}"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Muriel shield cue setup failed: {error}")
        raise
