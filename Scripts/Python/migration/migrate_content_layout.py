"""Project Arcane Arena Content 目录迁移工具。

默认只做 Dry Run，不修改任何资产。

推荐流程：
1. 先提交/备份当前 Git 工作区。
2. 保持 RUN_MODE = "dry_run"，在 Unreal Editor 中执行一次，检查 Saved/MigrationReports 输出。
3. 确认目标路径和冲突为 0 后，把 ALLOW_APPLY 改为 True，RUN_MODE 改为 "all"。
4. 迁移地图前不要让待迁移地图成为当前正在编辑的唯一打开地图；如果地图移动失败，打开其他地图后重跑即可。
5. 脚本不会自动 Fix Up Redirectors。完成重启 Editor、窄目标编译、PIE/关键地图验证后，再按迁移文档清理 Redirector。

重要：
- .uasset/.umap 只通过 Unreal EditorAssetLibrary.rename_asset() 移动。
- __ExternalActors__ / __ExternalObjects__ 从不手工移动，由 Unreal 随地图管理。
- World 的 PersistentLevel 等子对象不会作为独立资产迁移，会折叠到所属 .umap Package。
- __pycache__ / .pyc 属本地缓存，不移动到 Scripts/Python。
- 第三方目录和明确保留目录从不迁移。
"""

from __future__ import annotations

import datetime
import filecmp
import pathlib
import shutil
import traceback

import unreal


# -----------------------------------------------------------------------------
# 用户配置
# -----------------------------------------------------------------------------

# 可选：dry_run / apply / rewrite / validate / all
RUN_MODE = "dry_run"

# apply / all 必须显式改为 True，避免误触发真实迁移。
ALLOW_APPLY = False

# 是否移动 Content 中非 .uasset/.umap 的项目自有辅助文件，例如 UI 图标源 PNG、Content/Python 脚本。
MOVE_SUPPORT_FILES = True

# 遇到未分类的 Unreal 资产时，apply/all 是否直接停止。
ABORT_ON_UNKNOWN_ASSET = True

# 迁移后是否尝试保存每个目标资产。
SAVE_MOVED_ASSETS = True


PROJECT_NAMESPACE = "/Game/ProjectArcaneArena"

EXCLUDED_ROOTS = (
    "/Game/SlashTrail_SoftTofu",
    "/Game/ParagonMuriel",
    "/Game/CombatMagicAnims",
    "/Game/wukongManny",
    "/Game/Characters/Mannequins",
)

KEEP_ROOTS = (
    "/Game/Stylized_Spruce_Forest",
    "/Game/LevelPrototyping",
)

UE_MANAGED_ROOTS = (
    "/Game/__ExternalActors__",
    "/Game/__ExternalObjects__",
)

OLD_ASSET_ROOTS_TO_CLEAR = (
    "/Game/GameMode",
    "/Game/Characters/ArenaPlayer",
    "/Game/Characters/ArenaEnemy",
    "/Game/Boss",
    "/Game/GAS",
    "/Game/Data/Upgrade",
    "/Game/Data/Weapon",
    "/Game/Data/EnemyAffix",
    "/Game/Data/Pickup",
    "/Game/Blueprints",
    "/Game/UI",
    "/Game/TopDown",
    "/Game/Niagara",
    "/Game/Mass",
    "/Game/Tests",
    "/Game/Cursor",
    "/Game/Core",
    "/Game/Items",
    "/Game/Assets/Pickups",
)

OLD_TEXT_TOKENS = (
    "/Game/GameMode",
    "/Game/Characters/ArenaPlayer",
    "/Game/Characters/ArenaEnemy",
    "/Game/Boss",
    "/Game/GAS",
    "/Game/Data/Upgrade",
    "/Game/Data/Weapon",
    "/Game/Data/EnemyAffix",
    "/Game/Data/Pickup",
    "/Game/Blueprints/ArenaLightningStormArea",
    "/Game/Blueprints/DataAsset",
    "/Game/UI",
    "/Game/TopDown",
    "/Game/Niagara",
    "/Game/Mass",
    "/Game/Tests",
    "/Game/Cursor",
    "/Game/Core/BP_ArenaPlayerController",
    "/Game/Items",
    "/Game/Assets/Pickups",
    "/Game/Projectile/VFX",
)

REPORT_LINES: list[str] = []


# -----------------------------------------------------------------------------
# 日志
# -----------------------------------------------------------------------------

def _log(message: str) -> None:
    """同时写入 Unreal 日志和迁移报告。"""
    REPORT_LINES.append(message)
    unreal.log(f"[ArenaMigration] {message}")


def _warn(message: str) -> None:
    """同时写入 Unreal Warning 和迁移报告。"""
    REPORT_LINES.append(f"WARNING: {message}")
    unreal.log_warning(f"[ArenaMigration] {message}")


def _error(message: str) -> None:
    """同时写入 Unreal Error 和迁移报告。"""
    REPORT_LINES.append(f"ERROR: {message}")
    unreal.log_error(f"[ArenaMigration] {message}")


def _write_report() -> pathlib.Path:
    """把本次执行日志写到 Saved/MigrationReports。"""
    saved_dir = pathlib.Path(unreal.Paths.project_saved_dir())
    report_dir = saved_dir / "MigrationReports"
    report_dir.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    report_path = report_dir / f"content_migration_{timestamp}_{RUN_MODE}.log"
    report_path.write_text("\n".join(REPORT_LINES) + "\n", encoding="utf-8")
    unreal.log(f"[ArenaMigration] Report: {report_path}")
    return report_path


# -----------------------------------------------------------------------------
# 路径辅助
# -----------------------------------------------------------------------------

