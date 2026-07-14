#include "GAS/ArenaGameplayTags.h"

namespace ArenaGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character is dead and cannot act.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Character is stunned and cannot move or attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invincible, "State.Invincible", "Character ignores normal damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dashing, "State.Dashing", "Character is currently dashing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Casting, "State.Casting", "Character is currently casting.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "Character is performing an attack and cannot start another one.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_BasicAttack, "Ability.BasicAttack", "Basic attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fireball, "Ability.Fireball", "Fireball ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dash, "Ability.Dash", "Dash ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shield, "Ability.Shield", "Shield ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_LightningStorm, "Ability.LightningStorm", "Lightning storm area ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Passive_Overload, "Ability.Passive.Overload", "Passive elemental Overload ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Physical, "Damage.Physical", "Physical damage type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Fire, "Damage.Fire", "Fire damage type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Lightning, "Damage.Lightning", "Lightning damage type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Secondary, "Damage.Secondary", "Secondary damage that passive triggers may choose to ignore.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_BasicAttack, "Cooldown.BasicAttack", "Basic attack cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Fireball, "Cooldown.Fireball", "Fireball cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dash, "Cooldown.Dash", "Dash cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Shield, "Cooldown.Shield", "Shield cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_LightningStorm, "Cooldown.LightningStorm", "Lightning storm cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Enemy_MeleeAttack, "Ability.Enemy.MeleeAttack", "Enemy melee attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Enemy_MeleeAttack, "Cooldown.Enemy.MeleeAttack", "Enemy melee attack cooldown.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_BasicAttack_Activate, "GameplayCue.Ability.BasicAttack.Activate", "Basic attack activation presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_Fireball_Cast, "GameplayCue.Ability.Fireball.Cast", "Confirmed fireball cast presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_Dash_Active, "GameplayCue.Ability.Dash.Active", "Dash active looping presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_Shield_Active, "GameplayCue.Ability.Shield.Active", "Shield active looping presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_LightningStorm_Cast, "GameplayCue.Ability.LightningStorm.Cast", "Confirmed lightning storm cast presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_LightningStorm_Active, "GameplayCue.Ability.LightningStorm.Active", "Lightning storm area looping presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Ability_EnemyMelee_Activate, "GameplayCue.Ability.EnemyMelee.Activate", "Enemy melee activation presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Physical, "GameplayCue.Hit.Physical", "Confirmed physical damage hit presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Fire, "GameplayCue.Hit.Fire", "Confirmed fire damage hit presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Lightning, "GameplayCue.Hit.Lightning", "Confirmed lightning damage hit presentation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Combat, "Phase.Combat", "A combat wave is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Upgrade, "Phase.Upgrade", "Players are choosing upgrades.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Victory, "Phase.Victory", "All configured waves are complete.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Defeat, "Phase.Defeat", "All participating players are dead.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Fire, "Build.Fire", "Player owns progress in the fire build path.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Lightning, "Build.Lightning", "Player owns progress in the lightning build path.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Crit, "Build.Crit", "Player owns progress in the critical-hit build path.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Shield, "Build.Shield", "Player owns progress in the shield build path.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Dash, "Build.Dash", "Player owns progress in the dash build path.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Upgrade_Fireball_Damage, "Upgrade.Fireball.Damage", "Fireball-specific damage upgrade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Upgrade_Fireball_Burning, "Upgrade.Fireball.Burning", "Fireball applies the Burning status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Upgrade_LightningStorm_Damage, "Upgrade.LightningStorm.Damage", "LightningStorm-specific damage upgrade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Upgrade_LightningStorm_Shocked, "Upgrade.LightningStorm.Shocked", "LightningStorm applies the Shocked status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Upgrade_Combo_Overload, "Upgrade.Combo.Overload", "Fire and Lightning legendary Overload upgrade.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Burning, "Status.Burning", "Target is taking periodic fire damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Shocked, "Status.Shocked", "Target takes increased Lightning damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Overload_Lockout, "Status.Overload.Lockout", "Per-source Overload trigger lockout on a target.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Status_Burning_Active, "GameplayCue.Status.Burning.Active", "Looping Burning status presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Status_Shocked_Active, "GameplayCue.Status.Shocked.Active", "Looping Shocked status presentation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combo_Overload, "GameplayCue.Combo.Overload", "Overload explosion presentation at a world location.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trigger_OnDamageDealt, "Trigger.OnDamageDealt", "Authoritative damage dealt event.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trigger_OnDamageDealt_Physical, "Trigger.OnDamageDealt.Physical", "Authoritative physical damage dealt event.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trigger_OnDamageDealt_Fire, "Trigger.OnDamageDealt.Fire", "Authoritative fire damage dealt event.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trigger_OnDamageDealt_Lightning, "Trigger.OnDamageDealt.Lightning", "Authoritative lightning damage dealt event.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_Base, "SetByCaller.Damage.Base", "Runtime base damage value.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_SkillMultiplier, "SetByCaller.Damage.SkillMultiplier", "Runtime skill damage multiplier.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_Burning, "SetByCaller.Damage.Burning", "Runtime Burning damage per stack and tick.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Status_Shocked_LightningDamageBonus, "SetByCaller.Status.Shocked.LightningDamageBonus", "Runtime Lightning vulnerability granted by Shocked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Recovery_Health, "SetByCaller.Recovery.Health", "Runtime Health recovery applied after an upgrade choice.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Recovery_Energy, "SetByCaller.Recovery.Energy", "Runtime Energy recovery applied after an upgrade choice.");
}
