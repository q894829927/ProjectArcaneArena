#include "GAS/Targeting/ArenaTargetActor_MouseGround.h"

#include "Abilities/GameplayAbility.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
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
	if (!GetViewAimLocation(TargetLocation))
	{
		CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
		return;
	}

	TargetDataReadyDelegate.Broadcast(MakeLocationTargetData(TargetLocation));
}

// 根据玩家当前视角读取鼠标或中心准星命中点，并提供不信任客户端距离的兜底方向。
bool AArenaTargetActor_MouseGround::GetViewAimLocation(FVector& OutTargetLocation) const
{
	const UGameplayAbility* Ability = OwningAbility;
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	APlayerController* PlayerController = PrimaryPC.Get() ? PrimaryPC.Get() : (ActorInfo ? ActorInfo->PlayerController.Get() : nullptr);
	const AActor* AvatarActor = SourceActor.Get() ? SourceActor.Get() : (ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	const AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(AvatarActor);
	const bool bUseCenterScreenAim = PlayerCharacter && PlayerCharacter->IsUsingThirdPersonView();

	if (PlayerController)
	{
		FHitResult AimHit;
		bool bHasAimHit = false;
		if (bUseCenterScreenAim)
		{
			int32 ViewportSizeX = 0;
			int32 ViewportSizeY = 0;
			PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
			FVector TraceOrigin = FVector::ZeroVector;
			FVector TraceDirection = FVector::ZeroVector;
			if (ViewportSizeX > 0
				&& ViewportSizeY > 0
				&& PlayerController->DeprojectScreenPositionToWorld(
					ViewportSizeX * 0.5f,
					ViewportSizeY * 0.5f,
					TraceOrigin,
					TraceDirection))
			{
				if (UWorld* World = PlayerController->GetWorld())
				{
					FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaCenterScreenAim), true, AvatarActor);
					bHasAimHit = World->LineTraceSingleByChannel(
						AimHit,
						TraceOrigin,
						TraceOrigin + TraceDirection * CenterScreenTraceDistance,
						TraceChannel.GetValue(),
						QueryParams);
				}
			}
		}
		else
		{
			const ETraceTypeQuery TraceType = UEngineTypes::ConvertToTraceType(TraceChannel.GetValue());
			bHasAimHit = PlayerController->GetHitResultUnderCursorByChannel(TraceType, true, AimHit);
		}

		if (bHasAimHit && AimHit.bBlockingHit)
		{
			OutTargetLocation = AimHit.Location;
			return true;
		}
	}

	if (!AvatarActor)
	{
		return false;
	}

	FVector FallbackDirection = AvatarActor->GetActorForwardVector();
	if (bUseCenterScreenAim && PlayerController)
	{
		FallbackDirection = PlayerController->GetControlRotation().Vector();
	}
	FallbackDirection.Z = 0.0f;
	FallbackDirection = FallbackDirection.GetSafeNormal();
	if (FallbackDirection.IsNearlyZero())
	{
		return false;
	}

	// 未命中时只提供水平目标点，具体最大距离仍由服务端 Ability 校验和截断。
	OutTargetLocation = AvatarActor->GetActorLocation() + FallbackDirection * FallbackDistance;
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
