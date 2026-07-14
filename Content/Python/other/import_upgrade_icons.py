from pathlib import Path

import unreal


PROJECT_DIR = Path(unreal.Paths.project_dir())
SOURCE_DIR = PROJECT_DIR / "Content" / "UI" / "UpgradeIcons"
DESTINATION_PATH = "/Game/UI/UpgradeIcons"
ICON_NAMES = (
    "T_Upgrade_AttackPower_Icon",
    "T_Upgrade_MaxHealth_Icon",
    "T_Upgrade_MoveSpeed_Icon",
    "T_Upgrade_FireballDamage_Icon",
    "T_Upgrade_FireballBurning_Icon",
    "T_Upgrade_LightningStormDamage_Icon",
    "T_Upgrade_LightningStormShocked_Icon",
)
UPGRADE_DATA_PATH = "/Game/Data/Upgrade"
ICON_BINDINGS = {
    "DA_Upgrade_AttackPower": "T_Upgrade_AttackPower_Icon",
    "DA_Upgrade_MaxHealth": "T_Upgrade_MaxHealth_Icon",
    "DA_Upgrade_MoveSpeed": "T_Upgrade_MoveSpeed_Icon",
    "DA_Upgrade_FireballDamage": "T_Upgrade_FireballDamage_Icon",
    "DA_Upgrade_FireballBurning": "T_Upgrade_FireballBurning_Icon",
    "DA_Upgrade_LightningStormDamage": "T_Upgrade_LightningStormDamage_Icon",
    "DA_Upgrade_LightningStormShocked": "T_Upgrade_LightningStormShocked_Icon",
}


# 在任何导入写入前验证 PNG 源文件和目标升级 DataAsset，避免执行到中途才留下半套配置。
def validate_inputs():
    for icon_name in ICON_NAMES:
        source_file = SOURCE_DIR / f"{icon_name}.png"
        if not source_file.is_file():
            raise RuntimeError(f"Missing upgrade icon source: {source_file}")

    for upgrade_name in ICON_BINDINGS:
        upgrade_path = f"{UPGRADE_DATA_PATH}/{upgrade_name}"
        if not unreal.EditorAssetLibrary.does_asset_exist(upgrade_path):
            raise RuntimeError(f"Missing upgrade DataAsset: {upgrade_path}")


# 导入单个透明 PNG，并返回生成的 Texture2D 资产。
def import_icon(asset_name: str):
    source_file = SOURCE_DIR / f"{asset_name}.png"
    if not source_file.is_file():
        raise RuntimeError(f"Missing upgrade icon source: {source_file}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", False)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION_PATH}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Texture2D import failed: {asset_path}")
    return texture, asset_path


# 将纹理配置为升级 UI 图标，避免流送和远距离 mip 降低界面清晰度。
def configure_ui_texture(texture: unreal.Texture2D):
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.modify()


# 把导入后的纹理写入对应 Upgrade DataAsset 的 Icon 字段，并保存资产引用。
def configure_upgrade_icons(imported_textures):
    configured_paths = []
    for upgrade_name, icon_name in ICON_BINDINGS.items():
        upgrade_path = f"{UPGRADE_DATA_PATH}/{upgrade_name}"
        upgrade = unreal.EditorAssetLibrary.load_asset(upgrade_path)
        texture = imported_textures.get(icon_name)
        if upgrade is None or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(
                f"Cannot bind upgrade icon: {upgrade_path} <- {icon_name}"
            )

        upgrade.modify()
        upgrade.set_editor_property("icon", texture)
        if not unreal.EditorAssetLibrary.save_asset(
            upgrade_path,
            only_if_is_dirty=False,
        ):
            raise RuntimeError(f"Failed to save upgrade icon binding: {upgrade_path}")
        configured_paths.append(upgrade_path)
    return configured_paths


# 批量导入、配置并保存七枚升级图标，再连接到对应升级 DataAsset。
def run():
    validate_inputs()
    imported_textures = {}
    for icon_name in ICON_NAMES:
        texture, asset_path = import_icon(icon_name)
        configure_ui_texture(texture)
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError(f"Failed to save upgrade icon: {asset_path}")
        imported_textures[icon_name] = texture

    configured_paths = configure_upgrade_icons(imported_textures)
    unreal.log("Upgrade icons imported: " + ", ".join(imported_textures))
    unreal.log("Upgrade icon bindings configured: " + ", ".join(configured_paths))


# 提供可从 Unreal Editor 控制台直接执行的升级图标导入入口。
def main():
    run()

    unreal.log("Upgrade icon setup completed successfully.")


if __name__ == "__main__":
    main()
