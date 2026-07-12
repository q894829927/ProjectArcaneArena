#include "GAS/Targeting/ArenaTargetData_DashDirection.h"

bool FGameplayAbilityTargetData_DashDirection::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Direction.NetSerialize(Ar, Map, bOutSuccess);
	return true;
}
