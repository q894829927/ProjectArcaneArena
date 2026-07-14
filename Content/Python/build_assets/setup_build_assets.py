"""预检并生成全部已配置的 Fire/Lightning/Overload 构筑资产。"""

import importlib

import unreal


GENERATOR_MODULE_NAMES = (
    "generate_gameplay_effect_blueprints",
    "generate_gameplay_ability_blueprints",
    "generate_upgrade_assets",
    "generate_looping_gameplay_cues",
    "generate_burst_gameplay_cues",
    "configure_build_asset_links",
)


def _load_generator_modules():
    """重新加载分类模块，确保编辑器长会话使用磁盘上的最新配置。"""
    modules = []
    package_name = __package__ or "build_assets"
    for module_name in GENERATOR_MODULE_NAMES:
        module = importlib.import_module(f"{package_name}.{module_name}")
        modules.append(importlib.reload(module))
    return modules


def _configured_asset_paths(configs):
    """从带目标目录和资产名的配置表汇总预期输出路径。"""
    return {
        f"{config['destination_path']}/{config['asset_name']}"
        for config in configs
    }


def _validate_all(modules):
    """在任何写入前验证所有分类配置及跨分类连接引用。"""
    (
        effect_module,
        ability_module,
        upgrade_module,
        looping_cue_module,
        burst_cue_module,
        link_module,
    ) = modules
    effect_module.validate_configs()
    ability_module.validate_configs()

    generated_dependency_paths = _configured_asset_paths(
        effect_module.EFFECT_BLUEPRINT_CONFIGS
    )
    generated_dependency_paths.update(
        _configured_asset_paths(ability_module.ABILITY_BLUEPRINT_CONFIGS)
    )
    upgrade_module.validate_configs(generated_dependency_paths)
    looping_cue_module.validate_configs()
    burst_cue_module.validate_configs()

    generated_asset_paths = set(generated_dependency_paths)
    generated_asset_paths.update(
        _configured_asset_paths(upgrade_module.UPGRADE_CONFIGS)
    )
    generated_asset_paths.update(
        _configured_asset_paths(looping_cue_module.LOOPING_CUE_CONFIGS)
    )
    generated_asset_paths.update(
        _configured_asset_paths(burst_cue_module.BURST_CUE_CONFIGS)
    )
    link_module.validate_configs(generated_asset_paths)


def main():
    """预检并按依赖顺序执行全部构筑资产生成器。"""
    modules = _load_generator_modules()
    _validate_all(modules)

    with unreal.ScopedSlowTask(6, "Generating Project Arcane Arena build assets") as task:
        task.make_dialog(True)
        for module, progress_text in zip(
            modules,
            (
                "Generating GameplayEffect Blueprints",
                "Generating GameplayAbility Blueprints",
                "Generating Upgrade DataAssets",
                "Generating looping GameplayCues",
                "Generating Burst GameplayCues",
                "Connecting Abilities and UpgradePool",
            ),
        ):
            task.enter_progress_frame(1, progress_text)
            module.run()

    unreal.log("Fire/Lightning/Overload build asset setup completed successfully.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Fire/Lightning/Overload build asset setup failed: {error}")
        raise