def _canonical_asset_path(raw_path: str) -> str:
    """把 ObjectPath/SubobjectPath 统一成唯一 PackagePath。

    例如：
    /Game/Map/Lvl.Lvl -> /Game/Map/Lvl
    /Game/Map/Lvl.Lvl:PersistentLevel -> /Game/Map/Lvl

    Asset Registry 在部分 World 上会同时返回 World 与 PersistentLevel 子对象；
    迁移计划必须把它们折叠到同一个 Package，避免同一 .umap 被计划移动两次。
    """
    path = str(raw_path).replace("\\", "/")

    if ":" in path:
        path = path.split(":", 1)[0]

    leaf = path.rsplit("/", 1)[-1]
    if "." in leaf:
        path = path.rsplit(".", 1)[0]

    return path.rstrip("/")


def _starts_with_root(path: str, root: str) -> bool:
    """判断路径是否等于指定根或位于根目录下。"""
    return path == root or path.startswith(root + "/")


def _asset_name(asset_path: str) -> str:
    """取得资产包名最后一段。"""
    return asset_path.rsplit("/", 1)[-1]


def _replace_prefix(path: str, old_prefix: str, new_prefix: str) -> str:
    """仅替换已确认匹配的 Unreal 虚拟路径前缀。"""
    return new_prefix + path[len(old_prefix):]


def _is_redirector(asset_path: str) -> bool:
    """识别旧路径是否已经只是 ObjectRedirector。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        return False

    asset_class = asset.get_class()
    return bool(asset_class and asset_class.get_name() == "ObjectRedirector")


def _is_world_asset(asset_path: str) -> bool:
    """识别 World/Map，使地图最后迁移，降低引用更新时的干扰。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        return False

    asset_class = asset.get_class()
    return bool(asset_class and asset_class.get_name() in {"World", "Level"})


def _ensure_virtual_directory(asset_path: str) -> None:
    """为目标资产创建 Content Browser 目录。"""
    directory = asset_path.rsplit("/", 1)[0]
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        if not unreal.EditorAssetLibrary.make_directory(directory):
            raise RuntimeError(f"Failed to create Unreal directory: {directory}")


def _rename_unreal_asset(source: str, target: str) -> bool:
    """按普通路径、已加载资产、AssetTools 三层方式尝试迁移资产。

    部分 Blueprint（尤其当前 World/GameMode 正在引用的 Blueprint）在
    EditorAssetLibrary.rename_asset() 下可能返回 False。此时先对已加载 UObject
    使用 rename_loaded_asset()；仍失败再交给 AssetTools RenameAssets。
    所有方式仍通过 Unreal Editor API 执行，不直接移动 .uasset/.umap。
    """
    if unreal.EditorAssetLibrary.rename_asset(source, target):
        return True

    _warn(
        f"rename_asset returned False, trying loaded-asset fallback: "
        f"{source} -> {target}"
    )

    asset = unreal.EditorAssetLibrary.load_asset(source)
    if asset is None:
        _error(f"Fallback cannot load source asset: {source}")
        return False

    try:
        if unreal.EditorAssetLibrary.rename_loaded_asset(asset, target):
            _log(f"[FALLBACK] rename_loaded_asset succeeded: {source} -> {target}")
            return True
    except Exception as exc:
        _warn(f"rename_loaded_asset raised for {source}: {exc}")

    _warn(
        f"rename_loaded_asset failed, trying AssetTools fallback: "
        f"{source} -> {target}"
    )

    try:
        package_path, new_name = target.rsplit("/", 1)
        rename_data = unreal.AssetRenameData()
        rename_data.set_editor_property("asset", asset)
        rename_data.set_editor_property("new_package_path", package_path)
        rename_data.set_editor_property("new_name", new_name)

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        if asset_tools.rename_assets([rename_data]):
            _log(f"[FALLBACK] AssetTools rename_assets succeeded: {source} -> {target}")
            return True
    except Exception as exc:
        _warn(f"AssetTools rename fallback raised for {source}: {exc}")

    return False


# -----------------------------------------------------------------------------
# Unreal 资产分类
# -----------------------------------------------------------------------------

