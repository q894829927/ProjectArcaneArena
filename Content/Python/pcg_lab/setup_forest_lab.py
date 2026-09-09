"""为森林学习场景创建独立实验关卡的环境骨架。

用法（与 PCG lab 同约定，配合编辑器原生 Save Current Level As 流程）：

1. File > New Level（Empty），Save Current Level As 保存为：
   /Game/Tests/Forest/Lvl_ForestLab
2. 保持该关卡打开，在编辑器控制台执行：
   py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/pcg_lab/setup_forest_lab.py"
3. 脚本会放置 Directional Light、Sky Light 和一个覆盖地形的 PCGVolume。
   Landscape 地形由 Landscape Mode 手工创建（脚本不建地图、不建地形）。

注意：APCGVolume 的几何在 BSP Brush 中，脚本 spawn 出的 Volume 刷体为空、
包围盒为零，SurfaceSampler 无法采样。因此正式 Volume 需编辑器 Place Actors
面板原生放置后手工缩放；脚本只清理旧的空刷体 Volume。
"""

import unreal


LAB_ROOT = "/Game/Tests/Forest"
LAB_LEVEL_PATH = f"{LAB_ROOT}/Lvl_ForestLab"
LIGHT_TAG = "ForestLabLight"
VOLUME_TAG = "ForestLabVolume"

# PCGVolume 覆盖范围建议值（单位：Unreal 厘米），按 4x4 63x63 Landscape 估计。
VOLUME_SCALE = (22.0, 22.0, 4.0)
VOLUME_Z = 500.0


def _canonical_path(asset_or_path):
    """把 actor/资产路径统一为不带对象名的包路径，用于关卡路径比较。"""
    raw_path = (
        asset_or_path.get_path_name()
        if hasattr(asset_or_path, "get_path_name")
        else str(asset_or_path)
    )
    object_separator_index = raw_path.find(".", raw_path.rfind("/"))
    return (
        raw_path[:object_separator_index]
        if object_separator_index >= 0
        else raw_path
    )


def _destroy_tagged_actors(editor_actor_subsystem, tag):
    """销毁当前关卡中带指定 Tag 的 Actor，保证脚本可重复执行。"""
    for actor in editor_actor_subsystem.get_all_level_actors():
        if tag in actor.get_editor_property("tags"):
            editor_actor_subsystem.destroy_actor(actor)


def _spawn_light(editor_actor_subsystem, light_class, label, tag, rotation):
    """生成一个光照 Actor 并打上可清理标签。"""
    light = editor_actor_subsystem.spawn_actor_from_class(
        light_class,
        unreal.Vector(0.0, 0.0, 0.0),
        rotation,
        False,
    )
    if light is None:
        raise RuntimeError(f"Failed to spawn {label}.")
    light.set_actor_label(label)
    light.set_editor_property("tags", [tag])
    return light


def _configure_lab_level():
    """仅在已打开的 Lvl_ForestLab 中重建光照并清理旧 Volume。"""
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor_subsystem.get_editor_world()
    if world is None:
        raise RuntimeError("Failed to resolve the current editor World.")

    current_level_path = _canonical_path(world.get_path_name())
    if current_level_path != LAB_LEVEL_PATH:
        unreal.log_warning(
            "Forest lab assets are ready. Use File > Save Current Level As to create "
            f"{LAB_LEVEL_PATH}, keep that level open, then run this script again."
        )
        return False

    _destroy_tagged_actors(editor_actor_subsystem, LIGHT_TAG)
    _destroy_tagged_actors(editor_actor_subsystem, VOLUME_TAG)

    _spawn_light(
        editor_actor_subsystem,
        unreal.DirectionalLight,
        "ForestLab_DirectionalLight",
        LIGHT_TAG,
        unreal.Rotator(-45.0, -30.0, 0.0),
    )
    _spawn_light(
        editor_actor_subsystem,
        unreal.SkyLight,
        "ForestLab_SkyLight",
        LIGHT_TAG,
        unreal.Rotator(0.0, 0.0, 0.0),
    )

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.save_current_level():
        raise RuntimeError(f"Failed to save forest lab level: {LAB_LEVEL_PATH}")
    return True


def run():
    """重建森林实验关卡的光照；PCGVolume 与 Landscape 需编辑器手工放置。"""
    if _configure_lab_level():
        unreal.log(
            f"Forest lab lights ready: {LAB_LEVEL_PATH}. "
            "Place a PCG Volume from the Place Actors panel covering the landscape, "
            "then start the PCG_Forest graph."
        )


def main():
    """提供可从 Unreal Editor 控制台单独执行的森林实验入口。"""
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Forest lab setup failed: {error}")
