"""资产迁移后的 Wave/EnemySpawn 诊断脚本。

只读诊断，不修改资产。
在 Unreal Editor 中打开正式 Lvl_Arena 后执行本脚本。
"""

import unreal


GAME_MODE_PATH = "/Game/ProjectArcaneArena/Core/GameMode/BP_ArenaGameMode"
WAVE_DATA_PATH = "/Game/ProjectArcaneArena/Systems/Waves/Data/DA_Waves_Prototype"
EXPECTED_MAP_SUFFIX = "/Game/ProjectArcaneArena/World/Maps/Lvl_Arena"


def log(message):
    unreal.log(f"[ArenaWaveMigrationDiag] {message}")


def warn(message):
    unreal.log_warning(f"[ArenaWaveMigrationDiag] {message}")


def error(message):
    unreal.log_error(f"[ArenaWaveMigrationDiag] {message}")


def load_required_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        error(f"Missing asset: {path}")
    else:
        log(f"Asset OK: {path} ({asset.get_class().get_name()})")
    return asset


def get_generated_class(blueprint):
    if blueprint is None:
        return None
    generated_class = blueprint.get_editor_property("generated_class")
    if generated_class is None:
        error(f"Blueprint has no generated_class: {blueprint.get_path_name()}")
    return generated_class


def inspect_game_mode():
    blueprint = load_required_asset(GAME_MODE_PATH)
    generated_class = get_generated_class(blueprint)
    if generated_class is None:
        return

    defaults = unreal.get_default_object(generated_class)
    log(f"GameMode CDO: {defaults.get_path_name()}")

    try:
        wave_data = defaults.get_editor_property("wave_data")
    except Exception as exc:
        error(f"Cannot read BP_ArenaGameMode.wave_data: {exc}")
        wave_data = None

    if wave_data is None:
        error("BP_ArenaGameMode WaveData = None")
    else:
        log(f"BP_ArenaGameMode WaveData = {wave_data.get_path_name()}")

    try:
        pickup_table = defaults.get_editor_property("pickup_drop_table")
    except Exception as exc:
        warn(f"Cannot read PickupDropTable: {exc}")
        pickup_table = None

    log(
        "BP_ArenaGameMode PickupDropTable = "
        + (pickup_table.get_path_name() if pickup_table else "None")
    )


def inspect_wave_data():
    wave_data = load_required_asset(WAVE_DATA_PATH)
    if wave_data is None:
        return

    try:
        waves = list(wave_data.get_editor_property("waves"))
    except Exception as exc:
        error(f"Cannot read WaveData.waves: {exc}")
        return

    log(f"WaveData Waves = {len(waves)}")
    if not waves:
        error("DA_Waves_Prototype contains zero waves.")
        return

    for wave_index, wave in enumerate(waves, start=1):
        try:
            enemies = list(wave.get_editor_property("enemies"))
        except Exception as exc:
            error(f"Wave {wave_index}: cannot read enemies: {exc}")
            continue

        valid_count = 0
        details = []
        for entry in enemies:
            try:
                enemy_class = entry.get_editor_property("enemy_class")
                count = int(entry.get_editor_property("count"))
            except Exception as exc:
                details.append(f"<invalid entry: {exc}>")
                continue

            class_name = enemy_class.get_path_name() if enemy_class else "None"
            details.append(f"{class_name} x{count}")
            if enemy_class is not None and count > 0:
                valid_count += count

        try:
            boss_wave = bool(wave.get_editor_property("boss_wave"))
        except Exception:
            try:
                boss_wave = bool(wave.get_editor_property("b_boss_wave"))
            except Exception:
                boss_wave = False

        log(
            f"Wave {wave_index}: validEnemyCount={valid_count}, "
            f"boss={boss_wave}, entries=[{'; '.join(details)}]"
        )
        if valid_count <= 0:
            error(f"Wave {wave_index} has no valid enemy entries.")


def inspect_editor_world():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        error("No editor world is open.")
        return

    world_path = world.get_path_name()
    log(f"Editor World = {world_path}")
    if EXPECTED_MAP_SUFFIX not in world_path:
        warn("Current editor world is not Lvl_Arena.")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors() if actor_subsystem else []

    spawn_points = []
    player_starts = []
    for actor in actors:
        if actor is None:
            continue

        class_name = actor.get_class().get_name()
        if class_name == "PlayerStart":
            player_starts.append(actor)

        try:
            tags = [str(tag) for tag in actor.get_editor_property("tags")]
        except Exception:
            tags = []

        if "EnemySpawn" in tags:
            spawn_points.append(actor)

    log(f"Loaded actors = {len(actors)}")
    log(f"PlayerStart count = {len(player_starts)}")
    log(f"EnemySpawn tagged actor count = {len(spawn_points)}")

    for actor in spawn_points:
        log(
            f"EnemySpawn: {actor.get_path_name()} "
            f"@ {actor.get_actor_location()}"
        )

    if not spawn_points:
        error(
            "No loaded actor has Actor Tag 'EnemySpawn'. "
            "WaveManager will reject StartNextWave."
        )


def inspect_map_game_mode_override():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        return

    world_settings = world.get_world_settings()
    if world_settings is None:
        warn("WorldSettings is unavailable.")
        return

    try:
        override = world_settings.get_editor_property("default_game_mode")
    except Exception as exc:
        warn(f"Cannot read WorldSettings.default_game_mode: {exc}")
        return

    log(
        "Lvl_Arena GameMode Override = "
        + (override.get_path_name() if override else "None (uses project default)")
    )


def main():
    log("=" * 70)
    log("Wave migration diagnostic started")
    inspect_editor_world()
    inspect_map_game_mode_override()
    inspect_game_mode()
    inspect_wave_data()
    log("Wave migration diagnostic finished")
    log("Also PIE once and filter Output Log for: LogArenaWaves / LogArenaNetworkFlow")
    log("=" * 70)


main()
