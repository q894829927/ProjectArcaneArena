from pathlib import Path

import unreal


PROJECT_DIR = Path(unreal.Paths.project_dir())
SOURCE_DIR = PROJECT_DIR / "Content" / "UI" / "UpgradeIcons"
DESTINATION_PATH = "/Game/UI/UpgradeIcons"
ICON_NAMES = (
    "T_Upgrade_AttackPower_Icon",
    "T_Upgrade_MaxHealth_Icon",
    "T_Upgrade_MoveSpeed_Icon",
)


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


# 批量导入、配置并保存三枚升级图标资产。
def main():
    imported_paths = []
    for icon_name in ICON_NAMES:
        texture, asset_path = import_icon(icon_name)
        configure_ui_texture(texture)
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError(f"Failed to save upgrade icon: {asset_path}")
        imported_paths.append(asset_path)

    unreal.log("Upgrade icons imported: " + ", ".join(imported_paths))


if __name__ == "__main__":
    main()
