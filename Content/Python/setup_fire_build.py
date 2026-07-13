"""Create and configure the first Fire build assets.

Run this file from Unreal Editor via Tools > Execute Python Script after the
new Fire build C++ classes have compiled and the editor has reloaded them.
"""

import unreal


UPGRADE_PATH = "/Game/Data/Upgrade"
GAMEPLAY_EFFECT_PATH = "/Game/GAS/GameplayEffect/Status"
GAMEPLAY_CUE_PATH = "/Game/GAS/GameplayCues/DurationCue"
FIREBALL_ABILITY_PATH = "/Game/GAS/GameplayAbility/GA_Fireball"
GAME_MODE_PATH = "/Game/GameMode/BP_ArenaGameMode"
FIRE_NIAGARA_PATH = "/Game/SlashTrail_SoftTofu/Niagara/Fire/NS_AuraFX_Fire"
SHIELD_CUE_PATH = "/Game/GAS/GameplayCues/DurationCue/GCN_Shield_Active"


def make_tag(tag_name):
    """从已注册的标签名称创建 GameplayTag。"""
    if not tag_name:
        raise ValueError("GameplayTag name cannot be empty.")

    tag = unreal.GameplayTag()
    serialized_value = f'(TagName="{tag_name}")'

    if not tag.import_text(serialized_value):
        raise RuntimeError(
            f"Failed to import GameplayTag '{tag_name}' "
            f"from '{serialized_value}'."
        )

    if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag):
        raise RuntimeError(
            f"GameplayTag is invalid or not registered: {tag_name}"
        )

    return tag


def make_tag_container(*tag_names):
    """创建包含指定精确标签的 GameplayTagContainer。"""
    tags = [make_tag(tag_name) for tag_name in tag_names]
    return unreal.GameplayTagContainer(gameplay_tags=tags)