def classify_asset(source: str) -> tuple[str, str | None]:
    """按照迁移文档把旧资产映射到最终目录。

    返回：
    - ("move", target)
    - ("already_target", None)
    - ("excluded", None)
    - ("keep", None)
    - ("ue_managed", None)
    - ("review", None)
    """
    source = _canonical_asset_path(source)

    if _starts_with_root(source, PROJECT_NAMESPACE):
        return "already_target", None

    if any(_starts_with_root(source, root) for root in EXCLUDED_ROOTS):
        return "excluded", None

    if any(_starts_with_root(source, root) for root in KEEP_ROOTS):
        return "keep", None

    if any(_starts_with_root(source, root) for root in UE_MANAGED_ROOTS):
        return "ue_managed", None

    if source.startswith("/Game/Assets/Pickups/"):
        return "move", _replace_prefix(
            source,
            "/Game/Assets/Pickups",
            f"{PROJECT_NAMESPACE}/Systems/Pickups/Art",
        )

    if source.startswith("/Game/Blueprints/ArenaLightningStormArea/"):
        return "move", _replace_prefix(
            source,
            "/Game/Blueprints/ArenaLightningStormArea",
            f"{PROJECT_NAMESPACE}/Combat/Areas/LightningStorm",
        )

    if source.startswith("/Game/Blueprints/DataAsset/"):
        return "move", _replace_prefix(
            source,
            "/Game/Blueprints/DataAsset",
            f"{PROJECT_NAMESPACE}/Systems/Waves/Data",
        )

    if source.startswith("/Game/Boss/"):
        return "move", _replace_prefix(
            source,
            "/Game/Boss",
            f"{PROJECT_NAMESPACE}/Characters/Boss",
        )

    if source.startswith("/Game/Characters/ArenaEnemy/"):
        return "move", _replace_prefix(
            source,
            "/Game/Characters/ArenaEnemy",
            f"{PROJECT_NAMESPACE}/Characters/Enemies",
        )

    if source.startswith("/Game/Characters/ArenaPlayer/"):
        return "move", _replace_prefix(
            source,
            "/Game/Characters/ArenaPlayer",
            f"{PROJECT_NAMESPACE}/Characters/Player",
        )

    if source == "/Game/Core/BP_ArenaPlayerController":
        return "move", f"{PROJECT_NAMESPACE}/Core/Controllers/BP_ArenaPlayerController"

    if source.startswith("/Game/Cursor/"):
        return "move", _replace_prefix(
            source,
            "/Game/Cursor",
            f"{PROJECT_NAMESPACE}/UI/Cursor/Current",
        )

    if source.startswith("/Game/Data/EnemyAffix/"):
        return "move", _replace_prefix(
            source,
            "/Game/Data/EnemyAffix",
            f"{PROJECT_NAMESPACE}/Characters/Enemies/Data/Affixes",
        )

    if source.startswith("/Game/Data/Pickup/"):
        return "move", _replace_prefix(
            source,
            "/Game/Data/Pickup",
            f"{PROJECT_NAMESPACE}/Systems/Pickups/Data",
        )

    if source.startswith("/Game/Data/Upgrade/"):
        return "move", _replace_prefix(
            source,
            "/Game/Data/Upgrade",
            f"{PROJECT_NAMESPACE}/Systems/Upgrades/Data",
        )

    if source.startswith("/Game/Data/Weapon/"):
        return "move", _replace_prefix(
            source,
            "/Game/Data/Weapon",
            f"{PROJECT_NAMESPACE}/Combat/Weapons/Data",
        )

    if source.startswith("/Game/GAS/Area/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/Area",
            f"{PROJECT_NAMESPACE}/Combat/Areas/Dash",
        )

    if source.startswith("/Game/GAS/DamageFeedback/"):
        name = _asset_name(source)
        if name.startswith("CS_"):
            bucket = "Camera"
        elif name.startswith("M_") or name.startswith("MI_"):
            bucket = "Materials"
        elif name.startswith("SA_") or name.startswith("S_"):
            bucket = "Audio"
        else:
            bucket = "Misc"

        return "move", f"{PROJECT_NAMESPACE}/Combat/Feedback/{bucket}/{name}"

    if source.startswith("/Game/GAS/GameplayAbility/"):
        name = _asset_name(source)
        if name.startswith("GA_Enemy"):
            bucket = "Enemy"
        elif name.startswith("GA_EnergyOn"):
            bucket = "Triggers"
        else:
            bucket = "Player"

        return "move", f"{PROJECT_NAMESPACE}/Combat/GAS/Abilities/{bucket}/{name}"

    if source.startswith("/Game/GAS/GameplayCues/DurationCue/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/GameplayCues/DurationCue",
            f"{PROJECT_NAMESPACE}/Combat/GAS/Cues/Looping",
        )

    if source.startswith("/Game/GAS/GameplayCues/InstaneCue/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/GameplayCues/InstaneCue",
            f"{PROJECT_NAMESPACE}/Combat/GAS/Cues/Instant",
        )

    if source.startswith("/Game/GAS/GameplayCues/Elite/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/GameplayCues/Elite",
            f"{PROJECT_NAMESPACE}/Combat/GAS/Cues/Elite",
        )

    if source.startswith("/Game/GAS/GameplayEffect/Status/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/GameplayEffect/Status",
            f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Status",
        )

    if source.startswith("/Game/GAS/GameplayEffect/Trigger/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/GameplayEffect/Trigger",
            f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Triggers",
        )

    if source.startswith("/Game/GAS/GameplayEffect/Upgrade/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/GameplayEffect/Upgrade",
            f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Upgrades",
        )

    if source.startswith("/Game/GAS/GameplayEffect/"):
        name = _asset_name(source)
        if name.startswith("GE_Cooldown_"):
            bucket = "Cooldowns"
        elif name.startswith("GE_Cost_"):
            bucket = "Costs"
        elif name.startswith("GE_Init_"):
            bucket = "Init"
        elif name.startswith("GE_Status_"):
            bucket = "Status"
        elif name.startswith("GE_Elite_"):
            return "move", f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Enemy/Elite/{name}"
        else:
            bucket = "Core"

        return "move", f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/{bucket}/{name}"

    if source.startswith("/Game/GAS/Projectile/"):
        return "move", _replace_prefix(
            source,
            "/Game/GAS/Projectile",
            f"{PROJECT_NAMESPACE}/Combat/Projectiles/Actors",
        )

    if source.startswith("/Game/GameMode/"):
        return "move", _replace_prefix(
            source,
            "/Game/GameMode",
            f"{PROJECT_NAMESPACE}/Core/GameMode",
        )

    if source.startswith("/Game/Items/Inventory/"):
        return "move", _replace_prefix(
            source,
            "/Game/Items/Inventory",
            f"{PROJECT_NAMESPACE}/Systems/Inventory",
        )

    if source.startswith("/Game/Items/Pickups/"):
        return "move", _replace_prefix(
            source,
            "/Game/Items/Pickups",
            f"{PROJECT_NAMESPACE}/Systems/Pickups/Blueprints",
        )

    if source.startswith("/Game/Mass/"):
        return "move", _replace_prefix(
            source,
            "/Game/Mass",
            f"{PROJECT_NAMESPACE}/Dev/Experiments/Mass",
        )

    if source.startswith("/Game/Niagara/"):
        return "move", _replace_prefix(
            source,
            "/Game/Niagara",
            f"{PROJECT_NAMESPACE}/VFX/Common",
        )

    if source.startswith("/Game/Tests/"):
        return "move", _replace_prefix(
            source,
            "/Game/Tests",
            f"{PROJECT_NAMESPACE}/Dev/Tests",
        )

    if source.startswith("/Game/TopDown/Blueprints/"):
        return "move", _replace_prefix(
            source,
            "/Game/TopDown/Blueprints",
            f"{PROJECT_NAMESPACE}/Dev/LegacyTemplate/TopDown/Blueprints",
        )

    if source.startswith("/Game/TopDown/Cursor/"):
        return "move", _replace_prefix(
            source,
            "/Game/TopDown/Cursor",
            f"{PROJECT_NAMESPACE}/Dev/LegacyTemplate/TopDown/Cursor",
        )

    if source.startswith("/Game/TopDown/Input/Actions/"):
        return "move", _replace_prefix(
            source,
            "/Game/TopDown/Input/Actions",
            f"{PROJECT_NAMESPACE}/Input/Actions",
        )

    if source.startswith("/Game/TopDown/Input/"):
        return "move", _replace_prefix(
            source,
            "/Game/TopDown/Input",
            f"{PROJECT_NAMESPACE}/Input/Contexts",
        )

    if source == "/Game/TopDown/Lvl_TopDown":
        return "move", f"{PROJECT_NAMESPACE}/World/Maps/Lvl_Arena"

    if source == "/Game/TopDown/MI_Colorway":
        return "move", f"{PROJECT_NAMESPACE}/World/Materials/MI_Colorway"

    if source.startswith("/Game/UI/MainMenu/"):
        return "move", _replace_prefix(
            source,
            "/Game/UI/MainMenu",
            f"{PROJECT_NAMESPACE}/UI/MainMenu",
        )

    if source.startswith("/Game/UI/Inventory/"):
        return "move", _replace_prefix(
            source,
            "/Game/UI/Inventory",
            f"{PROJECT_NAMESPACE}/UI/Inventory",
        )

    if source.startswith("/Game/UI/UpgradeIcons/"):
        return "move", _replace_prefix(
            source,
            "/Game/UI/UpgradeIcons",
            f"{PROJECT_NAMESPACE}/UI/Upgrade/Icons",
        )

    if source in {
        "/Game/UI/BP_ArenaDamageNumberActor",
        "/Game/UI/WBP_DamageNumber",
    }:
        return "move", f"{PROJECT_NAMESPACE}/UI/DamageNumbers/{_asset_name(source)}"

    if source == "/Game/UI/WBP_EnemyHealthBar":
        return "move", f"{PROJECT_NAMESPACE}/UI/HUD/Enemy/WBP_EnemyHealthBar"

    if source == "/Game/UI/WBP_PlayerHUD":
        return "move", f"{PROJECT_NAMESPACE}/UI/HUD/WBP_PlayerHUD"

    if source.startswith("/Game/UI/"):
        return "move", _replace_prefix(
            source,
            "/Game/UI",
            f"{PROJECT_NAMESPACE}/UI",
        )

    return "review", None


