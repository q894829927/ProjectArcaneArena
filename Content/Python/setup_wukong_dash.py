"""Create and configure the Wukong Manny dash montage for GA_Dash.

Run this file from Unreal Editor via Tools > Execute Python Script.
"""

import unreal


SOURCE_ANIMATION_PATH = "/Game/wukongManny/RMB_Evade_CC"
REFERENCE_MONTAGE_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/Jump/AM_Dash"
MONTAGE_PACKAGE_PATH = "/Game/wukongManny"
MONTAGE_NAME = "AM_PlayerDash_Wukong"
MONTAGE_ASSET_PATH = f"{MONTAGE_PACKAGE_PATH}/{MONTAGE_NAME}"
GA_DASH_PATH = "/Game/GAS/GameplayAbility/GA_Dash"
MONTAGE_PLAY_RATE = 4.0


def require_asset(asset_path, expected_class=None):
    """加载必需资产，并在类型不匹配时立即终止，避免写入错误资源。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Required asset was not found: {asset_path}")

    if expected_class is not None and not isinstance(asset, expected_class):
        raise RuntimeError(
            f"Asset {asset_path} is {asset.get_class().get_name()}, "
            f"expected {expected_class.__name__}."
        )
    return asset


def get_editor_property(obj, *property_names):
    """兼容 Python 风格与 C++ 风格属性名，并返回第一个可读取属性。"""
    last_error = None
    for property_name in property_names:
        try:
            return obj.get_editor_property(property_name)
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not read any property {property_names} from {obj}: {last_error}"
    )


def set_editor_property(obj, value, *property_names):
    """兼容 Python 风格与 C++ 风格属性名，并写入第一个可编辑属性。"""
    last_error = None
    for property_name in property_names:
        try:
            obj.set_editor_property(property_name, value)
            return
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not set any property {property_names} on {obj}: {last_error}"
    )


def validate_source_animation(source_animation):
    """确认新动画与项目现有 Manny Montage 使用同一 Skeleton。"""
    reference_montage = require_asset(REFERENCE_MONTAGE_PATH, unreal.AnimMontage)
    source_skeleton = get_editor_property(source_animation, "skeleton", "Skeleton")
    reference_skeleton = get_editor_property(reference_montage, "skeleton", "Skeleton")

    if source_skeleton != reference_skeleton:
        raise RuntimeError(
            "Skeleton mismatch: "
            f"{SOURCE_ANIMATION_PATH} uses {source_skeleton.get_path_name()}, but "
            f"the current dash montage uses {reference_skeleton.get_path_name()}."
        )

    root_motion_enabled = bool(
        get_editor_property(source_animation, "enable_root_motion", "bEnableRootMotion")
    )
    if root_motion_enabled:
        unreal.log_warning(
            "Source animation Root Motion was enabled. Disabling it because GA_Dash "
            "already owns movement through ApplyRootMotionConstantForce."
        )
        set_editor_property(
            source_animation,
            False,
            "enable_root_motion",
            "bEnableRootMotion",
        )
        unreal.EditorAssetLibrary.save_loaded_asset(source_animation, False)


def create_or_load_montage(source_animation):
    """从闪避 Animation Sequence 创建 Montage；重复执行时复用已有资产。"""
    existing_asset = unreal.EditorAssetLibrary.load_asset(MONTAGE_ASSET_PATH)
    if existing_asset is not None:
        if not isinstance(existing_asset, unreal.AnimMontage):
            raise RuntimeError(
                f"{MONTAGE_ASSET_PATH} already exists but is not an AnimMontage."
            )
        unreal.log(f"Reusing existing montage: {MONTAGE_ASSET_PATH}")
        return existing_asset

    factory = unreal.AnimMontageFactory()
    set_editor_property(factory, source_animation, "source_animation", "SourceAnimation")

    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MONTAGE_NAME,
        MONTAGE_PACKAGE_PATH,
        unreal.AnimMontage,
        factory,
    )
    if montage is None:
        raise RuntimeError(f"Failed to create montage: {MONTAGE_ASSET_PATH}")

    # AnimMontageFactory creates the first track as DefaultSlot and a Default section.
    unreal.EditorAssetLibrary.save_loaded_asset(montage, False)
    unreal.log(f"Created montage: {MONTAGE_ASSET_PATH}")
    return montage


def configure_dash_ability(montage):
    """把新 Montage 和压缩后的播放速度写入 GA_Dash 类默认值。"""
    ga_dash_blueprint = require_asset(GA_DASH_PATH, unreal.Blueprint)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(GA_DASH_PATH)
    if generated_class is None:
        raise RuntimeError(f"Could not load generated class for {GA_DASH_PATH}")

    class_defaults = unreal.get_default_object(generated_class)
    set_editor_property(class_defaults, montage, "dash_montage", "DashMontage")
    set_editor_property(
        class_defaults,
        MONTAGE_PLAY_RATE,
        "dash_montage_play_rate",
        "DashMontagePlayRate",
    )
    set_editor_property(
        class_defaults,
        unreal.Name("Default"),
        "dash_montage_start_section",
        "DashMontageStartSection",
    )

    unreal.EditorAssetLibrary.save_loaded_asset(ga_dash_blueprint, False)
    unreal.log(
        f"Configured {GA_DASH_PATH}: montage={MONTAGE_ASSET_PATH}, "
        f"play_rate={MONTAGE_PLAY_RATE}, start_section=Default"
    )


def main():
    """执行资源校验、Montage 创建和技能蓝图配置。"""
    with unreal.ScopedSlowTask(3, "Setting up Wukong Manny dash animation") as task:
        task.make_dialog(True)

        source_animation = require_asset(SOURCE_ANIMATION_PATH, unreal.AnimSequence)
        validate_source_animation(source_animation)
        task.enter_progress_frame(1, "Validated Manny skeleton and Root Motion")

        montage = create_or_load_montage(source_animation)
        task.enter_progress_frame(1, "Created dash montage")

        configure_dash_ability(montage)
        task.enter_progress_frame(1, "Configured GA_Dash")

    unreal.log("Wukong dash setup completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Wukong dash setup failed: {error}")
        raise
