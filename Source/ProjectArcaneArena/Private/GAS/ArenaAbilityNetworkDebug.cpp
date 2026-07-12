#include "GAS/ArenaAbilityNetworkDebug.h"

#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogArenaAbilityNet);

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarArenaRejectNextAbility(
		TEXT("arena.Net.RejectNextAbility"),
		0,
		TEXT("Reject the next matching predicted ability on the server: 1 Basic, 2 Fireball, 3 Dash, 4 Shield, 5 Storm."),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarArenaAbilityAudit(
		TEXT("arena.Net.AbilityAudit"),
		0,
		TEXT("Log predicted ability activation, authoritative spawn and damage audit events."),
		ECVF_Cheat);
#endif

	TAtomic<uint64> ServerExecutionSequence(0);
}

bool ArenaAbilityNetworkDebug::ConsumeServerRejection(EArenaNetworkAbilityId AbilityId)
{
#if !UE_BUILD_SHIPPING
	const int32 RequestedAbility = CVarArenaRejectNextAbility.GetValueOnGameThread();
	if (RequestedAbility == static_cast<int32>(AbilityId) && AbilityId != EArenaNetworkAbilityId::None)
	{
		CVarArenaRejectNextAbility->Set(0, ECVF_SetByConsole);
		UE_LOG(LogArenaAbilityNet, Warning, TEXT("Forced server rejection consumed for ability id %d."), RequestedAbility);
		return true;
	}
#endif

	return false;
}

bool ArenaAbilityNetworkDebug::IsAuditEnabled()
{
#if !UE_BUILD_SHIPPING
	return CVarArenaAbilityAudit.GetValueOnAnyThread() != 0;
#else
	return false;
#endif
}

uint64 ArenaAbilityNetworkDebug::NextServerExecutionSequence()
{
	return ++ServerExecutionSequence;
}