def build_asset_plan() -> dict:
    """扫描当前 Editor Asset Registry，形成可重复执行的迁移计划。"""
    raw_assets = unreal.EditorAssetLibrary.list_assets(
        "/Game",
        recursive=True,
        include_folder=False,
    )

    result = {
        "moves": [],
        "already_moved": [],
        "conflicts": [],
        "review": [],
        "excluded": [],
        "keep": [],
        "ue_managed": [],
        "already_target": [],
        "all_pairs": [],
    }

    seen_sources: set[str] = set()

    for raw_path in raw_assets:
        source = _canonical_asset_path(raw_path)
        if source in seen_sources:
            continue
        seen_sources.add(source)

        kind, target = classify_asset(source)

        if kind == "move":
            assert target is not None
            result["all_pairs"].append((source, target))

            source_exists = unreal.EditorAssetLibrary.does_asset_exist(source)
            target_exists = unreal.EditorAssetLibrary.does_asset_exist(target)

            if target_exists:
                if source_exists and _is_redirector(source):
                    result["already_moved"].append((source, target))
                elif source_exists:
                    result["conflicts"].append((source, target))
                else:
                    result["already_moved"].append((source, target))
            elif source_exists and not _is_redirector(source):
                result["moves"].append((source, target))
            elif source_exists and _is_redirector(source):
                result["review"].append(
                    f"Redirector without target: {source} -> {target}"
                )
            else:
                result["review"].append(
                    f"Source and target both missing: {source} -> {target}"
                )
        elif kind == "review":
            result["review"].append(source)
        else:
            result[kind].append(source)

    return result


# -----------------------------------------------------------------------------
# Unreal 资产迁移
# -----------------------------------------------------------------------------

def print_asset_plan(plan: dict) -> None:
    """输出迁移计划、冲突和未知资产。"""
    _log(
        "Asset plan: "
        f"move={len(plan['moves'])}, "
        f"already_moved={len(plan['already_moved'])}, "
        f"conflict={len(plan['conflicts'])}, "
        f"review={len(plan['review'])}, "
        f"excluded={len(plan['excluded'])}, "
        f"keep={len(plan['keep'])}, "
        f"ue_managed={len(plan['ue_managed'])}, "
        f"already_target={len(plan['already_target'])}"
    )

    for source, target in plan["moves"]:
        _log(f"[MOVE] {source} -> {target}")

    for source, target in plan["already_moved"]:
        _log(f"[ALREADY] {source} -> {target}")

    for source, target in plan["conflicts"]:
        _error(f"[CONFLICT] source and target both exist: {source} -> {target}")

    for item in plan["review"]:
        _warn(f"[REVIEW] {item}")


