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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Physical, "Damage.Physical", "Physical damage type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Fire, "Damage.Fire", "Fire damage type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Lightning, "Damage.Lightning", "Lightning damage type.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_BasicAttack, "Cooldown.BasicAttack", "Basic attack cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Fireball, "Cooldown.Fireball", "Fireball cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dash, "Cooldown.Dash", "Dash cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Shield, "Cooldown.Shield", "Shield cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_LightningStorm, "Cooldown.LightningStorm", "Lightning storm cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Enemy_MeleeAttack, "Ability.Enemy.MeleeAttack", "Enemy melee attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Enemy_MeleeAttack, "Cooldown.Enemy.MeleeAttack", "Enemy melee attack cooldown.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Combat, "Phase.Combat", "A combat wave is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Upgrade, "Phase.Upgrade", "Players are choosing upgrades.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Victory, "Phase.Victory", "All configured waves are complete.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase_Defeat, "Phase.Defeat", "All participating players are dead.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_Base, "SetByCaller.Damage.Base", "Runtime base damage value.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_SkillMultiplier, "SetByCaller.Damage.SkillMultiplier", "Runtime skill damage multiplier.");
}
