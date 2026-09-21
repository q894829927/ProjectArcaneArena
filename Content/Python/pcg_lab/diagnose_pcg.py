"""诊断 PCG 图节点配置、组件挂图和生成资源数量。

默认诊断 SimpleForest 模板图 /Game/Tests/PCG/forest/PCG_Forest。
"""

import unreal


LAB_GRAPH_PATH = "/Game/Tests/PCG/forest/PCG_Forest"
VOLUME_TAG = "PCGLabVolume"


def _safe(loader):
    """执行读取函数，任何异常都返回 None 而不是中断整个脚本。"""
    try:
        return loader()
    except Exception:
        return None


def _settings_summary(node):
    """返回节点的 Settings 类名和关键配置，无法读取时返回 None。"""
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
    if settings is None:
        return "SETTINGS_UNAVAILABLE"

    class_name = settings.get_class().get_name()
    lines = [class_name]

    if class_name == "PCGDataFromActorSettings":
        selector = _safe(lambda: settings.get_editor_property("actor_selector"))
        if selector:
            lines.append(
                "  ActorFilter=%s"
                % _safe(lambda: str(selector.get_editor_property("actor_filter")))
            )
            lines.append(
                "  ActorSelection=%s"
                % _safe(lambda: str(selector.get_editor_property("actor_selection")))
            )
            lines.append(
                "  ActorSelectionTag=%s"
                % _safe(lambda: str(selector.get_editor_property("actor_selection_tag")))
            )
        component_selector = _safe(
            lambda: settings.get_editor_property("component_selector")
        )
        if component_selector:
            lines.append(
                "  ComponentSelection=%s"
                % _safe(
                    lambda: str(
                        component_selector.get_editor_property("component_selection")
                    )
                )
            )
            lines.append(
                "  ComponentSelectionTag=%s"
                % _safe(
                    lambda: str(
                        component_selector.get_editor_property(
                            "component_selection_tag"
                        )
                    )
                )
            )
            lines.append(
                "  ComponentSelectionClass=%s"
                % _safe(
                    lambda: str(
                        component_selector.get_editor_property(
                            "component_selection_class"
                        )
                    )
                )
            )
    elif class_name == "PCGSurfaceSamplerSettings":
        lines.append(
            "  PointsPerSquaredMeter=%s"
            % _safe(lambda: str(settings.get_editor_property("points_per_squared_meter")))
        )
        lines.append(
            "  PointExtents=%s"
            % _safe(lambda: str(settings.get_editor_property("point_extents")))
        )
        lines.append(
            "  bUnbounded=%s" % _safe(lambda: str(settings.get_editor_property("b_unbounded")))
        )
    elif class_name == "PCGStaticMeshSpawnerSettings":
        selector_type = _safe(
            lambda: settings.get_editor_property("mesh_selector_type")
        )
        parameters = _safe(
            lambda: settings.get_editor_property("mesh_selector_parameters")
        )
        lines.append(
            "  MeshSelectorType=%s"
            % (_safe(lambda: selector_type.get_name()) if selector_type else "NONE")
        )
        if parameters:
            entries = _safe(lambda: parameters.get_editor_property("mesh_entries"))
            lines.append(
                "  MeshSelectorParameters=%s (%s entries)"
                % (parameters.get_class().get_name(), len(entries) if entries else 0)
            )
            for entry in entries or []:
                descriptor = _safe(lambda: entry.get_editor_property("descriptor"))
                if descriptor:
                    mesh = _safe(lambda: descriptor.get_editor_property("static_mesh"))
                    lines.append(
                        "    Mesh=%s"
                        % (_safe(lambda: mesh.get_path_name()) if mesh else "NONE")
                    )
                weight = _safe(lambda: entry.get_editor_property("weight"))
                lines.append(f"    Weight={weight}")
        else:
            lines.append("  MeshSelectorParameters=NONE")
    elif class_name == "PCGDensityFilterSettings":
        lines.append(
            "  LowerBound=%s"
            % _safe(lambda: str(settings.get_editor_property("lower_bound")))
        )
        lines.append(
            "  UpperBound=%s"
            % _safe(lambda: str(settings.get_editor_property("upper_bound")))
        )

    return "\n".join(lines)


def _pin_label(pin):
    """读取 Pin 的显示名。"""
    if pin is None:
        return "?"
    props = _safe(lambda: pin.get_editor_property("properties"))
    if props is None:
        return "?"
    return _safe(lambda: str(props.get_editor_property("label"))) or "?"


def _pin_edges_summary(pin):
    """解析 Pin 上每条边的去向（输出 Pin 的边 -> 目标 Pin；输入 Pin 的边 <- 来源 Pin）。"""
    edges = _safe(lambda: pin.get_editor_property("edges"))
    if not edges:
        return "no edges"
    summaries = []
    for edge in edges:
        input_pin = _safe(lambda: edge.get_editor_property("input_pin"))
        output_pin = _safe(lambda: edge.get_editor_property("output_pin"))
        summaries.append(f"{_pin_label(output_pin)}->{_pin_label(input_pin)}")
    return "; ".join(summaries)