def apply_asset_moves(plan: dict) -> None:
    """使用 Unreal API 迁移资产；地图最后处理。"""
    if plan["conflicts"]:
        raise RuntimeError(
            f"Found {len(plan['conflicts'])} asset path conflicts. "
            "Resolve them before apply."
        )

    if plan["review"] and ABORT_ON_UNKNOWN_ASSET:
        raise RuntimeError(
            f"Found {len(plan['review'])} review items. "
            "Resolve them or set ABORT_ON_UNKNOWN_ASSET=False."
        )

    moves = list(plan["moves"])
    moves.sort(key=lambda pair: (_is_world_asset(pair[0]), pair[0]))

    for index, (source, target) in enumerate(moves, start=1):
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            if unreal.EditorAssetLibrary.does_asset_exist(source) and _is_redirector(source):
                _log(f"[{index}/{len(moves)}] already moved: {target}")
                continue
            raise RuntimeError(f"Target already exists: {target}")

        if not unreal.EditorAssetLibrary.does_asset_exist(source):
            _warn(f"[{index}/{len(moves)}] source disappeared, skip: {source}")
            continue

        _ensure_virtual_directory(target)
        _log(f"[{index}/{len(moves)}] rename_asset: {source} -> {target}")

        if not _rename_unreal_asset(source, target):
            raise RuntimeError(
                f"All Unreal rename methods failed: {source} -> {target}. "
                "Close any Blueprint/Map editors that have this asset open, "
                "open a neutral map, then rerun; already completed moves are idempotent."
            )

        if SAVE_MOVED_ASSETS:
            if not unreal.EditorAssetLibrary.save_asset(
                target,
                only_if_is_dirty=False,
            ):
                raise RuntimeError(f"Failed to save moved asset: {target}")


# -----------------------------------------------------------------------------
# Content 中非 Unreal 资产文件
# -----------------------------------------------------------------------------

def map_support_file(relative_path: str) -> str | None:
    """给 Content 内的普通文件规划物理目标；绝不用于 .uasset/.umap。"""
    rel = relative_path.replace("\\", "/")

    if rel == "mcp-conversation-export.md":
        return "../Docs/Archive/mcp-conversation-export.md"

    if rel.startswith("Python/"):
        return "../Scripts/Python/" + rel[len("Python/"):]

    if rel.startswith("Stylized_Spruce_Forest/"):
        return None

    if rel.startswith("LevelPrototyping/"):
        return None

    excluded_content_roots = (
        "SlashTrail_SoftTofu/",
        "ParagonMuriel/",
        "CombatMagicAnims/",
        "wukongManny/",
        "Characters/Mannequins/",
    )
    if rel.startswith(excluded_content_roots):
        return None

    if rel.startswith("UI/UpgradeIcons/"):
        # 原始 PNG 属 Source Art，不应继续留在 Content 触发 Auto Reimport。
        return (
            "../SourceArt/UI/UpgradeIcons/"
            + rel[len("UI/UpgradeIcons/"):]
        )

    if rel.startswith("ProjectArcaneArena/UI/Upgrade/Icons/"):
        # 修复早期迁移脚本已经把部分 PNG 搬进新 Content 目录的中间状态。
        return (
            "../SourceArt/UI/UpgradeIcons/"
            + rel[len("ProjectArcaneArena/UI/Upgrade/Icons/"):]
        )

    if rel.startswith("Assets/Pickups/"):
        return (
            "ProjectArcaneArena/Systems/Pickups/Art/"
            + rel[len("Assets/Pickups/"):]
        )

    if rel.startswith("Boss/"):
        return (
            "ProjectArcaneArena/Characters/Boss/"
            + rel[len("Boss/"):]
        )

    if rel.startswith("Characters/ArenaEnemy/"):
        return (
            "ProjectArcaneArena/Characters/Enemies/"
            + rel[len("Characters/ArenaEnemy/"):]
        )

    if rel.startswith("Characters/ArenaPlayer/"):
        return (
            "ProjectArcaneArena/Characters/Player/"
            + rel[len("Characters/ArenaPlayer/"):]
        )

    if rel.startswith("Data/Upgrade/"):
        return (
            "ProjectArcaneArena/Systems/Upgrades/Data/"
            + rel[len("Data/Upgrade/"):]
        )

    if rel.startswith("Data/Weapon/"):
        return (
            "ProjectArcaneArena/Combat/Weapons/Data/"
            + rel[len("Data/Weapon/"):]
        )

    if rel.startswith("Data/EnemyAffix/"):
        return (
            "ProjectArcaneArena/Characters/Enemies/Data/Affixes/"
            + rel[len("Data/EnemyAffix/"):]
        )

    if rel.startswith("Data/Pickup/"):
        return (
            "ProjectArcaneArena/Systems/Pickups/Data/"
            + rel[len("Data/Pickup/"):]
        )

    if rel.startswith("UI/"):
        return "ProjectArcaneArena/UI/" + rel[len("UI/"):]

    return None


