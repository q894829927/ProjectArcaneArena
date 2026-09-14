#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "ArenaDamageFeedbackTypes.generated.h"

// 描述一次权威伤害最终消耗了哪类资源，客户端只据此选择表现。
UENUM(BlueprintType)
enum class EArenaDamageFeedbackType : uint8
{
	None,
	ShieldOnly,
	ShieldBreak,
	HealthOnly,
	ShieldBreakWithHealthDamage
};

// 保存一段独立伤害结算的权威表现数据，批量 RPC 只合并传输次数。
USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaDamageFeedbackData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EArenaDamageFeedbackType FeedbackType = EArenaDamageFeedbackType::None;

	UPROPERTY(BlueprintReadOnly)
	float ActualShieldDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float ActualHealthDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float HealthDamageRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FVector_NetQuantize DamageSourceLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	bool bHasDamageSourceLocation = false;

	// 保留 GAS 标准上下文、命中位置、标签与总实际伤害，供现有 Cue 继续消费。
	UPROPERTY(BlueprintReadOnly)
	FGameplayCueParameters CueParameters;

	// 返回本次真正消耗的 Shield 与 Health 总和，用于唯一伤害数字。
	float GetTotalDamage() const
	{
		return ActualShieldDamage + ActualHealthDamage;
	}
};
