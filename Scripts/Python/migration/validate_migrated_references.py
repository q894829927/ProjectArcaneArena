"""在新启动的 Unreal Python commandlet 中只读检查迁移引用；不保存资产。

NullRHI 验证加载和对象引用，不替代材质视觉检查、PIE 或 Cook。
"""

import pathlib
import re
import runpy

import unreal


def require(condition, message):
    """记录每个断言，失败时继续检查以收集完整问题。"""
    if not condition:
        failures.append(message)
        unreal.log_error("[MigrationReferences] FAIL: " + message)


def check_waves(path):
    """检查波次中敌人 Class 和精英依赖能否真实解析。"""
    asset = unreal.load_asset(path)
    require(asset is not None, "Wave data missing: " + path)
    if not asset:
        return
    waves = asset.get_editor_property("waves")
    require(len(waves) > 0, "No waves: " + path)
    for index, wave in enumerate(waves):
        entries = wave.get_editor_property("enemies")
        require(len(entries) > 0, f"Empty wave {index + 1}: {path}")
        for entry in entries:
            enemy = entry.get_editor_property("enemy_class")
            count = entry.get_editor_property("count")
            require(enemy is not None and count > 0, f"Invalid enemy/count in wave {index + 1}: {path}")
            unreal.log(f"[MigrationReferences] Wave {index + 1}: enemy={enemy} count={count}")
            if entry.get_editor_property("elite_count") > 0:
                pool = entry.get_editor_property("elite_affix_pool")
                require(len(pool) > 0 and all(pool), f"Invalid affix pool: {path} wave {index + 1}")
                require(asset.get_editor_property("elite_baseline_effect_class") is not None,
                        "Missing elite baseline effect: " + path)


def check_materials(root):
    """核对父级和贴图节点引用；NullRHI 下不依赖 ShaderMap 的 GetUsedTextures。"""
    for relative in ("Bottle/MI_Bottle", "Liquid/MI_BlueLiquid", "Liquid/MI_RedLiquid", "Cork/M_Cork"):
        path = root + "/" + relative
        material = unreal.load_asset(path)
        require(material is not None, "Missing material: " + path)
        if not material:
            continue
        visited = set()
        while isinstance(material, unreal.MaterialInstance):
            name = material.get_path_name()
            require(name not in visited, "Material parent cycle: " + path)
            if name in visited:
                break
            visited.add(name)
            material = material.get_editor_property("parent")
            require(material is not None, "Missing material parent: " + name)
            unreal.log(f"[MigrationReferences] Material parent: {name} -> {material}")
        if isinstance(material, unreal.Material):
            # NullRHI 不生成渲染资源，GetUsedTextures 可能为空；直接检查加载的表达式。
            expressions = [node for node in unreal.ObjectIterator(unreal.MaterialExpressionTextureBase)
                           if node.get_outer() == material]
            textures = [node.get_editor_property("texture") for node in expressions]
            unreal.log(f"[MigrationReferences] Material textures: {material.get_path_name()} -> {textures}")
            require(len(textures) > 0 and all(textures), "Missing potion material textures: " + path)


def main(check_legacy_paths=True):
    """检查目标引用；禁用兼容映射的独立验收不主动加载旧路径。"""
    failures.clear()
    config = pathlib.Path(unreal.Paths.project_dir()) / "Config/DefaultEngine.ini"
    pairs = re.findall(r'^\+PackageRedirects=\(OldName="([^"]+)",NewName="([^"]+)"\)',
                       config.read_text(encoding="utf-8-sig"), re.MULTILINE)
    require(len(pairs) == 264, f"Expected 264 migration redirects, got {len(pairs)}")
    migration = runpy.run_path(str(pathlib.Path(__file__).with_name("migrate_content_layout.py")))
    require(not migration["validate_text_residuals"](), "Unexpected old hard-coded paths outside CoreRedirects")
    check_waves("/Game/ProjectArcaneArena/Systems/Waves/Data/DA_Waves_Prototype")
    check_waves("/Game/ProjectArcaneArena/Systems/Waves/Data/DA_Waves_Prototype_test")
    check_materials("/Game/ProjectArcaneArena/Systems/Pickups/Art/Potions")
    for old, new in pairs:
        target = unreal.load_asset(new)
        require(target is not None, "Target load failed: " + new)
        # 改名地图由 ObjectRedirect 单独处理，避免旧对象名推导影响 Package 检查。
        if check_legacy_paths and old.rsplit("/", 1)[-1] == new.rsplit("/", 1)[-1]:
            legacy = unreal.load_asset(old)
            require(legacy is not None and legacy == target, "Old reference did not resolve to target: " + old)
    # 地图已经改名，Python load_object 不执行对象名 CoreRedirect；实际地图启动另做游戏冒烟。
    unreal.log(f"[MigrationReferences] COMPLETE: redirects={len(pairs)} failures={len(failures)}")
    if failures:
        raise RuntimeError(f"Migration reference validation failed: {len(failures)} checks")


failures = []
if __name__ == "__main__":
    main()
