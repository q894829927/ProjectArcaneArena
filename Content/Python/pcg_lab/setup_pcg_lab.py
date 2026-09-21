"""为 PCG 实操课创建独立实验关卡骨架。

用法（配合编辑器的原生 Save Current Level As 流程，与 Overload 测试关卡同约定）：

1. 确认 PCG 插件已在 ProjectArcaneArena.uproject 启用，并重启编辑器。
2. File > New Level（Empty），然后 Save Current Level As 保存为：
   /Game/Tests/PCG/Lvl_PCGLab
3. 保持该关卡打开，在编辑器控制台执行本脚本：
   py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/pcg_lab/setup_pcg_lab.py"
4. 脚本会在当前关卡放置：
   - 一块缩放为平坦地面的 StaticMeshActor（SM_Cube），作为 Surface 采样源；
   - 清理脚本早期生成的空刷体 PCGVolume（若有）。
   重复执行会先清理旧实例再重建，不会产生重复 Actor。

注意：APCGVolume 的几何在 BSP Brush 中，spawn_actor_from_class 不会执行
BrushBuilder，刷体为空、包围盒为零，Surface Sampler 采样不到任何表面。
因此 PCGVolume 必须由编辑器 Place Actors 面板原生放置（会执行默认
立方体 BrushBuilder），放置后手动缩放并把 Graph 指到 PCG_Decor_Lab。

PCG 图资产（如 PCG_Decor_Lab）与图内节点连线属于编辑器交互式操作，
不在此脚本自动生成，由练习步骤在 PCG 编辑器内手工完成。
"""

import unreal


LAB_ROOT = "/Game/Tests/PCG"
LAB_LEVEL_PATH = f"{LAB_ROOT}/Lvl_PCGLab"
GROUND_TAG = "PCGLabGround"
VOLUME_TAG = "PCGLabVolume"

# 地面覆盖范围（SM_Cube 单边 100 单位，scale 后约为 2000x2000）。
GROUND_SCALE = (20.0, 20.0, 0.05)
GROUND_Z = 0.0


def _require_native_types():
    """确认地面所需原生类型可用；不可用时提示重启编辑器。"""
    static_mesh_actor_type = getattr(unreal, "StaticMeshActor", None)
    if static_mesh_actor_type is None:
        raise RuntimeError("StaticMeshActor is unavailable. Restart Unreal Editor.")
    return static_mesh_actor_type


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


def _spawn_ground(editor_actor_subsystem, static_mesh_actor_type):
    """创建一块缩放为平坦地面的 StaticMeshActor，作为 Surface 采样源。"""
    cube_asset = unreal.EditorAssetLibrary.load_asset(
        "/Game/LevelPrototyping/Meshes/SM_Cube"
    )
    if cube_asset is None:
        raise RuntimeError("Ground mesh SM_Cube was not found.")

    ground = editor_actor_subsystem.spawn_actor_from_class(
        static_mesh_actor_type,
        unreal.Vector(0.0, 0.0, GROUND_Z),
        unreal.Rotator(0.0, 0.0, 0.0),
        False,
    )
    if ground is None:
        raise RuntimeError("Failed to spawn ground StaticMeshActor.")

    ground.set_actor_label("PCGLab_Ground")
    ground.set_editor_property("tags", [GROUND_TAG])
    ground.set_actor_scale3d(unreal.Vector(*GROUND_SCALE))
    mesh_component = ground.get_editor_property("static_mesh_component")
    mesh_component.set_editor_property("static_mesh", cube_asset)
    # 地面作为采样源参与碰撞与导航，供练习观察装饰避开墙体。
    mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    return ground


def _configure_lab_level():
    """仅在已打开的 Lvl_PCGLab 中重建地面并清理空刷体 Volume。"""
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor_subsystem.get_editor_world()
    if world is None:
        raise RuntimeError("Failed to resolve the current editor World.")

    current_level_path = _canonical_path(world.get_path_name())
    if current_level_path != LAB_LEVEL_PATH:
        unreal.log_warning(
            "PCG lab assets are ready. Use File > Save Current Level As to create "
            f"{LAB_LEVEL_PATH}, keep that level open, then run this script again."
        )
        return False

    static_mesh_actor_type = _require_native_types()
    _destroy_tagged_actors(editor_actor_subsystem, GROUND_TAG)
    # 清理早期脚本生成的空刷体 Volume；正式 Volume 需编辑器原生放置。
    _destroy_tagged_actors(editor_actor_subsystem, VOLUME_TAG)

    _spawn_ground(editor_actor_subsystem, static_mesh_actor_type)

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem.save_current_level():
        raise RuntimeError(f"Failed to save PCG lab level: {LAB_LEVEL_PATH}")
    return True


def run():
    """重建实验关卡地面；PCGVolume 需编辑器原生放置。"""
    if _configure_lab_level():
        unreal.log(
            f"PCG lab ground ready: {LAB_LEVEL_PATH}. "
            "Place a PCG Volume from the Place Actors panel, scale it over the "
            "ground, set its PCG Component Graph to PCG_Decor_Lab, then Generate."
        )


def main():
    """提供可从 Unreal Editor 控制台单独执行的 PCG 实验入口。"""
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"PCG lab setup failed: {error}")
