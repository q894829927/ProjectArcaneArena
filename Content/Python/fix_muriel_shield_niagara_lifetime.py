"""Fix NS_Shield_Muriel lifecycle through the project Niagara editor bridge.

Run this file from Unreal Editor via Tools > Execute Python Script after building
the ProjectArcaneArenaEditor module.
"""

import unreal


TARGET_SYSTEM_PATH = "/Game/Niagara/NS_Shield_Muriel"
LOOPING_REFERENCE_PATH = "/Game/Niagara/NS_ShieldAura"
BACKUP_SYSTEM_PATH = "/Game/Niagara/NS_Shield_Muriel_BeforeLifetimeFix"


def require_asset(asset_path, expected_class=None):
    """加载必需资产，并在缺失或类型不匹配时停止修改。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Required asset was not found: {asset_path}")
    if expected_class is not None and not isinstance(asset, expected_class):
        raise RuntimeError(
            f"Asset {asset_path} is {asset.get_class().get_name()}, "
            f"expected {expected_class.__name__}."
        )
    return asset


def create_backup():
    """首次执行时备份转换后的原始 Muriel Niagara System。"""
    if unreal.EditorAssetLibrary.does_asset_exist(BACKUP_SYSTEM_PATH):
        unreal.log(f"Backup already exists: {BACKUP_SYSTEM_PATH}")
        return

    backup = unreal.EditorAssetLibrary.duplicate_asset(
        TARGET_SYSTEM_PATH,
        BACKUP_SYSTEM_PATH,
    )
    if backup is None:
        raise RuntimeError(f"Failed to create backup: {BACKUP_SYSTEM_PATH}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(backup, False):
        raise RuntimeError(f"Failed to save backup: {BACKUP_SYSTEM_PATH}")
    unreal.log(f"Created backup: {BACKUP_SYSTEM_PATH}")


def call_editor_bridge(target_system, reference_system):
    """调用 Editor C++ 桥接层修改 Python 无法访问的 Niagara Stack。"""
    bridge = getattr(unreal, "ArenaNiagaraEditorLibrary", None)
    if bridge is None:
        raise RuntimeError(
            "ArenaNiagaraEditorLibrary is unavailable. Close Unreal Editor, build "
            "ProjectArcaneArenaEditor, and restart the editor before running this script."
        )

    result = bridge.copy_looping_lifecycle_from_reference(
        target_system,
        reference_system,
    )
    if isinstance(result, tuple):
        success = bool(result[0])
        report = str(result[1]) if len(result) > 1 else ""
    else:
        success = bool(result)
        report = ""

    if not success:
        raise RuntimeError(f"Niagara editor bridge failed: {report}")
    return report


def open_target_editor(target_system):
    """打开修复后的 Niagara，方便立即预览和确认编译结果。"""
    try:
        subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        subsystem.open_editor_for_assets([target_system])
    except Exception as error:
        unreal.log_warning(f"Could not open Niagara editor automatically: {error}")


def main():
    """备份、修复、保存并打开 Muriel 护盾 Niagara。"""
    target_system = require_asset(TARGET_SYSTEM_PATH, unreal.NiagaraSystem)
    reference_system = require_asset(LOOPING_REFERENCE_PATH, unreal.NiagaraSystem)
    create_backup()

    report = call_editor_bridge(target_system, reference_system)
    if not unreal.EditorAssetLibrary.save_loaded_asset(target_system, False):
        raise RuntimeError(f"Failed to save {TARGET_SYSTEM_PATH}")

    open_target_editor(target_system)
    unreal.log(f"Muriel shield Niagara lifetime fix completed. {report}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Muriel shield Niagara lifetime fix failed: {error}")
        raise
