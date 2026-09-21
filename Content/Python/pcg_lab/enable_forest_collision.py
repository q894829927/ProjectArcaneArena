"""为 PCG_Forest 所有 Spawner 的 Mesh entry 描述符开启碰撞。

PCG 描述符构造时强制 NoCollision（PCGISMDescriptor.cpp），本脚本把每条
Mesh entry 的 bUseDefaultCollision 设为 true，使生成的 ISM 使用每个
StaticMesh 自带的简单碰撞体（Spruce 资产自带碰撞）。

用法：编辑器内执行
  py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/pcg_lab/enable_forest_collision.py"
"""

import unreal


GRAPH_PATH = "/Game/Tests/PCG/forest/PCG_Forest"


def _get_settings(node):
    """读取节点 Settings 对象，读取失败返回 None。"""
    try:
        return node.get_editor_property("settings")
    except Exception:
        pass
    try:
        return node.get_editor_property("settings_interface")
    except Exception:
        return None


def run():
    graph = unreal.EditorAssetLibrary.load_asset(GRAPH_PATH)
    if graph is None:
        unreal.log_error(f"Graph asset not found: {GRAPH_PATH}")
        return

    total_entries = 0
    updated_spawners = 0
    nodes = graph.get_editor_property("nodes")
    for node in nodes:
        settings = _get_settings(node)
        if settings is None or settings.get_class().get_name() != "PCGStaticMeshSpawnerSettings":
            continue

        parameters = settings.get_editor_property("mesh_selector_parameters")
        if parameters is None:
            continue
        entries = parameters.get_editor_property("mesh_entries") or []
        entry_count = 0
        for entry in entries:
            descriptor = entry.get_editor_property("descriptor")
            if descriptor is None:
                continue
            descriptor.set_editor_property("b_use_default_collision", True)
            entry.set_editor_property("descriptor", descriptor)
            entry_count += 1
        if entry_count:
            parameters.set_editor_property("mesh_entries", entries)
            updated_spawners += 1
            total_entries += entry_count

    if not unreal.EditorAssetLibrary.save_asset(GRAPH_PATH):
        raise RuntimeError(f"Failed to save graph: {GRAPH_PATH}")

    unreal.log(
        f"Enabled bUseDefaultCollision on {updated_spawners} spawner(s) "
        f"({total_entries} entries); graph saved. Regenerate to apply."
    )


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Forest collision enable failed: {error}")
