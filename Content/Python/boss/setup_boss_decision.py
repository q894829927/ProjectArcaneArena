"""幂等创建 Boss 阶段二 A 的 Behavior Tree 资产外壳与类引用。

脚本只创建 Controller Blueprint、Blackboard 和 BehaviorTree，并连接已有 Boss Blueprint。
BehaviorTree 图需要按 README 的固定结构手动连接；脚本不会覆盖已经编辑的图。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

AI_DIRECTORY = "/Game/Boss/AI"
CONTROLLER_PATH = f"{AI_DIRECTORY}/BP_ArenaBossAIController"
BLACKBOARD_PATH = f"{AI_DIRECTORY}/BB_ArenaBoss"
BEHAVIOR_TREE_PATH = f"{AI_DIRECTORY}/BT_ArenaBoss"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
DEFAULT_INITIAL_ABILITY_DELAY = 3.0


def _require_editor_type(type_name):
    """写入任何资产前检查编辑器工厂类型，缺失时给出精确的手动创建提示。"""
    editor_type = getattr(unreal, type_name, None)
    if editor_type is None:
        raise RuntimeError(
            f"Unreal Python does not expose {type_name}. No asset was changed. "
            "Create a Blackboard named BB_ArenaBoss and a Behavior Tree named "
            "BT_ArenaBoss under /Game/Boss/AI manually, then follow this folder's README."
        )
    return editor_type


def _create_or_load_asset(asset_path, asset_class, factory_class):
    """幂等创建普通编辑器资产；已有资产只验证类型，不覆盖内容。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return tools.require_asset(asset_path, asset_class)

    destination_path, asset_name = asset_path.rsplit("/", 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        destination_path,
        asset_class,
        factory_class(),
    )
    if asset is None:
        raise RuntimeError(f"Failed to create asset: {asset_path}")
    return tools.require_asset(asset_path, asset_class)


def _ensure_target_actor_key(blackboard):
    """尝试创建 TargetActor Key；UE Python 未导出 Key 类型时返回 False 交由编辑器手动配置。"""
    blackboard_entry_type = getattr(unreal, "BlackboardEntry", None)
    object_key_type = getattr(unreal, "BlackboardKeyType_Object", None)
    if blackboard_entry_type is None or object_key_type is None:
        unreal.log_warning(
            "This Unreal Python build does not expose BlackboardEntry or "
            "BlackboardKeyType_Object. BB_ArenaBoss will still be created, but "
            "TargetActor must be added manually as Object / Actor / not Instance Synced."
        )
        return False

    keys = list(blackboard.get_editor_property("keys"))
    target_entries = [
        entry
        for entry in keys
        if str(entry.get_editor_property("entry_name")) == "TargetActor"
    ]
    if len(target_entries) > 1:
        raise RuntimeError("BB_ArenaBoss contains duplicate TargetActor keys.")

    if target_entries:
        key_type = target_entries[0].get_editor_property("key_type")
        if not isinstance(key_type, object_key_type):
            raise RuntimeError("BB_ArenaBoss.TargetActor must use the Object key type.")
        key_type.set_editor_property("base_class", unreal.Actor)
        target_entries[0].set_editor_property("instance_synced", False)
    else:
        key_type = unreal.new_object(
            object_key_type,
            outer=blackboard,
            name="TargetActor_KeyType",
        )
        key_type.set_editor_property("base_class", unreal.Actor)
        entry = blackboard_entry_type()
        entry.set_editor_property("entry_name", unreal.Name("TargetActor"))
        entry.set_editor_property("key_type", key_type)
        entry.set_editor_property("instance_synced", False)
        keys.append(entry)

    unrelated_keys = [
        entry
        for entry in keys
        if str(entry.get_editor_property("entry_name")) != "TargetActor"
    ]
    if unrelated_keys:
        names = ", ".join(
            str(entry.get_editor_property("entry_name")) for entry in unrelated_keys
        )
        unreal.log_warning(
            f"BB_ArenaBoss contains extra keys ({names}). The stage-two-A runtime only uses TargetActor."
        )

    blackboard.modify()
    blackboard.set_editor_property("keys", keys)
    return True


def _try_link_blackboard(behavior_tree, blackboard):
    """尝试写入 BehaviorTree 的 Blackboard 引用；属性未导出时返回 False 交由编辑器手动连接。"""
    try:
        behavior_tree.modify()
        behavior_tree.set_editor_property("blackboard_asset", blackboard)
        return True
    except Exception as error:
        unreal.log_warning(
            "This Unreal Python build does not expose BehaviorTree.BlackboardAsset. "
            "BT_ArenaBoss will still be created, but select BB_ArenaBoss manually in "
            f"the Behavior Tree editor. Details: {error}"
        )
        return False


def _compile_blueprint_if_available(blueprint, asset_path):
    """写入 Class Defaults 后主动编译 Blueprint；接口未导出时保留手动 Compile 回退。"""
    blueprint_editor_library = getattr(unreal, "BlueprintEditorLibrary", None)
    if blueprint_editor_library is None:
        unreal.log_warning(
            f"Unreal Python does not expose BlueprintEditorLibrary. Compile {asset_path} "
            "manually before PIE."
        )
        return False

    blueprint_editor_library.compile_blueprint(blueprint)
    return True