def load_required(asset_path, expected_class=None):
    """加载必需资产，并在缺失或类型不匹配时立即停止配置。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Required asset was not found: {asset_path}")
    if expected_class is not None and not isinstance(asset, expected_class):
        raise RuntimeError(
            f"Asset {asset_path} is {asset.get_class().get_name()}, "
            f"expected {expected_class.__name__}."
        )
    return asset


def save_required(asset_path):
    """保存已配置资产，保存失败时停止，避免留下半配置状态。"""
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save asset: {asset_path}")


def create_or_load_data_asset(asset_name):
    """创建或加载 ArenaUpgradeDataAsset。"""
    asset_path = f"{UPGRADE_PATH}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing = unreal.EditorAssetLibrary.load_asset(asset_path)
        if existing is None:
            raise RuntimeError(f"Failed to load existing upgrade data asset: {asset_path}")
        return existing, asset_path

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ArenaUpgradeDataAsset)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        UPGRADE_PATH,
        unreal.ArenaUpgradeDataAsset,
        factory,
    )
    if asset is None:
        raise RuntimeError(f"Failed to create upgrade data asset: {asset_path}")
    return asset, asset_path


def create_or_load_burning_effect_blueprint():
    """创建或加载继承原生 Burning GE 的蓝图类。"""
    asset_name = "GE_Status_Burning"
    asset_path = f"{GAMEPLAY_EFFECT_PATH}/{asset_name}"
    existing = None
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.ArenaGameplayEffect_Burning)
        existing = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            GAMEPLAY_EFFECT_PATH,
            unreal.Blueprint,
            factory,
        )
    if existing is None:
        raise RuntimeError(f"Failed to create Burning GameplayEffect: {asset_path}")

    effect_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if effect_class is None:
        raise RuntimeError(f"Failed to load generated class: {asset_path}")
    save_required(asset_path)
    return effect_class, asset_path


def configure_upgrade(
    asset,
    upgrade_id,
    display_name,
    description,
    upgrade_tags,
    required_tags,
    numeric_value,
    max_stacks,
    stackable,
):
    """写入 Fireball 升级的路由、标签、数值和堆叠配置。"""
    asset.set_editor_property("upgrade_id", unreal.Name(upgrade_id))
    asset.set_editor_property("upgrade_name", display_name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("upgrade_tags", make_tag_container(*upgrade_tags))
    asset.set_editor_property("required_tags", make_tag_container(*required_tags))
    asset.set_editor_property("blocked_tags", make_tag_container())
    asset.set_editor_property("target_ability_tag", make_tag("Ability.Fireball"))
    asset.set_editor_property("damage_type_tag", make_tag("Damage.Fire"))
    asset.set_editor_property("numeric_value", numeric_value)
    asset.set_editor_property("max_stacks", max_stacks)
    asset.set_editor_property("stackable", stackable)
    asset.modify()


def configure_fireball_ability(burning_effect_class):
    """让现有 GA_Fireball 显式引用 Burning GE 蓝图子类。"""
    ability_blueprint = load_required(FIREBALL_ABILITY_PATH, unreal.Blueprint)
    ability_class = unreal.EditorAssetLibrary.load_blueprint_class(FIREBALL_ABILITY_PATH)
    if ability_class is None:
        raise RuntimeError(f"Failed to load generated class: {FIREBALL_ABILITY_PATH}")

    ability_defaults = unreal.get_default_object(ability_class)
    ability_defaults.set_editor_property("burning_effect_class", burning_effect_class)
    ability_defaults.modify()
    save_required(FIREBALL_ABILITY_PATH)
    return ability_blueprint


def configure_game_mode_upgrade_pool(fireball_damage, fireball_burning):
    """把 Fire 构筑资产追加到 GameMode 升级池，并保留现有升级。"""
    game_mode_blueprint = load_required(GAME_MODE_PATH, unreal.Blueprint)
    game_mode_class = unreal.EditorAssetLibrary.load_blueprint_class(GAME_MODE_PATH)
    if game_mode_class is None:
        raise RuntimeError(f"Failed to load generated class: {GAME_MODE_PATH}")

    game_mode_defaults = unreal.get_default_object(game_mode_class)
    upgrade_pool = list(game_mode_defaults.get_editor_property("upgrade_pool"))
    for upgrade in (fireball_damage, fireball_burning):
        if upgrade not in upgrade_pool:
            upgrade_pool.append(upgrade)
    game_mode_defaults.set_editor_property("upgrade_pool", upgrade_pool)
    game_mode_defaults.modify()
    save_required(GAME_MODE_PATH)
    return game_mode_blueprint


def configure_burning_cue():
    """复制持续 Cue 模板，并替换为附着目标根组件的循环火焰 Niagara。"""
    asset_name = "GCN_Burning_Active"
    cue_asset_path = f"{GAMEPLAY_CUE_PATH}/{asset_name}"
    cue_blueprint = None
    if unreal.EditorAssetLibrary.does_asset_exist(cue_asset_path):
        cue_blueprint = unreal.EditorAssetLibrary.load_asset(cue_asset_path)
    else:
        cue_blueprint = unreal.EditorAssetLibrary.duplicate_asset(SHIELD_CUE_PATH, cue_asset_path)
    if cue_blueprint is None:
        raise RuntimeError(f"Failed to create Burning GameplayCue: {cue_asset_path}")

    cue_class = unreal.EditorAssetLibrary.load_blueprint_class(cue_asset_path)
    if cue_class is None:
        raise RuntimeError(f"Failed to load generated class: {cue_asset_path}")

    fire_system = load_required(FIRE_NIAGARA_PATH, unreal.NiagaraSystem)
    cue_defaults = unreal.get_default_object(cue_class)
    cue_defaults.set_editor_property(
        "gameplay_cue_tag",
        make_tag("GameplayCue.Status.Burning.Active"),
    )

    placement = cue_defaults.get_editor_property("default_placement_info")
    placement.set_editor_property(
        "attach_policy",
        unreal.GameplayCueNotifyAttachPolicy.ATTACH_TO_TARGET,
    )
    placement.set_editor_property("socket_name", unreal.Name("None"))
    placement.set_editor_property("override_rotation", False)
    placement.set_editor_property("override_scale", False)
    cue_defaults.set_editor_property("default_placement_info", placement)

    particle_info = unreal.GameplayCueNotifyParticleInfo()
    particle_info.set_editor_property("niagara_system", fire_system)
    particle_info.set_editor_property("override_spawn_condition", False)
    particle_info.set_editor_property("override_placement_info", False)
    looping_effects = cue_defaults.get_editor_property("looping_effects")
    looping_effects.set_editor_property("looping_particles", [particle_info])
    cue_defaults.set_editor_property("looping_effects", looping_effects)
    cue_defaults.modify()
    save_required(cue_asset_path)
    return cue_blueprint


def main():
    """创建并连接 Fireball Damage、Burning、GE、Cue 和 GameMode 配置。"""
    fireball_damage, damage_path = create_or_load_data_asset("DA_Upgrade_FireballDamage")
    configure_upgrade(
        fireball_damage,
        "FireballDamage",
        "Fireball Damage",
        "Fireball deals 20% more damage per stack.",
        ("Build.Fire", "Upgrade.Fireball.Damage"),
        (),
        0.20,
        3,
        True,
    )
    save_required(damage_path)

    fireball_burning, burning_path = create_or_load_data_asset("DA_Upgrade_FireballBurning")
    configure_upgrade(
        fireball_burning,
        "FireballBurning",
        "Ignition",
        "Fireball applies a three-stack Burning effect.",
        ("Build.Fire", "Upgrade.Fireball.Burning"),
        ("Build.Fire",),
        5.0,
        1,
        False,
    )
    save_required(burning_path)

    burning_effect_class, effect_path = create_or_load_burning_effect_blueprint()
    configure_fireball_ability(burning_effect_class)
    configure_game_mode_upgrade_pool(fireball_damage, fireball_burning)
    configure_burning_cue()

    unreal.log(
        "Fire build setup completed: "
        f"upgrades={damage_path},{burning_path}; effect={effect_path}"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Fire build setup failed: {error}")
        raise
