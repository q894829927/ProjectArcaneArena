"""Idempotently create and connect Boss death assets for the Victory Outro.

Compile the new C++ code and restart Unreal Editor before running this script.
The script never edits level cameras, Widget graphs, or the Behavior Tree.
"""

import importlib

import unreal

from build_assets import arena_asset_tools


tools = importlib.reload(arena_asset_tools)

SOURCE_DEATH_ANIMATION = "/Game/wukongManny/Death"
SOURCE_DEATH_NIAGARA = "/Game/Boss/VFX/NS_BossCharge_Impact"
SOURCE_DEATH_SOUND = "/Game/SlashTrail_SoftTofu/Resource/Audio/Hit/SC_Hit_Cue"
BOSS_CHARACTER_PATH = "/Game/Boss/Character/BP_ArenaBossCharacter"
BOSS_MESH_PATH = "/Game/Boss/Character/SKM_ArenaBoss"

DEATH_ANIMATION_PATH = "/Game/Boss/Animation/AS_BossDeath"
DEATH_MONTAGE_PATH = "/Game/Boss/Animation/AM_BossDeath"
DEATH_NIAGARA_PATH = "/Game/Boss/VFX/NS_BossDeath"
DEATH_CUE_PATH = "/Game/Boss/GAS/GameplayCue/GCN_BossDeath"


def _get_editor_property(obj, *property_names):
    """Read an editor property while tolerating Python naming differences."""
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
    """Write an editor property while tolerating Python naming differences."""
    last_error = None
    for property_name in property_names:
        try:
            obj.set_editor_property(property_name, value)
            return
        except Exception as error:
            last_error = error
    raise RuntimeError(
        f"Could not write any property {property_names} on {obj}: {last_error}"
    )


def _ensure_directories():
    """Create the Boss animation, VFX, and GameplayCue directories."""
    for directory in (
        "/Game/Boss/Animation",
        "/Game/Boss/VFX",
        "/Game/Boss/GAS/GameplayCue",
    ):
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)


def _validate_prerequisites():
    """Validate native types and source assets before changing any target."""
    tools.require_unreal_type("ArenaBossCharacter")
    tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    tools.require_asset(BOSS_MESH_PATH, unreal.SkeletalMesh)
    tools.require_asset(SOURCE_DEATH_ANIMATION, unreal.AnimSequence)
    tools.require_asset(SOURCE_DEATH_NIAGARA, unreal.NiagaraSystem)
    tools.require_asset(SOURCE_DEATH_SOUND, unreal.SoundBase)
    tools.make_tag("GameplayCue.Boss.Death")

    if unreal.EditorAssetLibrary.does_asset_exist(DEATH_ANIMATION_PATH):
        tools.require_asset(DEATH_ANIMATION_PATH, unreal.AnimSequence)
    if unreal.EditorAssetLibrary.does_asset_exist(DEATH_MONTAGE_PATH):
        tools.require_asset(DEATH_MONTAGE_PATH, unreal.AnimMontage)
    if unreal.EditorAssetLibrary.does_asset_exist(DEATH_NIAGARA_PATH):
        tools.require_asset(DEATH_NIAGARA_PATH, unreal.NiagaraSystem)
    if unreal.EditorAssetLibrary.does_asset_exist(DEATH_CUE_PATH):
        tools.require_blueprint(DEATH_CUE_PATH, unreal.GameplayCueNotify_Burst)


def _prepare_death_animation():
    """Duplicate the Wukong death animation, register compatibility, and disable root motion."""
    death_animation = tools.duplicate_or_load_asset(
        DEATH_ANIMATION_PATH,
        SOURCE_DEATH_ANIMATION,
        unreal.AnimSequence,
    )
    boss_mesh = tools.require_asset(BOSS_MESH_PATH, unreal.SkeletalMesh)
    boss_skeleton = _get_editor_property(boss_mesh, "skeleton", "Skeleton")
    animation_skeleton = _get_editor_property(death_animation, "skeleton", "Skeleton")
    if boss_skeleton != animation_skeleton:
        boss_skeleton.modify()
        boss_skeleton.add_compatible_skeleton(animation_skeleton)
        if not unreal.EditorAssetLibrary.save_loaded_asset(boss_skeleton, False):
            raise RuntimeError(
                "Failed to save Boss compatible Skeleton entry: "
                f"{boss_skeleton.get_path_name()} <- {animation_skeleton.get_path_name()}"
            )

    death_animation.modify()
    _set_editor_property(
        death_animation,
        False,
        "enable_root_motion",
        "bEnableRootMotion",
    )
    tools.save_asset(DEATH_ANIMATION_PATH)
    return death_animation


