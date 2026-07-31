"""应用阶段六 B 的简历 Demo 最终近似平衡配置。

在 Unreal Editor 中通过 Python 控制台执行。本脚本幂等更新现有资产，
不会创建副本，也不会修改地图、Behavior Tree 或技能逻辑。
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

ENEMY_ATTRIBUTES_PATH = "/Game/GAS/GameplayEffect/GE_Init_EnemyAttributes"
BOSS_ATTRIBUTES_PATH = "/Game/Boss/GAS/GameplayEffect/GE_Init_BossAttributes"
LIGHTNING_STORM_UPGRADE_PATH = "/Game/Data/Upgrade/DA_Upgrade_LightningStormDamage"
WAVE_DATA_PATH = "/Game/Blueprints/DataAsset/DA_Waves_Prototype"

ENEMY_HEALTH = 100.0
BOSS_HEALTH = 3000.0
LIGHTNING_STORM_DAMAGE_PER_STACK = 0.16
NORMAL_WAVE_SPAWN_INTERVALS = (0.75, 0.75, 0.75, 0.75, 1.0, 1.0, 1.0, 1.0)


def _get_editor_property(obj, *property_names):
    """读取第一个可用编辑器属性，兼容少量 UE Python 命名差异。"""
    last_error = None
    for property_name in property_names:
        try:
            return obj.get_editor_property(property_name)
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not read any property {property_names} from {obj}: {last_error}"
    )


def _set_editor_property(obj, value, *property_names):
    """写入第一个可用编辑器属性，全部失败时停止而不静默跳过。"""
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


def _load_gameplay_effect_defaults(asset_path):
    """加载 GameplayEffect Blueprint 及其 CDO，供 Modifier 预检和保存。"""
    blueprint, generated_class = tools.require_blueprint(asset_path, unreal.GameplayEffect)
    defaults = unreal.get_default_object(generated_class)
    if defaults is None:
        raise RuntimeError(f"Could not load GameplayEffect defaults: {asset_path}")
    return blueprint, defaults


def _attribute_label(modifier):
    """把 GameplayAttribute 转换为可稳定匹配 Health/MaxHealth 的文本。"""
    attribute = _get_editor_property(modifier, "attribute", "Attribute")
    return str(attribute)


def _find_health_modifier_indices(defaults, asset_path):
    """在 GE 中精确定位 Health 与 MaxHealth Modifier，避免误改其他属性。"""
    modifiers = list(_get_editor_property(defaults, "modifiers", "Modifiers"))
    health_index = None
    max_health_index = None

    for index, modifier in enumerate(modifiers):
        label = _attribute_label(modifier)
        if "MaxHealth" in label:
            if max_health_index is not None:
                raise RuntimeError(f"{asset_path} contains duplicate MaxHealth modifiers.")
            max_health_index = index
        elif "Health" in label:
            if health_index is not None:
                raise RuntimeError(f"{asset_path} contains duplicate Health modifiers.")
            health_index = index

    if health_index is None or max_health_index is None:
        raise RuntimeError(
            f"{asset_path} must expose exactly one Health and one MaxHealth modifier. "
            f"Found Health={health_index}, MaxHealth={max_health_index}."
        )
    return modifiers, health_index, max_health_index


def _read_scalable_float_value(modifier, asset_path):
    """读取 ScalableFloat Modifier 当前常量，拒绝覆盖曲线或其他计算类型。"""
    magnitude = _get_editor_property(
        modifier,
        "modifier_magnitude",
        "ModifierMagnitude",
    )
    calculation_type = str(
        _get_editor_property(
            magnitude,
            "magnitude_calculation_type",
            "MagnitudeCalculationType",
        )
    )
    if "SCALABLE_FLOAT" not in calculation_type.upper():
        raise RuntimeError(
            f"{asset_path} Health modifiers must use ScalableFloat, got {calculation_type}."
        )

    scalable_float = _get_editor_property(
        magnitude,
        "scalable_float_magnitude",
        "ScalableFloatMagnitude",
    )
    curve = _get_editor_property(scalable_float, "curve", "Curve")
    curve_table = _get_editor_property(curve, "curve_table", "CurveTable")
    row_name = str(_get_editor_property(curve, "row_name", "RowName"))
    if curve_table is not None or row_name not in ("", "None"):
        raise RuntimeError(
            f"{asset_path} Health modifier uses a curve; refusing to replace designer data."
        )
    return float(_get_editor_property(scalable_float, "value", "Value"))


def _call_gameplay_effect_editor_bridge(defaults, modifier_indices, magnitudes):
    """通过 Editor C++ 桥接层批量写入 Python 无权修改的 GameplayEffect Magnitude。"""
    bridge = getattr(unreal, "ArenaGameplayEffectEditorLibrary", None)
    if bridge is None:
        raise RuntimeError(
            "ArenaGameplayEffectEditorLibrary is unavailable. Close Unreal Editor, "
            "build ProjectArcaneArenaEditor, restart the editor, and run this script again."
        )

    result = bridge.set_scalable_float_modifier_magnitudes(
        defaults,
        [int(index) for index in modifier_indices],
        [float(value) for value in magnitudes],
    )
    if isinstance(result, tuple):
        success = bool(result[0])
        report = str(result[1]) if len(result) > 1 else ""
    else:
        success = bool(result)
        report = ""

    if not success:
        raise RuntimeError(f"GameplayEffect editor bridge failed: {report}")
    return report


def _prepare_gameplay_effect_update(asset_path):
    """预检 GameplayEffect 并返回修改计划，写入阶段尚未开始。"""
    blueprint, defaults = _load_gameplay_effect_defaults(asset_path)
    modifiers, health_index, max_health_index = _find_health_modifier_indices(
        defaults,
        asset_path,
    )
    old_health = _read_scalable_float_value(modifiers[health_index], asset_path)
    old_max_health = _read_scalable_float_value(modifiers[max_health_index], asset_path)
    return {
        "asset_path": asset_path,
        "blueprint": blueprint,
        "defaults": defaults,
        "modifiers": modifiers,
        "health_index": health_index,
        "max_health_index": max_health_index,
        "old_health": old_health,
        "old_max_health": old_max_health,
    }


def _apply_gameplay_effect_update(plan, target_health):
    """把 Health 与 MaxHealth 同步写成目标值并保存 GameplayEffect Blueprint。"""
    plan["blueprint"].modify()
    report = _call_gameplay_effect_editor_bridge(
        plan["defaults"],
        [plan["health_index"], plan["max_health_index"]],
        [target_health, target_health],
    )
    tools.save_asset(plan["asset_path"])
    unreal.log(
        f"Balanced {plan['asset_path']}: "
        f"Health {plan['old_health']:.1f} -> {target_health:.1f}, "
        f"MaxHealth {plan['old_max_health']:.1f} -> {target_health:.1f}. "
        f"{report}"
    )


def _prepare_upgrade_update():
    """加载雷暴增幅 DataAsset 并记录旧值，供统一提交阶段使用。"""
    upgrade_class = tools.require_unreal_type("ArenaUpgradeDataAsset")
    upgrade = tools.require_asset(LIGHTNING_STORM_UPGRADE_PATH, upgrade_class)
    old_value = float(
        _get_editor_property(upgrade, "numeric_value", "NumericValue")
    )
    return upgrade, old_value


def _apply_upgrade_update(upgrade, old_value):
    """更新雷暴增幅数值和显示说明，避免 UI 继续显示旧百分比。"""
    upgrade.modify()
    _set_editor_property(
        upgrade,
        LIGHTNING_STORM_DAMAGE_PER_STACK,
        "numeric_value",
        "NumericValue",
    )
    tools.set_localized_text_property(
        upgrade,
        "Description",
        "闪电风暴伤害提高 16%，最多叠加 3 层",
    )
    tools.save_asset(LIGHTNING_STORM_UPGRADE_PATH)
    unreal.log(
        f"Balanced {LIGHTNING_STORM_UPGRADE_PATH}: "
        f"{old_value:.2f} -> {LIGHTNING_STORM_DAMAGE_PER_STACK:.2f}."
    )


def _prepare_wave_update():
    """验证八个普通波和最终 Boss 波结构，避免对错误 WaveData 写入间隔。"""
    wave_class = tools.require_unreal_type("ArenaWaveDataAsset")
    wave_data = tools.require_asset(WAVE_DATA_PATH, wave_class)
    waves = list(_get_editor_property(wave_data, "waves", "Waves"))
    if len(waves) != 9:
        raise RuntimeError(
            f"{WAVE_DATA_PATH} must contain 8 normal waves and one Boss wave; "
            f"found {len(waves)} waves."
        )

    for index, wave in enumerate(waves):
        is_boss = bool(_get_editor_property(wave, "boss_wave", "b_boss_wave"))
        if index < 8 and is_boss:
            raise RuntimeError(f"Wave {index + 1} is unexpectedly marked as Boss.")
        if index == 8 and not is_boss:
            raise RuntimeError("The final wave must be the Boss wave.")
    old_intervals = [
        float(_get_editor_property(waves[index], "spawn_interval", "SpawnInterval"))
        for index in range(8)
    ]
    return wave_data, waves, old_intervals


def _apply_wave_update(wave_data, waves, old_intervals):
    """写入八个普通波的渐进生成间隔，Boss 波保持原配置。"""
    wave_data.modify()
    for index, interval in enumerate(NORMAL_WAVE_SPAWN_INTERVALS):
        _set_editor_property(
            waves[index],
            float(interval),
            "spawn_interval",
            "SpawnInterval",
        )
    _set_editor_property(wave_data, waves, "waves", "Waves")
    tools.save_asset(WAVE_DATA_PATH)
    unreal.log(
        f"Balanced {WAVE_DATA_PATH}: normal wave SpawnIntervals "
        f"{old_intervals} -> {list(NORMAL_WAVE_SPAWN_INTERVALS)}."
    )


def main():
    """先完成全部只读预检，再统一修改并保存四项最终 Demo 平衡资产。"""
    enemy_plan = _prepare_gameplay_effect_update(ENEMY_ATTRIBUTES_PATH)
    boss_plan = _prepare_gameplay_effect_update(BOSS_ATTRIBUTES_PATH)
    upgrade, old_upgrade_value = _prepare_upgrade_update()
    wave_data, waves, old_intervals = _prepare_wave_update()

    _apply_gameplay_effect_update(enemy_plan, ENEMY_HEALTH)
    _apply_gameplay_effect_update(boss_plan, BOSS_HEALTH)
    _apply_upgrade_update(upgrade, old_upgrade_value)
    _apply_wave_update(wave_data, waves, old_intervals)

    unreal.log_warning(
        "Resume demo balance applied. Expected full-run target is approximately "
        "8-10 minutes; only one final smoke run is recommended."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(
            "Resume demo balance failed. No suffixed assets were created. "
            f"Details: {error}"
        )
        raise
