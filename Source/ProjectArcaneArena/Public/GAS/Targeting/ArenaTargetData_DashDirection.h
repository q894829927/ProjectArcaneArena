#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"
#include "ArenaTargetData_DashDirection.generated.h"

USTRUCT()
struct PROJECTARCANEARENA_API FGameplayAbilityTargetData_DashDirection : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector_NetQuantizeNormal(1.0, 0.0, 0.0);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("DashDirection: %s"), *Direction.ToString());
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_DashDirection>
	: public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_DashDirection>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
