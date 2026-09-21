"""幂等创建 P0/P1 高密度 Projectile 压力测试 Blueprint。

在完成 C++ 编译并重启 Unreal Editor 后，通过 Tools > Execute Python Script 执行。
脚本只创建/更新测试 Blueprint，不创建、加载、保存或切换地图。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

TEST_ROOT = "/Game/Tests/Projectile"
STRESS_BLUEPRINT_NAME = "BP_ArenaProjectileStressTestActor"
STRESS_BLUEPRINT_PATH = f"{TEST_ROOT}/{STRESS_BLUEPRINT_NAME}"


def _require_native_type():
    """确认 AArenaProjectileStressTestActor 已由当前 Editor 模块加载。"""
    return tools.require_unreal_type("ArenaProjectileStressTestActor")


def _create_or_load_stress_blueprint(parent_class):
    """幂等创建压力测试 Blueprint，并保持 C++ 默认值作为 P0/P1 单一配置基线。"""
    _, generated_class, asset_path = tools.create_or_load_blueprint(
        STRESS_BLUEPRINT_NAME,
        TEST_ROOT,
        parent_class,
    )

    defaults = unreal.get_default_object(generated_class)
    if defaults is None:
        raise RuntimeError(
            f"Failed to resolve defaults for generated class: {generated_class}"
        )

    # P0/P1 首轮参数全部由 C++ 默认值提供，避免 Python 与源码维护两份配置。
    tools.save_asset(asset_path)
    return generated_class, asset_path


def _validate_blueprint(generated_class, asset_path):
    """确认生成类可加载；父类一致性已由 create_or_load_blueprint 内部强校验。"""
    if generated_class is None:
        raise RuntimeError(f"Generated class is invalid: {asset_path}")

    unreal.log(
        "Projectile stress Blueprint ready: "
        f"{asset_path} -> {generated_class.get_path_name()}"
    )


def main():
    """创建并保存 BP_ArenaProjectileStressTestActor。"""
    parent_class = _require_native_type()
    generated_class, asset_path = _create_or_load_stress_blueprint(parent_class)
    _validate_blueprint(generated_class, asset_path)

    unreal.log(
        "P0/P1 stress Blueprint setup completed. "
        "Place BP_ArenaProjectileStressTestActor in an empty test map "
        "and keep its inherited C++ defaults for the first DataPool run."
    )


if __name__ == "__main__":
    main()
