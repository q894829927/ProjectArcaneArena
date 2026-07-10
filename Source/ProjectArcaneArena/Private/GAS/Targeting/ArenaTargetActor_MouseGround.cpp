#include "GAS/Targeting/ArenaTargetActor_MouseGround.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PlayerController.h"

// 构造鼠标地面目标 Actor，配置客户端产出 TargetData 的即时目标选择。
AArenaTargetActor_MouseGround::AArenaTargetActor_MouseGround()
{
	PrimaryActorTick.bCanEverTick = false;

	// Fireball 需要客户端鼠标位置，服务端只接收并校验 TargetData。
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;
}

// 开始目标选择时缓存 Avatar，供鼠标未命中时计算后备目标点。
void AArenaTargetActor_MouseGround::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);

	SourceActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
}

// 确认目标选择，将鼠标地面位置打包成 TargetData 广播给 Ability。
void AArenaTargetActor_MouseGround::ConfirmTargetingAndContinue()
{
	FVector TargetLocation = FVector::ZeroVector;
	if (!GetMouseGroundLocation(TargetLocation))
	{
		CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
		return;
	}

	TargetDataReadyDelegate.Broadcast(MakeLocationTargetData(TargetLocation));
}

// 获取鼠标下方地面命中点，失败时回退到角色前方位置。
bool AArenaTargetActor_MouseGround::GetMouseGroundLocation(FVector& OutTargetLocation) const
{
	const UGameplayAbility* Ability = OwningAbility;
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	APlayerController* PlayerController = PrimaryPC.Get() ? PrimaryPC.Get() : (ActorInfo ? ActorInfo->PlayerController.Get() : nullptr);

	if (PlayerController)
	{
		FHitResult CursorHit;
		const ETraceTypeQuery TraceType = UEngineTypes::ConvertToTraceType(TraceChannel.GetValue());
		if (PlayerController->GetHitResultUnderCursorByChannel(TraceType, true, CursorHit) && CursorHit.bBlockingHit)
		{
			OutTargetLocation = CursorHit.Location;
			return true;
		}
	}

	const AActor* AvatarActor = SourceActor.Get() ? SourceActor.Get() : (ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!AvatarActor)
	{
		return false;
	}

	// 鼠标没有命中地面时仍给服务端一个方向，避免技能因视口/碰撞配置暂时失效。
	OutTargetLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * FallbackDistance;
	return true;
}

// 将目标世界坐标封装为 GAS Location TargetData。
FGameplayAbilityTargetDataHandle AArenaTargetActor_MouseGround::MakeLocationTargetData(const FVector& TargetLocation) const
{
	FGameplayAbilityTargetingLocationInfo SourceLocation;
	SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	const AActor* SourceActorPtr = SourceActor.Get();
	SourceLocation.LiteralTransform = SourceActorPtr
		? SourceActorPtr->GetActorTransform()
		: FTransform(FRotator::ZeroRotator, TargetLocation);

	FGameplayAbilityTargetingLocationInfo TargetLocationInfo;
	TargetLocationInfo.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	TargetLocationInfo.LiteralTransform = FTransform(FRotator::ZeroRotator, TargetLocation);

	FGameplayAbilityTargetData_LocationInfo* LocationData = new FGameplayAbilityTargetData_LocationInfo();
	LocationData->SourceLocation = SourceLocation;
	LocationData->TargetLocation = TargetLocationInfo;

	return FGameplayAbilityTargetDataHandle(LocationData);
}
