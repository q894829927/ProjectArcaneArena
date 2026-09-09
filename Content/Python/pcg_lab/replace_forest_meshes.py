"""把 SimpleForest 模板图的 Mesh 替换为 Stylized_Spruce_Forest 资产。

按原 Mesh 特征分组替换：
  - PCG_Tree_*      -> SM_Spruce_01..07（大树）
  - PCG_Seedling_*  -> 草/花/小树/灌木（地被）
  - PCG_Boulder_*   -> 岩石 + 枯木/树桩（岩石与杂物）
权重保持原模板的相对比例。
"""

import unreal


GRAPH_PATH = "/Game/Tests/PCG/forest/PCG_Forest"

TREE_ROOT = "/Game/Stylized_Spruce_Forest/Meshes/Trees"
PLANT_ROOT = "/Game/Stylized_Spruce_Forest/Meshes/Plants"
ROCK_ROOT = "/Game/Stylized_Spruce_Forest/Meshes/Rocks"
DEBRIS_ROOT = "/Game/Stylized_Spruce_Forest/Meshes/Debris"

# (资产相对路径, 权重)
TREE_ENTRIES = [
    ("SM_Spruce_01", 1),
    ("SM_Spruce_02", 1),
    ("SM_Spruce_03", 1),
    ("SM_Spruce_04", 1),
    ("SM_Spruce_05", 1),
    ("SM_Spruce_06", 1),
    ("SM_Spruce_07", 1),
]

GROUND_ENTRIES = [
    ("SM_Grass_01", 120),
    ("SM_Grass_02", 120),
    ("SM_Flowers_01", 40),
    ("SM_Small_Spruce_01", 30),
    ("SM_Spruce_Bush_01", 30),
    ("SM_Bush_01", 20),
]

ROCK_ENTRIES = [
    ("SM_Rock_01", 1),
    ("SM_Rock_02", 1),
    ("SM_Rock_03", 1),
    ("SM_Small_Rock_01", 2),
    ("SM_Small_Rock_02", 2),
    ("SM_Small_Rock_03", 2),
    ("SM_Log_01", 1),
    ("SM_Stump_01", 1),
]


def _load_mesh(asset_name):
    """按名称从资产包加载 Mesh；先用 AssetRegistry 精确定位避免 Error 噪音。"""
    for root in (TREE_ROOT, PLANT_ROOT, ROCK_ROOT, DEBRIS_ROOT):
        path = f"{root}/{asset_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            mesh = unreal.EditorAssetLibrary.load_asset(path)
            if mesh is not None:
                return mesh
    raise RuntimeError(f"Mesh asset not found: {asset_name}")


def _build_entries(specs):
    """按 (名称, 权重) 列表构造 mesh_entries。"""
    descriptor_class = unreal.PCGSoftISMComponentDescriptor
    entry_class = unreal.PCGMeshSelectorWeightedEntry
    entries = []
    for asset_name, weight in specs:
        descriptor = descriptor_class()
        descriptor.set_editor_property("static_mesh", _load_mesh(asset_name))
        entry = entry_class()
        entry.set_editor_property("descriptor", descriptor)
        entry.set_editor_property("weight", weight)
        entries.append(entry)
    return entries


def _get_mesh_paths(parameters):
    """读取 selector 参数里当前所有 Mesh 路径，用于识别分组。"""
    paths = []
    entries = parameters.get_editor_property("mesh_entries") or []
    for entry in entries:
        descriptor = entry.get_editor_property("descriptor")
        mesh = descriptor.get_editor_property("static_mesh") if descriptor else None
        if mesh:
            paths.append(mesh.get_path_name())
    return paths


def _replace_group(settings, new_specs, label):
    """替换指定 Spawner 节点的 mesh_entries。"""
    parameters = settings.get_editor_property("mesh_selector_parameters")
    if parameters is None:
        raise RuntimeError(f"{label}: MeshSelectorParameters is None.")
    parameters.set_editor_property("mesh_entries", _build_entries(new_specs))
    unreal.log(f"{label}: replaced with {len(new_specs)} entries")


def run():
    graph = unreal.EditorAssetLibrary.load_asset(GRAPH_PATH)
    if graph is None:
        unreal.log_error(f"Graph asset not found: {GRAPH_PATH}")
        return

    replaced_trees = replaced_ground = replaced_rocks = 0
    nodes = graph.get_editor_property("nodes")
    for node in nodes:
        try:
            settings = node.get_editor_property("settings")
        except Exception:
            settings = None
        if settings is None:
            try:
                settings = node.get_editor_property("settings_interface")
            except Exception:
                settings = None
        if settings is None or settings.get_class().get_name() != "PCGStaticMeshSpawnerSettings":
            continue

        parameters = settings.get_editor_property("mesh_selector_parameters")
        if parameters is None:
            continue
        paths = _get_mesh_paths(parameters)
        joined = " ".join(paths)

        if "PCG_Tree" in joined:
            _replace_group(settings, TREE_ENTRIES, "Tree spawner")
            replaced_trees += 1
        elif "PCG_Seedling" in joined:
            _replace_group(settings, GROUND_ENTRIES, "Ground spawner")
            replaced_ground += 1
        elif "PCG_Boulder" in joined:
            _replace_group(settings, ROCK_ENTRIES, "Rock spawner")
            replaced_rocks += 1
        else:
            unreal.log_warning(f"Unmatched spawner with meshes: {joined}")

    if not unreal.EditorAssetLibrary.save_asset(GRAPH_PATH):
        raise RuntimeError(f"Failed to save graph: {GRAPH_PATH}")

    unreal.log(
        f"Replaced {replaced_trees} tree / {replaced_ground} ground / "
        f"{replaced_rocks} rock spawner(s); graph saved."
    )


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Forest mesh replace failed: {error}")
