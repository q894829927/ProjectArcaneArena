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

// 开始播放时设置生命周期，并把当前伤害数值写入 Widget。
void AArenaDamageNumberActor::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpan);
	SetDamagePresentation(DamageAmount, bCriticalHit);
}

// 每帧驱动伤害数字上浮表现，后续可替换为动画。
void AArenaDamageNumberActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 临时反馈采用简单上浮，后续可替换为 UMG 动画或 GameplayCue。
	AddActorWorldOffset(FVector::UpVector * FloatSpeed * DeltaSeconds, false);
}

// 保留原有蓝图接口，未指定样式时按普通伤害显示。
void AArenaDamageNumberActor::SetDamageAmount(float InDamageAmount)
{
	SetDamagePresentation(InDamageAmount, false);
}

// 设置伤害数字与暴击样式，并为暴击扩展绘制区域以避免大字号被裁切。
void AArenaDamageNumberActor::SetDamagePresentation(float InDamageAmount, bool bInCriticalHit)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);
	bCriticalHit = bInCriticalHit;

	if (!WidgetComponent)
	{
		return;
	}

	if (CachedBaseDrawSize.IsNearlyZero())
	{
		CachedBaseDrawSize = WidgetComponent->GetDrawSize();
	}
	const FVector2D PresentationDrawSize = bCriticalHit
		? CachedBaseDrawSize * 1.35
		: CachedBaseDrawSize;
	WidgetComponent->SetDrawSize(PresentationDrawSize);

	WidgetComponent->InitWidget();

	// WidgetClass 可由蓝图配置，C++ 只在存在对应基类时写入数值。
	UArenaDamageNumberWidget* DamageNumberWidget = Cast<UArenaDamageNumberWidget>(
		WidgetComponent->GetUserWidgetObject());
	if (DamageNumberWidget)
	{
		DamageNumberWidget->SetDamagePresentation(DamageAmount, bCriticalHit);
	}
}
