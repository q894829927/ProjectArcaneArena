#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "ArenaHitReactionComponent.generated.h"

class AArenaDamageNumberActor;
class UCameraShakeBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;

UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class PROJECTARCANEARENA_API UArenaHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaHitReactionComponent();

	// 在各客户端消费服务器确认的反馈，统一播放材质、数字、音效和本地玩家反馈。
	void PresentDamageFeedback(const FArenaDamageFeedbackData& DamageFeedback);

	// 汇总同一目标本 Tick 的角色反应，同时保留每段伤害各自的世界数字。
	void PresentDamageFeedbackBatch(const TArray<FArenaDamageFeedbackData>& DamageFeedbackBatch);

	// 播放服务器汇总后的唯一结果音层；由独立可靠 RPC 调用，避免视觉丢包同时吞掉命中声音。
	void PresentDamageFeedbackSound(EArenaDamageFeedbackType FeedbackType) const;

	// 为旧 Damage Number Cue 保留单一兼容入口，实际 Actor 仍由本组件创建。
	void SpawnDamageNumber(float DamageAmount, bool bCriticalHit, EArenaDamageFeedbackType FeedbackType);

protected:
	// 组件结束时恢复材质参数并清理表现定时器。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Number")
	TSubclassOf<AArenaDamageNumberActor> DamageNumberActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Number")
	FVector DamageNumberSpawnOffset = FVector(0.0f, 0.0f, 130.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Number", meta = (ClampMin = "0.0"))
	float DamageNumberLaneSpacing = 26.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material")
	TObjectPtr<UMaterialInterface> HitFlashOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material")
	FName HitFlashColorParameterName = TEXT("HitFlashColor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material")
	FName HitFlashIntensityParameterName = TEXT("HitFlashIntensity");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material")
	FLinearColor ShieldHitFlashColor = FLinearColor(0.0f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material")
	FLinearColor ShieldBreakFlashColor = FLinearColor(0.7f, 0.95f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material")
	FLinearColor HealthHitFlashColor = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material", meta = (ClampMin = "0.0"))
	float ShieldHitFlashIntensity = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material", meta = (ClampMin = "0.0"))
	float ShieldBreakFlashIntensity = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material", meta = (ClampMin = "0.0"))
	float HealthHitFlashIntensity = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material", meta = (ClampMin = "0.0"))
	float ShieldHitFlashDuration = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material", meta = (ClampMin = "0.0"))
	float ShieldBreakFlashDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Material", meta = (ClampMin = "0.0"))
	float HealthHitFlashDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Audio")
	TObjectPtr<USoundBase> ShieldHitSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Audio")
	TObjectPtr<USoundBase> ShieldBreakSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Audio")
	TObjectPtr<USoundBase> HealthHitSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Audio")
	TObjectPtr<USoundAttenuation> HitFeedbackAttenuationSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera")
	TSubclassOf<UCameraShakeBase> LightDamageCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera")
	TSubclassOf<UCameraShakeBase> MediumDamageCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera")
	TSubclassOf<UCameraShakeBase> HeavyDamageCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera")
	TSubclassOf<UCameraShakeBase> ShieldBreakCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ShieldHitCameraShakeScale = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ShieldBreakBaseShakeScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera", meta = (ClampMin = "0.0"))
	float HealthDamageShakeMultiplier = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float MaxCameraShakeScale = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Feedback|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ThirdPersonCameraShakeScaleMultiplier = 0.65f;

private:
	// 首次受击时创建项目 Overlay MID，避免改写角色的第三方主材质。
	void EnsureHitFlashOverlay();
	// 根据汇总反馈类型设置 Overlay 参数，并刷新复位定时器。
	void ApplyMaterialFlash(EArenaDamageFeedbackType FeedbackType);
	// 复位 Overlay 强度，并仅在未被其他系统替换时恢复原 Overlay。
	void ResetMaterialFlash();
	// 为本地控制玩家播放一次批次级 CameraShake 并通知 HUD。
	void PresentLocalPlayerFeedback(const FArenaDamageFeedbackData& DamageFeedback);
	// 根据生命损失比例选择轻、中、重 CameraShake 类。
	TSubclassOf<UCameraShakeBase> ResolveHealthCameraShake(float HealthDamageRatio) const;
	// 获取组件配置或角色兼容配置中的伤害数字类与偏移。
	TSubclassOf<AArenaDamageNumberActor> ResolveDamageNumberClass() const;
	FVector ResolveDamageNumberOffset() const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CachedHitFlashMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalOverlayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitFlashMaterialInstance;

	FTimerHandle HitFlashTimerHandle;
	bool bHitFlashOverlayApplied = false;
	int32 DamageNumberSequence = 0;
};
