#include "GAS/ArenaGameplayCueNotify_DamageNumber.h"

#include "Character/ArenaEnemyCharacter.h"

// Cue 只消费服务器复制的实际伤害值，本地 Actor 不反向影响任何玩法状态。
bool UArenaGameplayCueNotify_DamageNumber::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	AArenaEnemyCharacter* EnemyCharacter = Cast<AArenaEnemyCharacter>(MyTarget);
	if (!EnemyCharacter || Parameters.RawMagnitude <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	EnemyCharacter->SpawnDamageNumber(Parameters.RawMagnitude, bCriticalStyle);
	return true;
}