def _validate_runtime_blueprint_defaults(behavior_tree):
    """验证运行时 Controller、BehaviorTree、开场缓冲和 Boss 类引用。"""
    controller_class = tools.load_blueprint_class(CONTROLLER_PATH)
    controller_defaults = unreal.get_default_object(controller_class)
    configured_tree = controller_defaults.get_editor_property("behavior_tree_asset")
    if configured_tree != behavior_tree:
        raise RuntimeError(
            "BP_ArenaBossAIController did not retain BehaviorTreeAsset=BT_ArenaBoss. "
            "Set it in Class Defaults and compile the Blueprint manually."
        )
    configured_delay = controller_defaults.get_editor_property("initial_ability_delay")
    if abs(float(configured_delay) - DEFAULT_INITIAL_ABILITY_DELAY) > 0.001:
        raise RuntimeError(
            "BP_ArenaBossAIController did not retain InitialAbilityDelay="
            f"{DEFAULT_INITIAL_ABILITY_DELAY}. Set it in Class Defaults and compile manually."
        )

    boss_class = tools.load_blueprint_class(BOSS_CHARACTER_PATH)
    boss_defaults = unreal.get_default_object(boss_class)
    configured_controller_class = boss_defaults.get_editor_property("ai_controller_class")
    if configured_controller_class != controller_class:
        raise RuntimeError(
            "BP_ArenaBossCharacter did not retain AIControllerClass="
            "BP_ArenaBossAIController. Set it in Class Defaults and compile manually."
        )


def _validate_prerequisites():
    """在创建目录和资产前验证全部原生类、工厂与现有 Boss Blueprint。"""
    boss_controller_type = tools.require_unreal_type("ArenaBossAIController")
    boss_character_type = tools.require_unreal_type("ArenaBossCharacter")
    behavior_tree_factory_type = _require_editor_type("BehaviorTreeFactory")
    blackboard_factory_type = _require_editor_type("BlackboardDataFactory")
    _require_editor_type("BehaviorTree")
    _require_editor_type("BlackboardData")
    if unreal.EditorAssetLibrary.does_asset_exist(CONTROLLER_PATH):
        tools.require_blueprint(
            CONTROLLER_PATH,
            boss_controller_type,
        )
    tools.require_blueprint(BOSS_CHARACTER_PATH, boss_character_type)
    return (
        boss_controller_type,
        behavior_tree_factory_type,
        blackboard_factory_type,
    )


def main():
    """创建阶段二 A 资产、同步开场缓冲，并保留 BehaviorTree 图供手动搭建。"""
    (
        boss_controller_type,
        behavior_tree_factory_type,
        blackboard_factory_type,
    ) = _validate_prerequisites()

    if not unreal.EditorAssetLibrary.does_directory_exist(AI_DIRECTORY):
        unreal.EditorAssetLibrary.make_directory(AI_DIRECTORY)

    controller_blueprint, controller_class, controller_path = tools.create_or_load_blueprint(
        "BP_ArenaBossAIController",
        AI_DIRECTORY,
        boss_controller_type,
    )
    blackboard = _create_or_load_asset(
        BLACKBOARD_PATH,
        unreal.BlackboardData,
        blackboard_factory_type,
    )
    behavior_tree = _create_or_load_asset(
        BEHAVIOR_TREE_PATH,
        unreal.BehaviorTree,
        behavior_tree_factory_type,
    )

    target_key_was_configured = _ensure_target_actor_key(blackboard)
    blackboard_was_linked = _try_link_blackboard(behavior_tree, blackboard)

    controller_defaults = unreal.get_default_object(controller_class)
    controller_defaults.modify()
    controller_defaults.set_editor_property("behavior_tree_asset", behavior_tree)
    controller_defaults.set_editor_property(
        "initial_ability_delay",
        DEFAULT_INITIAL_ABILITY_DELAY,
    )

    boss_blueprint, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    boss_defaults.modify()
    boss_defaults.set_editor_property("ai_controller_class", controller_class)

    controller_was_compiled = _compile_blueprint_if_available(
        controller_blueprint,
        CONTROLLER_PATH,
    )
    boss_was_compiled = _compile_blueprint_if_available(
        boss_blueprint,
        BOSS_CHARACTER_PATH,
    )

    for asset_path in (
        controller_path,
        BLACKBOARD_PATH,
        BEHAVIOR_TREE_PATH,
        BOSS_CHARACTER_PATH,
    ):
        tools.save_asset(asset_path)

    _validate_runtime_blueprint_defaults(behavior_tree)

    target_key_message = (
        "TargetActor was configured automatically."
        if target_key_was_configured
        else "Add TargetActor manually to BB_ArenaBoss before editing the tree."
    )
    blackboard_link_message = (
        "BB_ArenaBoss was linked automatically."
        if blackboard_was_linked
        else "Select BB_ArenaBoss manually as BT_ArenaBoss's Blackboard Asset."
    )
    compile_message = (
        "Controller and Boss Blueprints were compiled automatically."
        if controller_was_compiled and boss_was_compiled
        else "Compile BP_ArenaBossAIController and BP_ArenaBossCharacter manually."
    )
    unreal.log_warning(
        f"Boss decision assets are ready. {target_key_message} "
        f"{blackboard_link_message} {compile_message} Open "
        "/Game/Boss/AI/BT_ArenaBoss and build the fixed stage-two-A graph described "
        "in Content/Python/boss/README.md. The script intentionally does not modify "
        "the BehaviorTree graph."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss decision setup failed: {error}")
        raise