def _create_or_load_montage(death_animation):
    """Create a DefaultSlot montage from the dedicated death animation."""
    if unreal.EditorAssetLibrary.does_asset_exist(DEATH_MONTAGE_PATH):
        return tools.require_asset(DEATH_MONTAGE_PATH, unreal.AnimMontage)

    montage_factory = unreal.AnimMontageFactory()
    _set_editor_property(
        montage_factory,
        death_animation,
        "source_animation",
        "SourceAnimation",
    )
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "AM_BossDeath",
        "/Game/Boss/Animation",
        unreal.AnimMontage,
        montage_factory,
    )
    if montage is None:
        raise RuntimeError(f"Failed to create Boss death montage: {DEATH_MONTAGE_PATH}")
    tools.save_asset(DEATH_MONTAGE_PATH)
    return montage


def _configure_death_cue():
    """Create the authoritative one-shot Boss death Niagara and generic hit sound Cue."""
    death_niagara = tools.duplicate_or_load_asset(
        DEATH_NIAGARA_PATH,
        SOURCE_DEATH_NIAGARA,
        unreal.NiagaraSystem,
    )
    death_sound = tools.require_asset(SOURCE_DEATH_SOUND, unreal.SoundBase)
    _, cue_class, cue_path = tools.create_or_load_blueprint(
        "GCN_BossDeath",
        "/Game/Boss/GAS/GameplayCue",
        unreal.GameplayCueNotify_Burst,
    )

    cue_defaults = unreal.get_default_object(cue_class)
    cue_defaults.modify()
    cue_defaults.set_editor_property(
        "gameplay_cue_tag",
        tools.make_tag("GameplayCue.Boss.Death"),
    )
    cue_defaults.set_editor_property(
        "default_placement_info",
        unreal.GameplayCueNotify_PlacementInfo(
            socket_name=unreal.Name("None"),
            attach_policy=unreal.GameplayCueNotify_AttachPolicy.DO_NOT_ATTACH,
            attachment_rule=unreal.AttachmentRule.KEEP_WORLD,
            override_rotation=False,
            override_scale=True,
            rotation_override=unreal.Rotator(0.0, 0.0, 0.0),
            scale_override=unreal.Vector(2.0, 2.0, 2.0),
        ),
    )
    cue_defaults.set_editor_property(
        "burst_effects",
        unreal.GameplayCueNotify_BurstEffects(
            burst_particles=[
                unreal.GameplayCueNotify_ParticleInfo(
                    niagara_system=death_niagara,
                    override_spawn_condition=False,
                    override_placement_info=False,
                    cast_shadow=False,
                )
            ],
            burst_sounds=[
                unreal.GameplayCueNotify_SoundInfo(
                    sound=death_sound,
                    override_spawn_condition=False,
                    override_placement_info=False,
                    use_sound_parameter_interface=False,
                )
            ],
        ),
    )
    tools.save_asset(DEATH_NIAGARA_PATH)
    tools.save_asset(cue_path)


def _configure_boss_character(death_montage):
    """Assign the death montage and keep the corpse alive through the 4-second Outro."""
    _, boss_class = tools.require_blueprint(
        BOSS_CHARACTER_PATH,
        tools.require_unreal_type("ArenaBossCharacter"),
    )
    boss_defaults = unreal.get_default_object(boss_class)
    boss_defaults.modify()
    boss_defaults.set_editor_property("death_montage", death_montage)
    boss_defaults.set_editor_property("death_montage_play_rate", 1.0)
    boss_defaults.set_editor_property("death_life_span", 5.5)
    tools.save_asset(BOSS_CHARACTER_PATH)


def main():
    """Run validation, asset creation, Cue setup, and Boss Blueprint configuration."""
    _validate_prerequisites()
    _ensure_directories()
    with unreal.ScopedSlowTask(4, "Setting up Boss Victory Outro assets") as task:
        task.make_dialog(True)
        death_animation = _prepare_death_animation()
        task.enter_progress_frame(1, "Prepared Boss death animation")
        death_montage = _create_or_load_montage(death_animation)
        task.enter_progress_frame(1, "Created Boss death montage")
        _configure_death_cue()
        task.enter_progress_frame(1, "Configured Boss death GameplayCue")
        _configure_boss_character(death_montage)
        task.enter_progress_frame(1, "Configured Boss death presentation")

    unreal.log(
        "Boss Victory Outro assets are ready. Add a CameraActor tagged "
        "'BossVictoryCamera' and optional WBP_PlayerHUD widgets manually."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Boss Victory Outro setup failed: {error}")
        raise