def build_support_file_plan() -> tuple[list[tuple[pathlib.Path, pathlib.Path]], list[str]]:
    """扫描 Content 内的 Python、文档和源图片等普通文件。"""
    project_dir = pathlib.Path(unreal.Paths.project_dir()).resolve()
    content_dir = project_dir / "Content"

    moves: list[tuple[pathlib.Path, pathlib.Path]] = []
    review: list[str] = []

    for source in content_dir.rglob("*"):
        if not source.is_file():
            continue

        if source.suffix.lower() in {".uasset", ".umap"}:
            continue

        rel = source.relative_to(content_dir).as_posix()

        # __pycache__ / .pyc 是本地 Python 运行缓存，不属于迁移源文件。
        # 它们既不进入 Scripts/Python，也不参与 Git/Content 资产规划。
        if source.suffix.lower() == ".pyc" or "__pycache__" in source.parts:
            continue

        if rel == "3":
            review.append(
                "Content/3 is a large extensionless file and remains untouched."
            )
            continue

        mapped = map_support_file(rel)
        if not mapped:
            continue

        if mapped.startswith("../"):
            target = (content_dir / mapped).resolve()
        else:
            target = content_dir / mapped

        moves.append((source, target))

    return moves, review


def apply_support_file_moves(
    support_moves: list[tuple[pathlib.Path, pathlib.Path]],
) -> None:
    """只移动普通文本/图片源文件，不直接操作 Unreal 二进制资产包。"""
    for index, (source, target) in enumerate(support_moves, start=1):
        if source.suffix.lower() in {".uasset", ".umap"}:
            raise RuntimeError(
                f"Refusing filesystem move for Unreal package: {source}"
            )

        if target.exists():
            if source.resolve() == target.resolve():
                continue

            # 中断续跑时可能已经有同内容目标文件。相同则删除重复源并继续；
            # 内容不同才是真冲突，避免无提示覆盖用户文件。
            if filecmp.cmp(source, target, shallow=False):
                _log(
                    f"[FILE ALREADY] identical target exists; "
                    f"remove duplicate source: {source} -> {target}"
                )
                source.unlink()
                continue

            raise RuntimeError(
                f"Support-file target conflict with different content: "
                f"{source} -> {target}"
            )

        target.parent.mkdir(parents=True, exist_ok=True)
        _log(
            f"[FILE {index}/{len(support_moves)}] "
            f"{source} -> {target}"
        )
        shutil.move(str(source), str(target))


# -----------------------------------------------------------------------------
# 硬编码路径重写
# -----------------------------------------------------------------------------

