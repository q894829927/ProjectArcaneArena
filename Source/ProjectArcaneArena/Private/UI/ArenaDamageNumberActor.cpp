#include "UI/ArenaDamageNumberActor.h"

#include "Components/WidgetComponent.h"
#include "UI/ArenaDamageNumberWidget.h"

// 构造伤害数字表现 Actor，创建屏幕空间 Widget 并关闭复制。
AArenaDamageNumberActor::AArenaDamageNumberActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// 伤害数字只作为本地表现生成，不需要网络复制。
	bReplicates = false;

	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageNumberWidget"));
	SetRootComponent(WidgetComponent);
	WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComponent->SetDrawSize(FVector2D(80.0f, 32.0f));
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComponent->SetGenerateOverlapEvents(false);

	SetCanBeDamaged(false);
	InitialLifeSpan = LifeSpan;
}

// 开始播放时缓存固定起点、设置生命周期，并把当前伤害数值写入 Widget。
void AArenaDamageNumberActor::BeginPlay()
{
	Super::BeginPlay();

	PresentationStartLocation = GetActorLocation();
	ElapsedPresentationTime = 0.0f;
	if (WidgetComponent)
	{
		WidgetComponent->SetTintColorAndOpacity(FLinearColor::White);
	}
	SetLifeSpan(LifeSpan);
	SetDamagePresentation(DamageAmount, bCriticalHit);
}

// 每帧按归一化生命周期驱动 Ease-Out 上浮和末段渐隐，不受后续目标移动影响。
void AArenaDamageNumberActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedPresentationTime += FMath::Max(DeltaSeconds, 0.0f);
	const float SafeLifeSpan = FMath::Max(LifeSpan, KINDA_SMALL_NUMBER);
	const float NormalizedTime = FMath::Clamp(ElapsedPresentationTime / SafeLifeSpan, 0.0f, 1.0f);
	const float EaseOutAlpha = 1.0f - FMath::Pow(1.0f - NormalizedTime, 3.0f);
	const float RiseDistance = FloatSpeed * SafeLifeSpan;
	SetActorLocation(PresentationStartLocation + FVector::UpVector * RiseDistance * EaseOutAlpha);

	if (WidgetComponent)
	{
		const float SafeFadeStart = FMath::Clamp(FadeStartNormalized, 0.0f, 0.95f);
		const float FadeAlpha = NormalizedTime <= SafeFadeStart
			? 1.0f
			: 1.0f - (NormalizedTime - SafeFadeStart) / FMath::Max(1.0f - SafeFadeStart, KINDA_SMALL_NUMBER);
		WidgetComponent->SetTintColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(FadeAlpha, 0.0f, 1.0f)));
	}
}

// 保留原有蓝图接口，未指定样式时按普通伤害显示。
void AArenaDamageNumberActor::SetDamageAmount(float InDamageAmount)
{
	SetDamagePresentation(InDamageAmount, false);
}

// 设置伤害数字与暴击样式，并为暴击扩展绘制区域以避免大字号被裁切。
void AArenaDamageNumberActor::SetDamagePresentation(float InDamageAmount, bool bInCriticalHit)
{
	SetDamageFeedbackPresentation(InDamageAmount, bInCriticalHit, EArenaDamageFeedbackType::HealthOnly);
}

// 设置总实际损失、暴击与资源分类，并适当扩大破盾数字绘制区域。
void AArenaDamageNumberActor::SetDamageFeedbackPresentation(
	float InDamageAmount,
	bool bInCriticalHit,
	EArenaDamageFeedbackType InFeedbackType)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);
	bCriticalHit = bInCriticalHit;
	FeedbackType = InFeedbackType;

	if (!WidgetComponent)
	{
		return;
	}

	if (CachedBaseDrawSize.IsNearlyZero())
	{
		CachedBaseDrawSize = WidgetComponent->GetDrawSize();
	}
	const bool bEmphasized = bCriticalHit
		|| FeedbackType == EArenaDamageFeedbackType::ShieldBreak
		|| FeedbackType == EArenaDamageFeedbackType::ShieldBreakWithHealthDamage;
	const FVector2D PresentationDrawSize = bEmphasized ? CachedBaseDrawSize * 1.35 : CachedBaseDrawSize;
	WidgetComponent->SetDrawSize(PresentationDrawSize);

	WidgetComponent->InitWidget();

	// WidgetClass 可由蓝图配置，C++ 只在存在对应基类时写入数值。
	UArenaDamageNumberWidget* DamageNumberWidget = Cast<UArenaDamageNumberWidget>(
		WidgetComponent->GetUserWidgetObject());
	if (DamageNumberWidget)
	{
		DamageNumberWidget->SetDamageFeedbackPresentation(DamageAmount, bCriticalHit, FeedbackType);
	}
}
