"""在 Lvl_PCGLab 中对 PCGVolume 编程触发 Generate 并报告生成资源数。"""

import unreal


VOLUME_TAG = "PCGLabVolume"


def run():
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor_subsystem.get_editor_world()
    if world is None:
        unreal.log_error("No editor world.")
        return

    found = False
    for actor in editor_actor_subsystem.get_all_level_actors():
        if actor.get_class().get_name() != "PCGVolume":
            continue
        components = actor.get_components_by_class(unreal.PCGComponent) or []
        for component in components:
            found = True
            unreal.log(f"Generating on {actor.get_actor_label()} ...")
            component.generate_local(True)
            unreal.log(f"generate_local called on {component.get_name()}")

    if not found:
        unreal.log_error("No PCGVolume with PCGComponent found in current level.")
        return

    unreal.log("Generation requested. Wait a moment then rerun diagnose_pcg.py.")


def main():
    run()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"PCG generate failed: {error}")
