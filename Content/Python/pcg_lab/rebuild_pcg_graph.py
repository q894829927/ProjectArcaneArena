"""程序化重建一张确定性的 PCG 最小图，排除人工接线不确定性。

图结构：
  GraphInput.In --Out--> VolumeSampler.Volume
  VolumeSampler.Out --Out--> StaticMeshSpawner.In
  StaticMeshSpawner.Out --Out--> GraphOutput.Out

然后挂到 PCGVolume 并触发 Generate，报告生成资源数。
"""

import unreal


NEW_GRAPH_PATH = "/Game/Tests/PCG/PCG_Decor_Lab_Minimal"
GROUND_TAG = "PCGLabGround"
MESH_PATHS = (
    "/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder",
    "/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
)


def _existing_graph():
    return unreal.EditorAssetLibrary.load_asset(NEW_GRAPH_PATH)


def _create_fresh_graph():
    """删除旧图并新建空 PCG 图，保证脚本幂等可重复。"""
    if unreal.EditorAssetLibrary.does_asset_exist(NEW_GRAPH_PATH):
        unreal.EditorAssetLibrary.delete_asset(NEW_GRAPH_PATH)

    factory = unreal.PCGGraphFactory()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    graph = asset_tools.create_asset(
        "PCG_Decor_Lab_Minimal", "/Game/Tests/PCG", unreal.PCGGraph, factory
    )
    if graph is None:
        raise RuntimeError("Failed to create new PCG graph asset.")
    return graph


def _add_node(graph, settings_class):
    """向图添加节点并返回 (node, settings)。"""
    result = graph.add_node_of_type(settings_class)
    if isinstance(result, (tuple, list)):
        node, settings = result[0], result[1]
    else:
        node, settings = result, None
    if node is None:
        raise RuntimeError(f"Failed to add node of type {settings_class.get_name()}")
    if settings is None:
        settings = node.get_settings()
    return node, settings


def _configure_surface_sampler(settings):
    """配置采样密度，保证能看到实例。"""
    settings.set_editor_property("voxel_size", unreal.Vector(100.0, 100.0, 100.0))
    settings.set_editor_property("point_steepness", 1.0)


def _configure_spawner(settings):
    """把构造函数已创建的 Weighted MeshSelector 参数对象填入 Mesh 条目。"""
    selector = settings.get_editor_property("mesh_selector_parameters")
    if selector is None:
        raise RuntimeError("MeshSelectorParameters is None; type should create it.")

    descriptor_class = unreal.PCGSoftISMComponentDescriptor
    entry_class = unreal.PCGMeshSelectorWeightedEntry
    entries = []
    for mesh_path in MESH_PATHS:
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if mesh is None:
            raise RuntimeError(f"Mesh asset not found: {mesh_path}")
        descriptor = descriptor_class()
        descriptor.set_editor_property("static_mesh", mesh)
        entry = entry_class()
        entry.set_editor_property("descriptor", descriptor)
        entry.set_editor_property("weight", 1)
        entries.append(entry)
    selector.set_editor_property("mesh_entries", entries)


def _build_graph():
    """构建最小图并保存。"""
    graph = _create_fresh_graph()

    sampler_node, sampler_settings = _add_node(graph, unreal.PCGVolumeSamplerSettings)
    spawner_node, spawner_settings = _add_node(graph, unreal.PCGStaticMeshSpawnerSettings)

    _configure_surface_sampler(sampler_settings)
    _configure_spawner(spawner_settings)

    input_node = graph.get_editor_property("input_node")
    output_node = graph.get_editor_property("output_node")

    # GraphInput.In -> VolumeSampler.Volume
    input_node.add_edge_to("In", sampler_node, "Volume")
    # VolumeSampler.Out -> StaticMeshSpawner.In
    sampler_node.add_edge_to("Out", spawner_node, "In")
    # StaticMeshSpawner.Out -> GraphOutput.Out
    spawner_node.add_edge_to("Out", output_node, "Out")

    unreal.EditorAssetLibrary.save_asset(NEW_GRAPH_PATH)
    return graph


def _assign_and_generate():
    """把新图挂到 PCGVolume 并触发生成，报告结果。"""
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor_subsystem.get_editor_world()
    if world is None:
        raise RuntimeError("No editor world.")

    volume_components = []
    for actor in editor_actor_subsystem.get_all_level_actors():
        if actor.get_class().get_name() == "PCGVolume":
            volume_components.extend(actor.get_components_by_class(unreal.PCGComponent) or [])
    if not volume_components:
        raise RuntimeError("No PCGVolume with PCGComponent found.")

    graph = unreal.EditorAssetLibrary.load_asset(NEW_GRAPH_PATH)
    for component in volume_components:
        component.set_graph(graph)
        unreal.log(f"Assigned {NEW_GRAPH_PATH} to {component.get_name()}")
        component.generate_local(True)

    unreal.log("Waiting 1s for async generation ...")
    import time
    time.sleep(1.0)

    unreal.log("Generation request completed; inspect the viewport and LogPCG output.")


def run():
    graph = _build_graph()
    unreal.log(f"Built graph: {graph.get_path_name()} with "
               f"{len(graph.get_editor_property('nodes'))} nodes")
    _assign_and_generate()


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"PCG graph rebuild failed: {error}")
