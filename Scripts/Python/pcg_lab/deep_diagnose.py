"""综合诊断 Lvl_PCGLab：执行前置条件 + 生成结果。

检查：
1. 关卡里所有 Actor 及其标签（确认地面带 PCGLabGround）。
2. PCGVolume 的 PCGComponent bActivated / graph / 子系统。
3. 触发 Generate 后（等待一帧）的 generated_resources 与图输出。
"""

import time

import unreal


GROUND_TAG = "PCGLabGround"
VOLUME_TAG = "PCGLabVolume"


def _safe(loader):
    try:
        return loader()
    except Exception:
        return None


def _dump_actors(editor_actor_subsystem):
    """打印所有 Actor 与标签，重点找 PCGLabGround / PCGLabVolume / StaticMeshActor。"""
    unreal.log("=== Level actors ===")
    for actor in editor_actor_subsystem.get_all_level_actors():
        label = _safe(lambda: actor.get_actor_label())
        class_name = _safe(lambda: actor.get_class().get_name())
        tags = _safe(lambda: list(actor.get_editor_property("tags"))) or []
        unreal.log(f"  {label} ({class_name}) tags={tags}")
        if class_name == "StaticMeshActor":
            mesh_comp = _safe(
                lambda: actor.get_editor_property("static_mesh_component")
            )
            if mesh_comp:
                mesh = _safe(lambda: mesh_comp.get_editor_property("static_mesh"))
                unreal.log(f"    static_mesh={_safe(lambda: mesh.get_path_name()) if mesh else 'NONE'}")


def _dump_component(component):
    """打印 PCGComponent 执行前置条件。"""
    unreal.log(f"=== PCGComponent {component.get_name()} ===")
    b_activated = _safe(lambda: component.get_editor_property("b_activated"))
    unreal.log(f"  bActivated={b_activated}")
    graph = _safe(lambda: component.get_editor_property("graph"))
    graph_instance = _safe(lambda: component.get_editor_property("graph_instance"))
    if graph:
        unreal.log(f"  graph={_safe(lambda: graph.get_path_name())}")
    if graph_instance:
        unreal.log(f"  graph_instance={_safe(lambda: graph_instance.get_path_name())}")
        inst_graph = _safe(lambda: graph_instance.get_editor_property("graph"))
        if inst_graph:
            unreal.log(f"    instance.graph={_safe(lambda: inst_graph.get_path_name())}")

    subsystem = _safe(lambda: unreal.PCGSubsystem())
    unreal.log(f"  PCGSubsystem class available={subsystem is not None}")


def run():
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor_subsystem.get_editor_world()
    if world is None:
        unreal.log_error("No editor world.")
        return

    _dump_actors(editor_actor_subsystem)

    volume_components = []
    for actor in editor_actor_subsystem.get_all_level_actors():
        if actor.get_class().get_name() == "PCGVolume":
            components = actor.get_components_by_class(unreal.PCGComponent) or []
            volume_components.extend(components)

    if not volume_components:
        unreal.log_error("No PCGVolume with PCGComponent found.")
        return

    for component in volume_components:
        _dump_component(component)
        unreal.log(f"  Calling generate_local(True) on {component.get_name()} ...")
        component.generate_local(True)

    # 生成是异步的，等待后读取结果。
    unreal.log("Waiting 1s for async generation ...")
    time.sleep(1.0)

    for component in volume_components:
        generated = _safe(lambda: component.get_editor_property("generated_resources"))
        output = _safe(lambda: component.get_editor_property("generated_graph_output"))
        tagged = _safe(lambda: output.get_editor_property("tagged_data")) if output else None
        unreal.log(
            f"  AFTER GENERATE: generated_resources={len(generated) if generated else 0} "
            f"graph_output_tagged={len(tagged) if tagged else 0}"
        )


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"PCG deep diagnostic failed: {error}")
