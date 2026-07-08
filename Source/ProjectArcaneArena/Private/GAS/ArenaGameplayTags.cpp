#include "GAS/ArenaGameplayTags.h"

namespace ArenaGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character is dead and cannot act.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Character is stunned and cannot move or attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invincible, "State.Invincible", "Character ignores normal damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dashing, "State.Dashing", "Character is currently dashing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Casting, "State.Casting", "Character is currently casting.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_BasicAttack, "Ability.BasicAttack", "Basic attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fireball, "Ability.Fireball", "Fireball ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dash, "Ability.Dash", "Dash ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shield, "Ability.Shield", "Shield ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Physical, "Damage.Physical", "Physical damage type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Fire, "Damage.Fire", "Fire damage type.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_BasicAttack, "Cooldown.BasicAttack", "Basic attack cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Fireball, "Cooldown.Fireball", "Fireball cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dash, "Cooldown.Dash", "Dash cooldown.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Shield, "Cooldown.Shield", "Shield cooldown.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_Base, "SetByCaller.Damage.Base", "Runtime base damage value.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage_SkillMultiplier, "SetByCaller.Damage.SkillMultiplier", "Runtime skill damage multiplier.");
}
