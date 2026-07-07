#include "UI/ArenaDamageNumberActor.h"

#include "Components/WidgetComponent.h"
#include "UI/ArenaDamageNumberWidget.h"

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

void AArenaDamageNumberActor::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpan);
	SetDamageAmount(DamageAmount);
}

void AArenaDamageNumberActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 临时反馈采用简单上浮，后续可替换为 UMG 动画或 GameplayCue。
	AddActorWorldOffset(FVector::UpVector * FloatSpeed * DeltaSeconds, false);
}

void AArenaDamageNumberActor::SetDamageAmount(float InDamageAmount)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);

	if (!WidgetComponent)
	{
		return;
	}

	WidgetComponent->InitWidget();

	// WidgetClass 可由蓝图配置，C++ 只在存在对应基类时写入数值。
	UArenaDamageNumberWidget* DamageNumberWidget = Cast<UArenaDamageNumberWidget>(
		WidgetComponent->GetUserWidgetObject());
	if (DamageNumberWidget)
	{
		DamageNumberWidget->SetDamageAmount(DamageAmount);
	}
}
