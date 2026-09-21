"""给 Lvl_PCGLab 的 PCG 图修复 StaticMeshSpawner 的 Mesh Selector。

诊断发现 PCG_Decor_Lab 的 StaticMeshSpawner 节点 MeshSelector=NONE，
导致即使上游点数据正常，也没有任何 Mesh 可实例化。
本脚本幂等配置一个 Weighted Mesh Selector（SM_Cylinder），并保存图。
"""

import unreal


LAB_GRAPH_PATH = "/Game/Tests/PCG/PCG_Decor_Lab"
MESH_PATHS = (
    "/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder",
    "/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube",
)


def _find_spawner_node(graph):
    """找到 StaticMeshSpawner 节点，找不到返回 None。"""
    nodes = graph.get_editor_property("nodes")
    for node in nodes:
        settings = None
        try:
            settings = node.get_editor_property("settings")
        except Exception:
            settings = None
        if settings is None:
            try:
                settings = node.get_editor_property("settings_interface")
            except Exception:
                settings = None
        if settings and settings.get_class().get_name() == "PCGStaticMeshSpawnerSettings":
            return node, settings
    return None, None


def _build_weighted_selector():
    """构造 Weighted Mesh Selector，包含两条 Mesh 条目。"""
    selector_class = getattr(unreal, "PCGMeshSelectorWeighted", None)
    if selector_class is None:
        raise RuntimeError("PCGMeshSelectorWeighted is unavailable.")

    selector = unreal.new_object(selector_class)
    entries = []

    descriptor_class = getattr(unreal, "PCGSoftISMComponentDescriptor", None)
    entry_class = getattr(unreal, "PCGMeshSelectorWeightedEntry", None)
    if descriptor_class is None or entry_class is None:
        raise RuntimeError("Mesh selector entry structs are unavailable.")

    for index, mesh_path in enumerate(MESH_PATHS):
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
    return selector


def run():
    graph = unreal.EditorAssetLibrary.load_asset(LAB_GRAPH_PATH)
    if graph is None:
        unreal.log_error(f"Graph asset not found: {LAB_GRAPH_PATH}")
        return

    node, settings = _find_spawner_node(graph)
    if node is None:
        unreal.log_error("StaticMeshSpawner node not found in graph.")
        return

    selector_class = getattr(unreal, "PCGMeshSelectorWeighted", None)
    if selector_class is None:
        raise RuntimeError("PCGMeshSelectorWeighted is unavailable.")

    current_type = None
    current_params = None
    try:
        current_type = settings.get_editor_property("mesh_selector_type")
    except Exception:
        current_type = None
    try:
        current_params = settings.get_editor_property("mesh_selector_parameters")
    except Exception:
        current_params = None

    existing_entries = 0
    if current_params is not None:
        try:
            existing_entries = len(
                current_params.get_editor_property("mesh_entries") or []
            )
        except Exception:
            existing_entries = 0

    if current_type is not None and existing_entries > 0:
        unreal.log(
            f"MeshSelector already configured: type={current_type.get_name()} "
            f"entries={existing_entries}"
        )
        return

    selector = _build_weighted_selector()
    settings.set_editor_property("mesh_selector_type", selector_class)
    settings.set_editor_property("mesh_selector_parameters", selector)
    unreal.EditorAssetLibrary.save_asset(LAB_GRAPH_PATH)
    unreal.log(
        f"Configured Weighted MeshSelectorType + Parameters on "
        f"StaticMeshSpawner with {len(MESH_PATHS)} entries, graph saved."
    )


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Mesh selector fix failed: {error}")
