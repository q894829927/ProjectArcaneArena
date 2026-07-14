"""根据顶部配置创建或更新一次性 Burst GameplayCue Blueprint。"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

# rotation 使用 (Pitch, Yaw, Roll)，scale 使用 (X, Y, Z)。
BURST_CUE_CONFIGS = [
    {
        "asset_name": "GCN_Overload_Explosion",
        "destination_path": "/Game/GAS/GameplayCues/InstaneCue",
        "template_path": "/Game/GAS/GameplayCues/InstaneCue/GCN_Hit_Lightning",
        "cue_tag": "GameplayCue.Combo.Overload",
        "niagara_path": "/Game/SlashTrail_SoftTofu/Niagara/Lightning/NS_Hit_Lightning_once",
        "socket_name": "None",
        "attach_policy": "DO_NOT_ATTACH",
        "attachment_rule": "KEEP_WORLD",
        "override_rotation": False,
        "rotation": (0.0, 0.0, 0.0),
        "override_scale": True,
        "scale": (2.5, 2.5, 2.5),
        "cast_shadow": False,
    },
]

REQUIRED_CONFIG_KEYS = (
    "asset_name",
    "destination_path",
    "template_path",
    "cue_tag",
    "niagara_path",
    "socket_name",
    "attach_policy",
    "attachment_rule",
    "override_rotation",
    "rotation",
    "override_scale",
    "scale",
    "cast_shadow",
)


def _asset_path(config):
    """根据单项配置生成目标 Burst GameplayCue Blueprint 路径。"""
    return f"{config['destination_path']}/{config['asset_name']}"


def _require_python_types():
    """验证 Burst Cue 配置依赖的 UE Python 反射类型。"""
    for type_name in (
        "GameplayCueNotify_AttachPolicy",
        "GameplayCueNotify_PlacementInfo",
        "GameplayCueNotify_ParticleInfo",
        "GameplayCueNotify_BurstEffects",
        "GameplayCueNotify_Burst",
    ):
        tools.require_unreal_type(type_name)


def _validate_vector_tuple(value, field_name, index):
    """验证旋转或缩放配置是三个数值。"""
    if not isinstance(value, (tuple, list)) or len(value) != 3:
        raise RuntimeError(
            f"BURST_CUE_CONFIGS[{index}].{field_name} must contain three values."
        )


def _validate_cue_defaults(asset_path, generated_class):
    """确认目标 Burst Cue 类暴露生成器需要写入的默认属性。"""
    cue_defaults = unreal.get_default_object(generated_class)
    for property_name in ("gameplay_cue_tag", "default_placement_info", "burst_effects"):
        try:
            cue_defaults.get_editor_property(property_name)
        except Exception as error:
            raise RuntimeError(
                f"Cue {asset_path} does not expose '{property_name}': {error}"
            )


def validate_configs():
    """在写入前验证 Burst Cue 配置、模板、Niagara、Tag 和父类。"""
    _require_python_types()
    cue_parent_class = unreal.GameplayCueNotify_Burst
    seen_asset_paths = set()

    for index, config in enumerate(BURST_CUE_CONFIGS):
        missing_keys = [key for key in REQUIRED_CONFIG_KEYS if key not in config]
        if missing_keys:
            raise RuntimeError(
                f"BURST_CUE_CONFIGS[{index}] is missing keys: {', '.join(missing_keys)}"
            )
        _validate_vector_tuple(config["rotation"], "rotation", index)
        _validate_vector_tuple(config["scale"], "scale", index)

        asset_path = _asset_path(config)
        if asset_path in seen_asset_paths:
            raise RuntimeError(f"Duplicate Burst GameplayCue path: {asset_path}")
        seen_asset_paths.add(asset_path)

        tools.make_tag(config["cue_tag"])
        tools.require_asset(config["niagara_path"], unreal.NiagaraSystem)
        tools.resolve_enum_value(
            unreal.GameplayCueNotify_AttachPolicy,
            config["attach_policy"],
        )
        tools.resolve_enum_value(unreal.AttachmentRule, config["attachment_rule"])
        _, template_class = tools.require_blueprint(config["template_path"], cue_parent_class)
        _validate_cue_defaults(config["template_path"], template_class)

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            _, generated_class = tools.require_blueprint(asset_path, cue_parent_class)
            _validate_cue_defaults(asset_path, generated_class)


def _configure_cue(generated_class, config):
    """把单个配置写入 Burst GameplayCue 的类默认对象。"""
    niagara_system = tools.require_asset(config["niagara_path"], unreal.NiagaraSystem)
    cue_defaults = unreal.get_default_object(generated_class)
    cue_defaults.modify()
    cue_defaults.set_editor_property("gameplay_cue_tag", tools.make_tag(config["cue_tag"]))

    rotation = config["rotation"]
    scale = config["scale"]
    placement = unreal.GameplayCueNotify_PlacementInfo(
        socket_name=unreal.Name(config["socket_name"]),
        attach_policy=tools.resolve_enum_value(
            unreal.GameplayCueNotify_AttachPolicy,
            config["attach_policy"],
        ),
        attachment_rule=tools.resolve_enum_value(
            unreal.AttachmentRule,
            config["attachment_rule"],
        ),
        override_rotation=bool(config["override_rotation"]),
        override_scale=bool(config["override_scale"]),
        rotation_override=unreal.Rotator(rotation[0], rotation[1], rotation[2]),
        scale_override=unreal.Vector(scale[0], scale[1], scale[2]),
    )
    cue_defaults.set_editor_property("default_placement_info", placement)

    particle_info = unreal.GameplayCueNotify_ParticleInfo(
        niagara_system=niagara_system,
        override_spawn_condition=False,
        override_placement_info=False,
        cast_shadow=bool(config["cast_shadow"]),
    )
    burst_effects = unreal.GameplayCueNotify_BurstEffects(
        burst_particles=[particle_info],
    )
    cue_defaults.set_editor_property("burst_effects", burst_effects)


def run():
    """验证后幂等创建或更新全部 Burst GameplayCue Blueprint。"""
    validate_configs()
    for config in BURST_CUE_CONFIGS:
        _, generated_class, asset_path = tools.duplicate_or_load_blueprint(
            config["asset_name"],
            config["destination_path"],
            config["template_path"],
            unreal.GameplayCueNotify_Burst,
        )
        _configure_cue(generated_class, config)
        tools.save_asset(asset_path)
        unreal.log(f"Configured Burst GameplayCue: {asset_path}")


def main():
    """提供可从 Unreal Editor 单独执行的 Burst GameplayCue 入口。"""
    run()
    unreal.log("Burst GameplayCue generation completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Burst GameplayCue generation failed: {error}")
        raise