def _dump_boundary_nodes(graph):
    """打印图的 Input/Output 边界节点及其连线。"""
    unreal.log("=== Graph boundary nodes ===")
    input_node = _safe(lambda: graph.get_editor_property("input_node"))
    output_node = _safe(lambda: graph.get_editor_property("output_node"))
    for label, node in (("Input", input_node), ("Output", output_node)):
        if node is None:
            unreal.log(f"{label}: NONE")
            continue
        unreal.log(f"{label}: {node.get_name()} ({node.get_class().get_name()})")
        for pin in (_safe(lambda: node.get_editor_property("input_pins")) or []):
            label_name = _safe(
                lambda: pin.get_editor_property("properties").get_editor_property("label")
            )
            unreal.log(f"  IN  {label_name}: {_pin_edges_summary(pin)}")
        for pin in (_safe(lambda: node.get_editor_property("output_pins")) or []):
            label_name = _safe(
                lambda: pin.get_editor_property("properties").get_editor_property("label")
            )
            unreal.log(f"  OUT {label_name}: {_pin_edges_summary(pin)}")


def _dump_graph(graph):
    """打印图里每个节点的 Settings 类名、关键配置和 Pin 连边。"""
    nodes = _safe(lambda: graph.get_editor_property("nodes"))
    unreal.log(f"=== Graph nodes ({len(nodes) if nodes else 0}) ===")
    if not nodes:
        return
    for index, node in enumerate(nodes):
        settings_summary = _settings_summary(node)
        unreal.log(f"--- Node {index} ---")
        unreal.log(settings_summary)

        input_pins = _safe(lambda: node.get_editor_property("input_pins"))
        for pin in input_pins or []:
            label = _safe(
                lambda: pin.get_editor_property("properties").get_editor_property("label")
            )
            edges = _pin_edges_summary(pin)
            unreal.log(f"  IN  {label}: {edges}")

        output_pins = _safe(lambda: node.get_editor_property("output_pins"))
        for pin in output_pins or []:
            label = _safe(
                lambda: pin.get_editor_property("properties").get_editor_property("label")
            )
            edges = _pin_edges_summary(pin)
            unreal.log(f"  OUT {label}: {edges}")


def _dump_level_pcg():
    """打印关卡 PCGVolume 的组件挂图、激活状态与生成资源数量。"""
    unreal.log("=== Level PCG components ===")
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = _safe(editor_subsystem.get_editor_world)
    if world is None:
        unreal.log_error("No editor world.")
        return

    found = False
    for actor in editor_actor_subsystem.get_all_level_actors():
        tags = _safe(lambda: actor.get_editor_property("tags")) or []
        if VOLUME_TAG not in tags and "PCG" not in actor.get_class().get_name():
            continue
        found = True
        unreal.log(f"Actor: {actor.get_actor_label()} ({actor.get_class().get_name()}) tags={tags}")

        components = _safe(lambda: actor.get_components_by_class(unreal.PCGComponent)) or []
        if not components:
            unreal.log("  NO PCGComponent on this actor!")
            continue
        for component in components:
            activated = _safe(lambda: component.get_editor_property("b_activated"))
            graph = _safe(lambda: component.get_editor_property("graph"))
            graph_instance = _safe(lambda: component.get_editor_property("graph_instance"))
            generated = _safe(lambda: component.get_editor_property("generated_resources"))
            unreal.log(f"  PCGComponent={component.get_name()} bActivated={activated}")
            if graph:
                unreal.log(f"    graph={_safe(lambda: graph.get_path_name())}")
            if graph_instance:
                unreal.log(f"    graph_instance={_safe(lambda: graph_instance.get_path_name())}")
                inst_graph = _safe(lambda: graph_instance.get_editor_property("graph"))
                if inst_graph:
                    unreal.log(f"      instance.graph={_safe(lambda: inst_graph.get_path_name())}")
            unreal.log(f"    generated_resources={len(generated) if generated else 0}")
            output_data = _safe(lambda: component.get_editor_property("generated_graph_output"))
            if output_data is not None:
                tagged = _safe(lambda: output_data.get_editor_property("tagged_data"))
                unreal.log(f"    generated_graph_output.tagged_data={len(tagged) if tagged else 0}")
    if not found:
        unreal.log("No PCG volume / PCG actor found in level.")


def run():
    graph = unreal.EditorAssetLibrary.load_asset(LAB_GRAPH_PATH)
    if graph is None:
        unreal.log_error(f"Graph asset not found: {LAB_GRAPH_PATH}")
        return
    unreal.log(f"Loaded graph: {graph.get_path_name()} class={graph.get_class().get_name()}")
    _dump_boundary_nodes(graph)
    _dump_graph(graph)
    _dump_level_pcg()


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"PCG diagnostic failed: {error}")
