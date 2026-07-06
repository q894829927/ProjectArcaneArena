#include "UI/ArenaDamageNumberActor.h"

#include "Components/WidgetComponent.h"
#include "UI/ArenaDamageNumberWidget.h"

AArenaDamageNumberActor::AArenaDamageNumberActor()
{
	PrimaryActorTick.bCanEverTick = true;
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

	UArenaDamageNumberWidget* DamageNumberWidget = Cast<UArenaDamageNumberWidget>(
		WidgetComponent->GetUserWidgetObject());
	if (DamageNumberWidget)
	{
		DamageNumberWidget->SetDamageAmount(DamageAmount);
	}
}