def build_text_replacements(plan: dict) -> list[tuple[str, str]]:
    """根据实际资产迁移对生成文本替换规则，并补充目录级规则。"""
    replacements: dict[str, str] = {}

    for source, target in plan["all_pairs"]:
        old_name = _asset_name(source)
        new_name = _asset_name(target)

        # 先覆盖 GeneratedClass / ObjectPath，再覆盖 PackagePath。
        replacements[f"{source}.{old_name}_C"] = f"{target}.{new_name}_C"
        replacements[f"{source}.{old_name}"] = f"{target}.{new_name}"
        replacements[source] = target

    # P5 临时路径在资产创建前也要直接切到正式目录。
    replacements[
        "/Game/Projectile/VFX/NDC_ArenaProjectiles.NDC_ArenaProjectiles"
    ] = (
        f"{PROJECT_NAMESPACE}/Combat/Projectiles/VFX/DataChannels/"
        "NDC_ArenaProjectiles.NDC_ArenaProjectiles"
    )
    replacements[
        "/Game/Projectile/VFX/NDC_ArenaProjectileImpacts.NDC_ArenaProjectileImpacts"
    ] = (
        f"{PROJECT_NAMESPACE}/Combat/Projectiles/VFX/DataChannels/"
        "NDC_ArenaProjectileImpacts.NDC_ArenaProjectileImpacts"
    )
    replacements[
        "/Game/Projectile/VFX/NS_ArenaProjectiles_Shared.NS_ArenaProjectiles_Shared"
    ] = (
        f"{PROJECT_NAMESPACE}/Combat/Projectiles/VFX/Systems/"
        "NS_ArenaProjectiles_Shared.NS_ArenaProjectiles_Shared"
    )

    # Config 的目录扫描语义不能靠 /Game/GAS 根前缀替换，必须明确指定。
    replacements[
        "+GameplayCueNotifyPaths=/Game/GAS"
    ] = (
        f"+GameplayCueNotifyPaths={PROJECT_NAMESPACE}/Combat/GAS/Cues"
    )
    replacements[
        "+GameplayCueNotifyPaths=/Game/Boss"
    ] = (
        f"+GameplayCueNotifyPaths={PROJECT_NAMESPACE}/Characters/Boss/GAS/GameplayCue"
    )
    replacements[
        '+DirectoriesToAlwaysCook=(Path="/Game/GAS")'
    ] = (
        f'+DirectoriesToAlwaysCook=(Path="{PROJECT_NAMESPACE}/Combat")'
    )
    replacements[
        '+DirectoriesToAlwaysCook=(Path="/Game/Boss")'
    ] = (
        f'+DirectoriesToAlwaysCook=(Path="{PROJECT_NAMESPACE}/Characters/Boss")'
    )

    # 目录级规则用于生成脚本中的 destination folder 常量。
    directory_rules = (
        ("/Game/GAS/GameplayCues/DurationCue", f"{PROJECT_NAMESPACE}/Combat/GAS/Cues/Looping"),
        ("/Game/GAS/GameplayCues/InstaneCue", f"{PROJECT_NAMESPACE}/Combat/GAS/Cues/Instant"),
        ("/Game/GAS/GameplayCues/Elite", f"{PROJECT_NAMESPACE}/Combat/GAS/Cues/Elite"),
        ("/Game/GAS/GameplayEffect/Status", f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Status"),
        ("/Game/GAS/GameplayEffect/Trigger", f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Triggers"),
        ("/Game/GAS/GameplayEffect/Upgrade", f"{PROJECT_NAMESPACE}/Combat/GAS/Effects/Upgrades"),
        ("/Game/GAS/GameplayAbility", f"{PROJECT_NAMESPACE}/Combat/GAS/Abilities"),
        ("/Game/GAS/GameplayEffect", f"{PROJECT_NAMESPACE}/Combat/GAS/Effects"),
        ("/Game/GAS/GameplayCues", f"{PROJECT_NAMESPACE}/Combat/GAS/Cues"),
        ("/Game/GAS/DamageFeedback", f"{PROJECT_NAMESPACE}/Combat/Feedback"),
        ("/Game/GAS/Projectile", f"{PROJECT_NAMESPACE}/Combat/Projectiles/Actors"),
        ("/Game/GAS/Area", f"{PROJECT_NAMESPACE}/Combat/Areas/Dash"),
        ("/Game/Blueprints/ArenaLightningStormArea", f"{PROJECT_NAMESPACE}/Combat/Areas/LightningStorm"),
        ("/Game/Blueprints/DataAsset", f"{PROJECT_NAMESPACE}/Systems/Waves/Data"),
        ("/Game/Characters/ArenaPlayer", f"{PROJECT_NAMESPACE}/Characters/Player"),
        ("/Game/Characters/ArenaEnemy", f"{PROJECT_NAMESPACE}/Characters/Enemies"),
        ("/Game/Data/EnemyAffix", f"{PROJECT_NAMESPACE}/Characters/Enemies/Data/Affixes"),
        ("/Game/Data/Upgrade", f"{PROJECT_NAMESPACE}/Systems/Upgrades/Data"),
        ("/Game/Data/Weapon", f"{PROJECT_NAMESPACE}/Combat/Weapons/Data"),
        ("/Game/Data/Pickup", f"{PROJECT_NAMESPACE}/Systems/Pickups/Data"),
        ("/Game/Items/Inventory", f"{PROJECT_NAMESPACE}/Systems/Inventory"),
        ("/Game/Items/Pickups", f"{PROJECT_NAMESPACE}/Systems/Pickups/Blueprints"),
        ("/Game/Assets/Pickups", f"{PROJECT_NAMESPACE}/Systems/Pickups/Art"),
        ("/Game/GameMode", f"{PROJECT_NAMESPACE}/Core/GameMode"),
        ("/Game/Core/BP_ArenaPlayerController", f"{PROJECT_NAMESPACE}/Core/Controllers/BP_ArenaPlayerController"),
        ("/Game/TopDown/Input/Actions", f"{PROJECT_NAMESPACE}/Input/Actions"),
        ("/Game/TopDown/Input", f"{PROJECT_NAMESPACE}/Input/Contexts"),
        ("/Game/TopDown/Lvl_TopDown", f"{PROJECT_NAMESPACE}/World/Maps/Lvl_Arena"),
        ("/Game/TopDown/MI_Colorway", f"{PROJECT_NAMESPACE}/World/Materials/MI_Colorway"),
        ("/Game/TopDown/Blueprints", f"{PROJECT_NAMESPACE}/Dev/LegacyTemplate/TopDown/Blueprints"),
        ("/Game/TopDown/Cursor", f"{PROJECT_NAMESPACE}/Dev/LegacyTemplate/TopDown/Cursor"),
        ("/Game/UI/UpgradeIcons", f"{PROJECT_NAMESPACE}/UI/Upgrade/Icons"),
        ("/Game/UI", f"{PROJECT_NAMESPACE}/UI"),
        ("/Game/Niagara", f"{PROJECT_NAMESPACE}/VFX/Common"),
        ("/Game/Tests", f"{PROJECT_NAMESPACE}/Dev/Tests"),
        ("/Game/Mass", f"{PROJECT_NAMESPACE}/Dev/Experiments/Mass"),
        ("/Game/Cursor", f"{PROJECT_NAMESPACE}/UI/Cursor/Current"),
        ("/Game/Boss", f"{PROJECT_NAMESPACE}/Characters/Boss"),
    )

    for old, new in directory_rules:
        replacements.setdefault(old, new)

    # 最长字符串先替换，避免父路径抢先吞掉更具体规则。
    return sorted(
        replacements.items(),
        key=lambda pair: len(pair[0]),
        reverse=True,
    )


def iter_text_files() -> list[pathlib.Path]:
    """只扫描真正会参与构建/资产生成的文本文件，不改迁移设计文档本身。"""
    project_dir = pathlib.Path(unreal.Paths.project_dir()).resolve()
    result: set[pathlib.Path] = set()

    roots_and_extensions = (
        (project_dir / "Config", {".ini"}),
        (project_dir / "Source", {".h", ".hpp", ".cpp", ".cs"}),
        (project_dir / "Plugins", {".h", ".hpp", ".cpp", ".cs", ".ini", ".py"}),
        (project_dir / "Content" / "Python", {".py", ".md"}),
        (project_dir / "Scripts" / "Python", {".py", ".md"}),
    )

    migration_dir = (
        project_dir / "Scripts" / "Python" / "migration"
    ).resolve()

    for root, extensions in roots_and_extensions:
        if not root.exists():
            continue

        for path in root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in extensions:
                continue

            resolved = path.resolve()
            if migration_dir in resolved.parents:
                continue

            result.add(resolved)

    for project_file in project_dir.glob("*.uproject"):
        result.add(project_file.resolve())

    return sorted(result)


def _read_text_preserve_bom(path: pathlib.Path) -> tuple[str, str]:
    """读取 UTF-8 文本并保留是否带 BOM。"""
    data = path.read_bytes()
    encoding = "utf-8-sig" if data.startswith(b"\xef\xbb\xbf") else "utf-8"
    return data.decode(encoding), encoding


def rewrite_text_paths(plan: dict) -> int:
    """修改 Config / Source / Python 中的旧 Content 硬编码路径。"""
    replacements = build_text_replacements(plan)
    changed_files = 0

    for path in iter_text_files():
        try:
            old_text, encoding = _read_text_preserve_bom(path)
        except UnicodeDecodeError:
            _warn(f"Skip non-UTF8 text file: {path}")
            continue

        new_text = old_text
        replacement_count = 0

        for old, new in replacements:
            count = new_text.count(old)
            if count:
                new_text = new_text.replace(old, new)
                replacement_count += count

        if new_text == old_text:
            continue

        path.write_bytes(new_text.encode(encoding))
        changed_files += 1
        _log(
            f"[REWRITE] {path} "
            f"({replacement_count} path replacement(s))"
        )

    return changed_files


# -----------------------------------------------------------------------------
# 验证
# -----------------------------------------------------------------------------

def validate_old_asset_roots() -> list[str]:
    """检查旧资产根下是否仍存在非 Redirector 的真实资产。"""
    residuals: list[str] = []

    for root in OLD_ASSET_ROOTS_TO_CLEAR:
        assets = unreal.EditorAssetLibrary.list_assets(
            root,
            recursive=True,
            include_folder=False,
        )

        for raw_path in assets:
            asset_path = _canonical_asset_path(raw_path)
            if _is_redirector(asset_path):
                continue

            # 第三方 Mannequins 不在 ArenaPlayer/ArenaEnemy 子根里，不会误报。
            residuals.append(asset_path)

    return sorted(set(residuals))


def validate_text_residuals() -> list[str]:
    """检查构建/生成脚本中是否仍残留旧虚拟路径。"""
    residuals: list[str] = []

    for path in iter_text_files():
        try:
            text, _ = _read_text_preserve_bom(path)
        except UnicodeDecodeError:
            continue

        for line_number, line in enumerate(text.splitlines(), start=1):
            for token in OLD_TEXT_TOKENS:
                if token in line:
                    residuals.append(
                        f"{path}:{line_number}: contains {token}"
                    )
                    break

    return residuals


def validate_migration() -> bool:
    """执行迁移后的核心静态验收，不替代重启 Editor、UBT、PIE。"""
    asset_residuals = validate_old_asset_roots()
    text_residuals = validate_text_residuals()

    if asset_residuals:
        _warn(
            f"Validation found {len(asset_residuals)} non-redirector asset(s) "
            "under old roots."
        )
        for item in asset_residuals:
            _warn(f"[OLD ASSET] {item}")
    else:
        _log("Validation: no non-redirector Unreal assets remain under old roots.")

    if text_residuals:
        _warn(
            f"Validation found {len(text_residuals)} old hard-coded path occurrence(s)."
        )
        for item in text_residuals[:200]:
            _warn(f"[OLD TEXT] {item}")
        if len(text_residuals) > 200:
            _warn(
                f"... {len(text_residuals) - 200} additional text residual(s) omitted."
            )
    else:
        _log("Validation: no configured old hard-coded path tokens remain.")

    return not asset_residuals and not text_residuals


# -----------------------------------------------------------------------------
# 主流程
# -----------------------------------------------------------------------------

def main() -> None:
    """按照 RUN_MODE 执行 Dry Run、迁移、文本重写或验证。"""
    if RUN_MODE not in {"dry_run", "apply", "rewrite", "validate", "all"}:
        raise ValueError(f"Unsupported RUN_MODE: {RUN_MODE}")

    _log("=" * 72)
    _log(f"Project Arcane Arena Content Migration | mode={RUN_MODE}")
    _log(f"Project: {unreal.Paths.project_dir()}")
    _log("=" * 72)

    plan = build_asset_plan()
    print_asset_plan(plan)

    support_moves: list[tuple[pathlib.Path, pathlib.Path]] = []
    support_review: list[str] = []

    if MOVE_SUPPORT_FILES:
        support_moves, support_review = build_support_file_plan()
        _log(
            f"Support-file plan: move={len(support_moves)}, "
            f"review={len(support_review)}"
        )

        for source, target in support_moves:
            _log(f"[FILE MOVE] {source} -> {target}")

        for item in support_review:
            _warn(f"[FILE REVIEW] {item}")

    if RUN_MODE == "dry_run":
        _log("Dry Run complete. No files or assets were modified.")
        return

    if RUN_MODE in {"apply", "all"}:
        if not ALLOW_APPLY:
            raise RuntimeError(
                "RUN_MODE requests real migration, but ALLOW_APPLY=False. "
                "Review Dry Run first, then set ALLOW_APPLY=True."
            )

        apply_asset_moves(plan)

        if MOVE_SUPPORT_FILES:
            apply_support_file_moves(support_moves)

        _log(
            "Asset/file apply complete. Redirectors are intentionally kept "
            "until restart/build/PIE verification."
        )

    if RUN_MODE in {"rewrite", "all"}:
        changed_files = rewrite_text_paths(plan)
        _log(f"Text rewrite complete: changed_files={changed_files}")

    if RUN_MODE in {"validate", "all"}:
        if validate_migration():
            _log(
                "Static validation PASSED. Next: restart Editor, narrow UBT build, "
                "PIE/key-map verification, then Fix Up Redirectors."
            )
        else:
            _warn(
                "Static validation is not clean yet. Review the report before "
                "Fix Up Redirectors."
            )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        _error(f"Migration failed: {exc}")
        _error(traceback.format_exc())
        raise
    finally:
        _write_report()
