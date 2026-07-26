#include "UI/ArenaPlayerHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/ArenaBossCharacter.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Core/ArenaGameState.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

namespace
{
	// 计算当前值相对最大值的 UI 百分比，最大值无效时显示为空。
	float CalculatePercent(float CurrentValue, float MaxValue)
	{
		return MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;
	}

	// 生成生命/能量这类 Current/Max 属性的显示文本。
	FText MakeAttributeValueText(const FText& Label, float CurrentValue, float MaxValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "AttributeValueFormat", "{0} {1} / {2}"),
			Label,
			FText::AsNumber(FMath::RoundToInt(CurrentValue)),
			FText::AsNumber(FMath::RoundToInt(MaxValue)));
	}

	// 生成护盾显示文本，护盾当前没有 MaxShield。
	FText MakeShieldValueText(float CurrentValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldValueFormat", "Shield {0}"),
			FText::AsNumber(FMath::RoundToInt(CurrentValue)));
	}

	// 将冷却剩余时间格式化为一位小数秒数。
	FText MakeCooldownSecondsText(float RemainingTime)
	{
		FNumberFormattingOptions NumberFormat;
		NumberFormat.MinimumFractionalDigits = 1;
		NumberFormat.MaximumFractionalDigits = 1;

		return FText::AsNumber(FMath::Max(RemainingTime, 0.0f), &NumberFormat);
	}

	// 生成基础攻击完整冷却文本，用于主冷却提示。
	FText MakeBasicAttackFullText(bool bIsCooldownActive, float RemainingTime)
	{
		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackCooldownFormat", "LMB Basic {0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackReady", "LMB Basic Ready");
	}

	// 生成基础攻击技能槽文本，用于槽位上的简短状态。
	FText MakeBasicAttackSlotText(bool bIsCooldownActive, float RemainingTime)
	{
		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackSlotReady", "Basic Ready");
	}

	// 生成火球技能槽文本，兼顾未解锁和冷却状态。
	FText MakeFireballSlotText(bool bHasFireballAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasFireballAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "FireballSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "FireballSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "FireballSlotReady", "Fireball Ready");
	}

	// 生成冲刺技能槽文本，兼顾未解锁和冷却状态。
	FText MakeDashSlotText(bool bHasDashAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasDashAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotReady", "Dash Ready");
	}

	// 生成护盾技能槽文本，兼顾未解锁和冷却状态。
	FText MakeShieldSlotText(bool bHasShieldAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasShieldAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotReady", "Shield Ready");
	}

	// 生成闪电风暴技能槽文本，显示授予状态和冷却倒计时。
	FText MakeLightningStormSlotText(bool bHasLightningStormAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasLightningStormAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "LightningStormSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "LightningStormSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "LightningStormSlotReady", "Storm Ready");
	}
}

// 初始化 HUD，并为蓝图未提供的准星、阶段、波次、随机种子和 Boss Intro 控件创建运行时回退显示。
void UArenaPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UCanvasPanel* RootCanvas = WidgetTree ? Cast<UCanvasPanel>(GetRootWidget()) : nullptr;
	if (!AimReticleText && WidgetTree && RootCanvas)
	{
		AimReticleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AimReticleText_Runtime"));
		AimReticleText->SetText(NSLOCTEXT("ArenaPlayerHUDWidget", "ThirdPersonReticle", "+"));
		AimReticleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo ReticleFont = AimReticleText->GetFont();
		ReticleFont.Size = 24;
		ReticleFont.OutlineSettings.OutlineSize = 1;
		AimReticleText->SetFont(ReticleFont);

		if (UCanvasPanelSlot* ReticleSlot = RootCanvas->AddChildToCanvas(AimReticleText))
		{
			ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ReticleSlot->SetPosition(FVector2D::ZeroVector);
			ReticleSlot->SetAutoSize(true);
		}
	}

	if (WidgetTree && RootCanvas)
	{
		// 阶段控件缺失时在顶部集中显示服务器复制状态，便于原型和多人验收。
		auto CreateRuntimeStateText = [this, RootCanvas](FName WidgetName, float PositionY) -> UTextBlock*
		{
			UTextBlock* RuntimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
			RuntimeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			RuntimeText->SetVisibility(ESlateVisibility::HitTestInvisible);

			FSlateFontInfo StateFont = RuntimeText->GetFont();
			StateFont.Size = 18;
			StateFont.OutlineSettings.OutlineSize = 1;
			RuntimeText->SetFont(StateFont);

			if (UCanvasPanelSlot* RuntimeSlot = RootCanvas->AddChildToCanvas(RuntimeText))
			{
				RuntimeSlot->SetAnchors(FAnchors(0.5f, 0.0f));
				RuntimeSlot->SetAlignment(FVector2D(0.5f, 0.0f));
				RuntimeSlot->SetPosition(FVector2D(0.0f, PositionY));
				RuntimeSlot->SetAutoSize(true);
			}
			return RuntimeText;
		};

		if (!PhaseText)
		{
			PhaseText = CreateRuntimeStateText(TEXT("PhaseText_Runtime"), 24.0f);
		}
		if (!WaveText)
		{
			WaveText = CreateRuntimeStateText(TEXT("WaveText_Runtime"), 48.0f);
		}
		if (!RemainingEnemiesText)
		{
			RemainingEnemiesText = CreateRuntimeStateText(TEXT("RemainingEnemiesText_Runtime"), 72.0f);
		}

		if (!RandomSeedText)
		{
			RandomSeedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RandomSeedText_Runtime"));
			RandomSeedText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));
			RandomSeedText->SetVisibility(ESlateVisibility::HitTestInvisible);

			FSlateFontInfo SeedFont = RandomSeedText->GetFont();
			SeedFont.Size = 16;
			SeedFont.OutlineSettings.OutlineSize = 1;
			RandomSeedText->SetFont(SeedFont);

			if (UCanvasPanelSlot* SeedSlot = RootCanvas->AddChildToCanvas(RandomSeedText))
			{
				// 右上角使用负 X 边距，避免贴住视口边缘或随文本长度改变锚点位置。
				SeedSlot->SetAnchors(FAnchors(1.0f, 0.0f));
				SeedSlot->SetAlignment(FVector2D(1.0f, 0.0f));
				SeedSlot->SetPosition(FVector2D(-24.0f, 24.0f));
				SeedSlot->SetAutoSize(true);
			}
		}
	}

	if (!DamageDirectionIndicator && WidgetTree && RootCanvas)
	{
		UTextBlock* RuntimeDirectionIndicator = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("DamageDirectionIndicator_Runtime"));
		RuntimeDirectionIndicator->SetText(FText::FromString(TEXT("^")));
		RuntimeDirectionIndicator->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.05f, 0.02f, 1.0f)));
		FSlateFontInfo DirectionFont = RuntimeDirectionIndicator->GetFont();
		DirectionFont.Size = 38;
		DirectionFont.OutlineSettings.OutlineSize = 2;
		RuntimeDirectionIndicator->SetFont(DirectionFont);
		if (UCanvasPanelSlot* DirectionSlot = RootCanvas->AddChildToCanvas(RuntimeDirectionIndicator))
		{
			DirectionSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			DirectionSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DirectionSlot->SetPosition(FVector2D(0.0f, -220.0f));
			DirectionSlot->SetAutoSize(true);
		}
		DamageDirectionIndicator = RuntimeDirectionIndicator;
		bUsesRuntimeDamageDirectionIndicator = true;
	}

	if (!ShieldBreakText && WidgetTree && RootCanvas)
	{
		ShieldBreakText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("ShieldBreakText_Runtime"));
		ShieldBreakText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.95f, 1.0f, 1.0f)));
		FSlateFontInfo ShieldBreakFont = ShieldBreakText->GetFont();
		ShieldBreakFont.Size = 24;
		ShieldBreakFont.OutlineSettings.OutlineSize = 2;
		ShieldBreakText->SetFont(ShieldBreakFont);
		if (UCanvasPanelSlot* ShieldBreakSlot = RootCanvas->AddChildToCanvas(ShieldBreakText))
		{
			ShieldBreakSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ShieldBreakSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ShieldBreakSlot->SetPosition(FVector2D(0.0f, -110.0f));
			ShieldBreakSlot->SetAutoSize(true);
		}
	}

	if (WidgetTree && RootCanvas && !BossPanel && !BossNameText && !BossHealthProgressBar && !BossHealthText)
	{
		// 蓝图尚未补 Boss 控件时创建可直接验收的数据面板，不把布局状态写回玩法系统。
		UVerticalBox* RuntimeBossPanel = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("BossPanel_Runtime"));
		BossPanel = RuntimeBossPanel;
		if (UCanvasPanelSlot* BossPanelSlot = RootCanvas->AddChildToCanvas(RuntimeBossPanel))
		{
			BossPanelSlot->SetAnchors(FAnchors(0.5f, 0.0f));
			BossPanelSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			BossPanelSlot->SetPosition(FVector2D(0.0f, 108.0f));
			BossPanelSlot->SetSize(FVector2D(520.0f, 94.0f));
		}

		BossNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossNameText_Runtime"));
		BossNameText->SetJustification(ETextJustify::Center);
		BossNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.45f, 1.0f)));
		FSlateFontInfo BossNameFont = BossNameText->GetFont();
		BossNameFont.Size = 22;
		BossNameFont.OutlineSettings.OutlineSize = 1;
		BossNameText->SetFont(BossNameFont);
		if (UVerticalBoxSlot* NameSlot = RuntimeBossPanel->AddChildToVerticalBox(BossNameText))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		BossPhaseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossPhaseText_Runtime"));
		BossPhaseText->SetJustification(ETextJustify::Center);
		BossPhaseText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.45f, 1.0f)));
		FSlateFontInfo BossPhaseFont = BossPhaseText->GetFont();
		BossPhaseFont.Size = 15;
		BossPhaseFont.OutlineSettings.OutlineSize = 1;
		BossPhaseText->SetFont(BossPhaseFont);
		if (UVerticalBoxSlot* PhaseSlot = RuntimeBossPanel->AddChildToVerticalBox(BossPhaseText))
		{
			PhaseSlot->SetHorizontalAlignment(HAlign_Fill);
			PhaseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}

		USizeBox* BossBarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BossHealthSizeBox_Runtime"));
		BossBarSizeBox->SetWidthOverride(520.0f);
		BossBarSizeBox->SetHeightOverride(18.0f);
		BossHealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BossHealthProgressBar_Runtime"));
		BossHealthProgressBar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.08f, 0.04f, 1.0f));
		BossBarSizeBox->AddChild(BossHealthProgressBar);
		if (UVerticalBoxSlot* BarSlot = RuntimeBossPanel->AddChildToVerticalBox(BossBarSizeBox))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Center);
		}

		BossHealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossHealthText_Runtime"));
		BossHealthText->SetJustification(ETextJustify::Center);
		BossHealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo BossHealthFont = BossHealthText->GetFont();
		BossHealthFont.Size = 16;
		BossHealthFont.OutlineSettings.OutlineSize = 1;
		BossHealthText->SetFont(BossHealthFont);
		if (UVerticalBoxSlot* HealthTextSlot = RuntimeBossPanel->AddChildToVerticalBox(BossHealthText))
		{
			HealthTextSlot->SetHorizontalAlignment(HAlign_Fill);
			HealthTextSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
		}
	}

	if (WidgetTree
		&& RootCanvas
		&& (!BossIntroText
			|| !BossIntroCountdownText
			|| !BossIntroSkipText
			|| !BossIntroSkipProgressBar))
	{
		// 蓝图缺少部分 Intro 控件时只为缺失项创建回退，不覆盖已经完成的自定义布局。
		UVerticalBox* RuntimeIntroPanel = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("BossIntroPanel_Runtime"));
		if (!BossIntroPanel)
		{
			BossIntroPanel = RuntimeIntroPanel;
		}
		if (UCanvasPanelSlot* IntroPanelSlot = RootCanvas->AddChildToCanvas(RuntimeIntroPanel))
		{
			IntroPanelSlot->SetAnchors(FAnchors(0.5f, 0.25f));
			IntroPanelSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			IntroPanelSlot->SetPosition(FVector2D::ZeroVector);
			IntroPanelSlot->SetSize(FVector2D(520.0f, 150.0f));
		}

		auto CreateIntroText = [this, RuntimeIntroPanel](
			FName WidgetName,
			int32 FontSize,
			const FLinearColor& Color) -> UTextBlock*
		{
			UTextBlock* RuntimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
			RuntimeText->SetJustification(ETextJustify::Center);
			RuntimeText->SetColorAndOpacity(FSlateColor(Color));
			FSlateFontInfo IntroFont = RuntimeText->GetFont();
			IntroFont.Size = FontSize;
			IntroFont.OutlineSettings.OutlineSize = 1;
			RuntimeText->SetFont(IntroFont);
			if (UVerticalBoxSlot* TextSlot = RuntimeIntroPanel->AddChildToVerticalBox(RuntimeText))
			{
				TextSlot->SetHorizontalAlignment(HAlign_Fill);
				TextSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 1.0f));
			}
			return RuntimeText;
		};

		if (!BossIntroText)
		{
			BossIntroText = CreateIntroText(
				TEXT("BossIntroText_Runtime"),
				28,
				FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));
		}
		if (!BossIntroCountdownText)
		{
			BossIntroCountdownText = CreateIntroText(
				TEXT("BossIntroCountdownText_Runtime"),
				20,
				FLinearColor::White);
		}
		if (!BossIntroSkipText)
		{
			BossIntroSkipText = CreateIntroText(
				TEXT("BossIntroSkipText_Runtime"),
				15,
				FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));
		}
		if (!BossIntroSkipProgressBar)
		{
			USizeBox* SkipBarSizeBox = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				TEXT("BossIntroSkipSizeBox_Runtime"));
			SkipBarSizeBox->SetWidthOverride(280.0f);
			SkipBarSizeBox->SetHeightOverride(10.0f);
			BossIntroSkipProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
				UProgressBar::StaticClass(),
				TEXT("BossIntroSkipProgressBar_Runtime"));
			BossIntroSkipProgressBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));
			SkipBarSizeBox->AddChild(BossIntroSkipProgressBar);
			if (UVerticalBoxSlot* SkipBarSlot = RuntimeIntroPanel->AddChildToVerticalBox(SkipBarSizeBox))
			{
				SkipBarSlot->SetHorizontalAlignment(HAlign_Center);
				SkipBarSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
			}
		}
	}

	SetThirdPersonReticleVisible(false);
	SetGamePhase(EArenaGamePhase::Waiting);
	SetWaveState(0, 0);
	SetUpgradeRandomSeed(0);
	SetBossPanelVisible(false);
	SetBossIntroPresentation(false, FText::GetEmpty(), 0.0f, 0.0f);
	ClearDamageFeedbackPresentation();
}

// 切换准星显示；HitTestInvisible 保证它不会拦截任何战斗输入。
void UArenaPlayerHUDWidget::SetThirdPersonReticleVisible(bool bVisible)
{
	if (AimReticleText)
	{
		AimReticleText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// 将 GameState 阶段转换为简洁 HUD 文本，Defeat 单独控制可见性。
void UArenaPlayerHUDWidget::SetGamePhase(EArenaGamePhase NewPhase)
{
	FText PhaseDisplayText;
	switch (NewPhase)
	{
	case EArenaGamePhase::Combat:
		PhaseDisplayText = NSLOCTEXT("ArenaPlayerHUDWidget", "PhaseCombat", "Combat");
		break;
	case EArenaGamePhase::Upgrade:
		PhaseDisplayText = NSLOCTEXT("ArenaPlayerHUDWidget", "PhaseUpgrade", "Upgrade");
		break;
	case EArenaGamePhase::Victory:
		PhaseDisplayText = NSLOCTEXT("ArenaPlayerHUDWidget", "PhaseVictory", "Victory");
		break;
	case EArenaGamePhase::Defeat:
		PhaseDisplayText = NSLOCTEXT("ArenaPlayerHUDWidget", "PhaseDefeat", "Defeat");
		break;
	case EArenaGamePhase::BossIntro:
		PhaseDisplayText = NSLOCTEXT("ArenaPlayerHUDWidget", "PhaseBossIntro", "Boss Intro");
		break;
	default:
		PhaseDisplayText = NSLOCTEXT("ArenaPlayerHUDWidget", "PhaseWaiting", "Waiting");
		break;
	}

	if (PhaseText)
	{
		PhaseText->SetText(PhaseDisplayText);
	}
	if (DefeatText)
	{
		DefeatText->SetText(NSLOCTEXT("ArenaPlayerHUDWidget", "DefeatMessage", "DEFEAT"));
		DefeatText->SetVisibility(NewPhase == EArenaGamePhase::Defeat
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

// 仅更新本地 Intro 控件；Boss 名称、剩余时间和 Hold 进度均由 Controller 的复制快照提供。
void UArenaPlayerHUDWidget::SetBossIntroPresentation(
	bool bVisible,
	const FText& InBossName,
	float RemainingTime,
	float SkipProgress)
{
	const ESlateVisibility IntroVisibility = bVisible
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (BossIntroPanel)
	{
		BossIntroPanel->SetVisibility(IntroVisibility);
	}
	if (BossIntroText)
	{
		BossIntroText->SetText(bVisible ? InBossName : FText::GetEmpty());
		BossIntroText->SetVisibility(IntroVisibility);
	}
	if (BossIntroCountdownText)
	{
		BossIntroCountdownText->SetText(bVisible
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BossIntroCountdownFormat", "BOSS APPROACHING  {0}s"),
				FText::AsNumber(FMath::CeilToInt(FMath::Max(RemainingTime, 0.0f))))
			: FText::GetEmpty());
		BossIntroCountdownText->SetVisibility(IntroVisibility);
	}
	if (BossIntroSkipText)
	{
		BossIntroSkipText->SetText(bVisible
			? NSLOCTEXT("ArenaPlayerHUDWidget", "BossIntroSkipPrompt", "Hold Space to Skip")
			: FText::GetEmpty());
		BossIntroSkipText->SetVisibility(IntroVisibility);
	}
	if (BossIntroSkipProgressBar)
	{
		BossIntroSkipProgressBar->SetPercent(FMath::Clamp(SkipProgress, 0.0f, 1.0f));
		BossIntroSkipProgressBar->SetVisibility(IntroVisibility);
	}
}

// 使用 GameState 已复制的整数值刷新波次 HUD。
void UArenaPlayerHUDWidget::SetWaveState(int32 CurrentWaveIndex, int32 RemainingEnemyCount)
{
	if (WaveText)
	{
		WaveText->SetText(FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "WaveFormat", "Wave {0}"),
			FText::AsNumber(FMath::Max(CurrentWaveIndex, 0))));
	}
	if (RemainingEnemiesText)
	{
		RemainingEnemiesText->SetText(FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "EnemiesFormat", "Enemies {0}"),
			FText::AsNumber(FMath::Max(RemainingEnemyCount, 0))));
	}
}

// 将服务器复制的随机种子格式化为右上角调试文本，未同步时显示占位符。
void UArenaPlayerHUDWidget::SetUpgradeRandomSeed(int32 UpgradeRandomSeed)
{
	if (!RandomSeedText)
	{
		return;
	}

	RandomSeedText->SetText(UpgradeRandomSeed > 0
		? FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "UpgradeSeedFormat", "Seed: {0}"),
			FText::FromString(FString::FromInt(UpgradeRandomSeed)))
		: NSLOCTEXT("ArenaPlayerHUDWidget", "UpgradeSeedPending", "Seed --"));
}

// 绑定玩家 HUD 到 ASC/AttributeSet，并注册属性和冷却标签监听。
void UArenaPlayerHUDWidget::BindToAbilitySystem(UArenaAbilitySystemComponent* InAbilitySystemComponent, UArenaAttributeSet* InAttributeSet)
{
	UnbindFromAbilitySystem();

	if (!InAbilitySystemComponent || !InAttributeSet)
	{
		return;
	}

	BoundAbilitySystemComponent = InAbilitySystemComponent;
	BoundAttributeSet = InAttributeSet;

	// 玩家 HUD 只观察 PlayerState 上的 ASC/AttributeSet，保持 UI 与玩法状态分离。
	HealthChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleHealthChanged);
	MaxHealthChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleMaxHealthChanged);
	ShieldChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetShieldAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleShieldChanged);
	EnergyChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetEnergyAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleEnergyChanged);
	MaxEnergyChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxEnergyAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleMaxEnergyChanged);

	// RegisterAndCall 只会在标签 count > 0 时立即回调，所以这里先主动刷新一次可用状态。
	RefreshSkillSlotPlaceholders();
	RefreshBasicAttackCooldownFromAbilitySystem();
	RefreshFireballCooldownFromAbilitySystem();
	RefreshDashCooldownFromAbilitySystem();
	RefreshShieldCooldownFromAbilitySystem();
	RefreshLightningStormCooldownFromAbilitySystem();
	BasicAttackCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_BasicAttack,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBasicAttackCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	FireballCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Fireball,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleFireballCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	DashCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Dash,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleDashCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	ShieldCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Shield,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleShieldCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	LightningStormCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_LightningStorm,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleLightningStormCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);

	RefreshAttributeValues();
}

// 绑定当前 Boss 的属性、死亡与阶段 Tag 委托，切换 Boss 时先对称解除旧数据源。
void UArenaPlayerHUDWidget::BindToBoss(AArenaBossCharacter* InBoss)
{
	UnbindFromBoss();
	if (!InBoss)
	{
		return;
	}

	UArenaAbilitySystemComponent* BossASC = InBoss->GetArenaAbilitySystemComponent();
	UArenaAttributeSet* BossAttributeSet = InBoss->GetArenaAttributeSet();
	if (!BossASC || !BossAttributeSet)
	{
		return;
	}

	BoundBoss = InBoss;
	BoundBossAbilitySystemComponent = BossASC;
	BoundBossAttributeSet = BossAttributeSet;
	BossHealthChangedDelegateHandle = BossASC->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleBossHealthChanged);
	BossMaxHealthChangedDelegateHandle = BossASC->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleBossMaxHealthChanged);
	SetBossHealthValues(InBoss->GetBossDisplayName(), BossAttributeSet->GetHealth(), BossAttributeSet->GetMaxHealth());
	BossDeadTagDelegateHandle = BossASC->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::State_Dead,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBossDeadTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	BossPhaseOneTagDelegateHandle = BossASC->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Boss_Phase_One,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBossPhaseTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	BossPhaseTwoTagDelegateHandle = BossASC->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Boss_Phase_Two,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBossPhaseTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	BossPhaseThreeTagDelegateHandle = BossASC->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Boss_Phase_Three,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBossPhaseTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	RefreshBossPhasePresentation();
}

// Boss 生命显示严格保留零最大值语义，并同步刷新由 ASC 阶段标签驱动的名称区域。
void UArenaPlayerHUDWidget::SetBossHealthValues(const FText& InBossName, float InHealth, float InMaxHealth)
{
	const float DisplayHealth = FMath::Max(InHealth, 0.0f);
	const float DisplayMaxHealth = FMath::Max(InMaxHealth, 0.0f);
	if (BossNameText)
	{
		BossNameText->SetText(InBossName);
	}
	if (BossHealthProgressBar)
	{
		BossHealthProgressBar->SetPercent(CalculatePercent(DisplayHealth, DisplayMaxHealth));
	}
	if (BossHealthText)
	{
		BossHealthText->SetText(FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "BossHealthFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(DisplayHealth)),
			FText::AsNumber(FMath::RoundToInt(DisplayMaxHealth))));
	}
	RefreshBossPhasePresentation();
	SetBossPanelVisible(BoundBoss.IsValid());
}

// 设置生命条和生命文本显示，UI 不直接修改 Health 属性。
void UArenaPlayerHUDWidget::SetHealthValues(float InHealth, float InMaxHealth)
{
	CurrentHealth = FMath::Max(InHealth, 0.0f);
	CurrentMaxHealth = FMath::Max(InMaxHealth, 0.0f);
	HealthPercent = CalculatePercent(CurrentHealth, CurrentMaxHealth);

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthPercent);
	}

	if (HealthText)
	{
		HealthText->SetText(MakeAttributeValueText(NSLOCTEXT("ArenaPlayerHUDWidget", "HealthLabel", "HP"), CurrentHealth, CurrentMaxHealth));
	}
}

// 设置护盾显示值，进度条暂时只表达是否存在护盾。
void UArenaPlayerHUDWidget::SetShieldValues(float InShield)
{
	CurrentShield = FMath::Max(InShield, 0.0f);
	ShieldPercent = CurrentShield > 0.0f ? 1.0f : 0.0f;

	if (ShieldProgressBar)
	{
		// Shield 没有最大值，进度条暂时只表示是否存在护盾。
		ShieldProgressBar->SetPercent(ShieldPercent);
	}

	if (ShieldText)
	{
		ShieldText->SetText(MakeShieldValueText(CurrentShield));
	}
}

// 设置能量条和能量文本显示，UI 只观察 GAS 属性。
void UArenaPlayerHUDWidget::SetEnergyValues(float InEnergy, float InMaxEnergy)
{
	CurrentEnergy = FMath::Max(InEnergy, 0.0f);
	CurrentMaxEnergy = FMath::Max(InMaxEnergy, 0.0f);
	EnergyPercent = CalculatePercent(CurrentEnergy, CurrentMaxEnergy);

	if (EnergyProgressBar)
	{
		EnergyProgressBar->SetPercent(EnergyPercent);
	}

	if (EnergyText)
	{
		EnergyText->SetText(MakeAttributeValueText(NSLOCTEXT("ArenaPlayerHUDWidget", "EnergyLabel", "Energy"), CurrentEnergy, CurrentMaxEnergy));
	}
}

// 显示常驻 HUD 上的方向提示和破盾文本，并把动画扩展交给蓝图事件。
void UArenaPlayerHUDWidget::ShowDamageFeedback(
	float DirectionAngleDegrees,
	bool bHasDirection,
	float Intensity,
	EArenaDamageFeedbackType FeedbackType)
{
	const bool bShowsHealthDirection = FeedbackType == EArenaDamageFeedbackType::HealthOnly
		|| FeedbackType == EArenaDamageFeedbackType::ShieldBreakWithHealthDamage;
	const bool bShowsShieldBreak = FeedbackType == EArenaDamageFeedbackType::ShieldBreak
		|| FeedbackType == EArenaDamageFeedbackType::ShieldBreakWithHealthDamage;

	if (DamageDirectionIndicator)
	{
		DamageDirectionIndicator->SetVisibility(
			bShowsHealthDirection ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DamageDirectionIndicator->SetRenderOpacity(FMath::Clamp(Intensity, 0.2f, 1.0f));
		FWidgetTransform DirectionTransform = DamageDirectionIndicator->GetRenderTransform();
		DirectionTransform.Angle = bHasDirection ? DirectionAngleDegrees : 0.0f;
		DamageDirectionIndicator->SetRenderTransform(DirectionTransform);

		if (bUsesRuntimeDamageDirectionIndicator)
		{
			if (UCanvasPanelSlot* DirectionSlot = Cast<UCanvasPanelSlot>(DamageDirectionIndicator->Slot))
			{
				const float DirectionRadians = FMath::DegreesToRadians(DirectionAngleDegrees);
				const FVector2D EdgeOffset = bHasDirection
					? FVector2D(FMath::Sin(DirectionRadians) * 340.0f, -FMath::Cos(DirectionRadians) * 220.0f)
					: FVector2D::ZeroVector;
				DirectionSlot->SetPosition(EdgeOffset);
			}
		}
	}

	if (ShieldBreakText)
	{
		ShieldBreakText->SetText(NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldBreakMessage", "SHIELD BREAK"));
		ShieldBreakText->SetVisibility(
			bShowsShieldBreak ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	K2_OnDamageFeedback(DirectionAngleDegrees, bHasDirection, Intensity, FeedbackType);

	if (GetWorld())
	{
		const float DisplayDuration = bShowsShieldBreak
			? FMath::Max(DamageDirectionDuration, ShieldBreakMessageDuration)
			: DamageDirectionDuration;
		GetWorld()->GetTimerManager().SetTimer(
			DamageFeedbackTimerHandle,
			this,
			&UArenaPlayerHUDWidget::ClearDamageFeedbackPresentation,
			FMath::Max(DisplayDuration, KINDA_SMALL_NUMBER),
			false);
	}
}

// 快速设置基础攻击冷却激活状态，供蓝图或简单状态刷新调用。
void UArenaPlayerHUDWidget::SetBasicAttackCooldownActive(bool bInCooldownActive)
{
	SetBasicAttackCooldownValues(bInCooldownActive, 0.0f, 0.0f);
}

// 设置基础攻击冷却显示，包括文本和进度条。
void UArenaPlayerHUDWidget::SetBasicAttackCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	bBasicAttackCooldownActive = bInCooldownActive;
	BasicAttackCooldownRemaining = bBasicAttackCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	BasicAttackCooldownDuration = bBasicAttackCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	BasicAttackCooldownPercent = BasicAttackCooldownDuration > 0.0f
		? FMath::Clamp(BasicAttackCooldownRemaining / BasicAttackCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (BasicAttackCooldownText)
	{
		BasicAttackCooldownText->SetText(MakeBasicAttackFullText(bBasicAttackCooldownActive, BasicAttackCooldownRemaining));
	}

	if (BasicAttackSlotText)
	{
		BasicAttackSlotText->SetText(MakeBasicAttackSlotText(bBasicAttackCooldownActive, BasicAttackCooldownRemaining));
	}

	if (BasicAttackCooldownProgressBar)
	{
		BasicAttackCooldownProgressBar->SetPercent(BasicAttackCooldownPercent);
	}
}

// 设置火球冷却显示，并在未授予技能时显示锁定状态。
void UArenaPlayerHUDWidget::SetFireballCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasFireballAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Fireball);
	bFireballCooldownActive = bHasFireballAbility && bInCooldownActive;
	FireballCooldownRemaining = bFireballCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	FireballCooldownDuration = bFireballCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	FireballCooldownPercent = FireballCooldownDuration > 0.0f
		? FMath::Clamp(FireballCooldownRemaining / FireballCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (FireballSlotText)
	{
		FireballSlotText->SetText(MakeFireballSlotText(bHasFireballAbility, bFireballCooldownActive, FireballCooldownRemaining));
	}

	if (FireballCooldownProgressBar)
	{
		FireballCooldownProgressBar->SetPercent(FireballCooldownPercent);
	}
}

// 设置冲刺冷却显示，并在未授予技能时显示锁定状态。
void UArenaPlayerHUDWidget::SetDashCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasDashAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Dash);
	bDashCooldownActive = bHasDashAbility && bInCooldownActive;
	DashCooldownRemaining = bDashCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	DashCooldownDuration = bDashCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	DashCooldownPercent = DashCooldownDuration > 0.0f
		? FMath::Clamp(DashCooldownRemaining / DashCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (DashSlotText)
	{
		DashSlotText->SetText(MakeDashSlotText(bHasDashAbility, bDashCooldownActive, DashCooldownRemaining));
	}

	if (DashCooldownProgressBar)
	{
		DashCooldownProgressBar->SetPercent(DashCooldownPercent);
	}
}

// 设置护盾冷却显示，并在未授予技能时显示锁定状态。
void UArenaPlayerHUDWidget::SetShieldCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasShieldAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Shield);
	bShieldCooldownActive = bHasShieldAbility && bInCooldownActive;
	ShieldCooldownRemaining = bShieldCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	ShieldCooldownDuration = bShieldCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	ShieldCooldownPercent = ShieldCooldownDuration > 0.0f
		? FMath::Clamp(ShieldCooldownRemaining / ShieldCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (ShieldSlotText)
	{
		ShieldSlotText->SetText(MakeShieldSlotText(bHasShieldAbility, bShieldCooldownActive, ShieldCooldownRemaining));
	}

	if (ShieldCooldownProgressBar)
	{
		ShieldCooldownProgressBar->SetPercent(ShieldCooldownPercent);
	}
}

// 设置闪电风暴冷却显示，并在 AbilitySpec 尚未授予时显示锁定状态。
void UArenaPlayerHUDWidget::SetLightningStormCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasLightningStormAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_LightningStorm);
	bLightningStormCooldownActive = bHasLightningStormAbility && bInCooldownActive;
	LightningStormCooldownRemaining = bLightningStormCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	LightningStormCooldownDuration = bLightningStormCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	LightningStormCooldownPercent = LightningStormCooldownDuration > 0.0f
		? FMath::Clamp(LightningStormCooldownRemaining / LightningStormCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (UltimateSlotText)
	{
		UltimateSlotText->SetText(MakeLightningStormSlotText(
			bHasLightningStormAbility,
			bLightningStormCooldownActive,
			LightningStormCooldownRemaining));
	}
}

// Widget 销毁时隐藏 Intro 并解绑 GAS 委托，避免表现或 ASC 回调悬挂对象。
void UArenaPlayerHUDWidget::NativeDestruct()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DamageFeedbackTimerHandle);
	}
	SetBossIntroPresentation(false, FText::GetEmpty(), 0.0f, 0.0f);
	ClearDamageFeedbackPresentation();
	UnbindFromBoss();
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

// 隐藏复用控件；下一次受伤会刷新同一实例，不会持续创建 Widget。
void UArenaPlayerHUDWidget::ClearDamageFeedbackPresentation()
{
	if (DamageDirectionIndicator)
	{
		DamageDirectionIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ShieldBreakText)
	{
		ShieldBreakText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// 对称解除 Boss 属性与标签委托，并清空本地弱引用和面板状态。
void UArenaPlayerHUDWidget::UnbindFromBoss()
{
	if (UArenaAbilitySystemComponent* BossASC = BoundBossAbilitySystemComponent.Get())
	{
		if (BossHealthChangedDelegateHandle.IsValid())
		{
			BossASC->GetGameplayAttributeValueChangeDelegate(
				UArenaAttributeSet::GetHealthAttribute()).Remove(BossHealthChangedDelegateHandle);
		}
		if (BossMaxHealthChangedDelegateHandle.IsValid())
		{
			BossASC->GetGameplayAttributeValueChangeDelegate(
				UArenaAttributeSet::GetMaxHealthAttribute()).Remove(BossMaxHealthChangedDelegateHandle);
		}
		if (BossDeadTagDelegateHandle.IsValid())
		{
			BossASC->UnregisterGameplayTagEvent(
				BossDeadTagDelegateHandle,
				ArenaGameplayTags::State_Dead,
				EGameplayTagEventType::NewOrRemoved);
		}
		if (BossPhaseOneTagDelegateHandle.IsValid())
		{
			BossASC->UnregisterGameplayTagEvent(
				BossPhaseOneTagDelegateHandle,
				ArenaGameplayTags::Boss_Phase_One,
				EGameplayTagEventType::NewOrRemoved);
		}
		if (BossPhaseTwoTagDelegateHandle.IsValid())
		{
			BossASC->UnregisterGameplayTagEvent(
				BossPhaseTwoTagDelegateHandle,
				ArenaGameplayTags::Boss_Phase_Two,
				EGameplayTagEventType::NewOrRemoved);
		}
		if (BossPhaseThreeTagDelegateHandle.IsValid())
		{
			BossASC->UnregisterGameplayTagEvent(
				BossPhaseThreeTagDelegateHandle,
				ArenaGameplayTags::Boss_Phase_Three,
				EGameplayTagEventType::NewOrRemoved);
		}
	}

	BossHealthChangedDelegateHandle.Reset();
	BossMaxHealthChangedDelegateHandle.Reset();
	BossDeadTagDelegateHandle.Reset();
	BossPhaseOneTagDelegateHandle.Reset();
	BossPhaseTwoTagDelegateHandle.Reset();
	BossPhaseThreeTagDelegateHandle.Reset();
	BoundBoss.Reset();
	BoundBossAbilitySystemComponent.Reset();
	BoundBossAttributeSet.Reset();
	SetBossPanelVisible(false);
}

// Blueprint 可以只提供独立控件而不提供父面板，因此两种布局都需要正确显隐。
void UArenaPlayerHUDWidget::SetBossPanelVisible(bool bVisible)
{
	const ESlateVisibility BossVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (BossPanel)
	{
		BossPanel->SetVisibility(BossVisibility);
		return;
	}

	if (BossNameText)
	{
		BossNameText->SetVisibility(BossVisibility);
	}
	if (BossHealthProgressBar)
	{
		BossHealthProgressBar->SetVisibility(BossVisibility);
	}
	if (BossHealthText)
	{
		BossHealthText->SetVisibility(BossVisibility);
	}
	if (BossPhaseText)
	{
		const bool bHasPhase = BoundBoss.IsValid() && BoundBoss->GetCurrentBossPhaseTag().IsValid();
		BossPhaseText->SetVisibility(
			bVisible && bHasPhase ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// 阶段显示完全来自 Boss ASC Tag；缺少独立控件时把文本合并到旧 BossNameText。
void UArenaPlayerHUDWidget::RefreshBossPhasePresentation()
{
	const AArenaBossCharacter* Boss = BoundBoss.Get();
	if (!Boss)
	{
		if (BossPhaseText)
		{
			BossPhaseText->SetText(FText::GetEmpty());
			BossPhaseText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const FGameplayTag PhaseTag = Boss->GetCurrentBossPhaseTag();
	FText PhaseLabel = FText::GetEmpty();
	FLinearColor PhaseColor(1.0f, 0.85f, 0.45f, 1.0f);
	if (PhaseTag == ArenaGameplayTags::Boss_Phase_Three)
	{
		PhaseLabel = NSLOCTEXT("ArenaPlayerHUDWidget", "BossPhaseThree", "Phase 3 · Enraged");
		PhaseColor = FLinearColor(1.0f, 0.08f, 0.03f, 1.0f);
	}
	else if (PhaseTag == ArenaGameplayTags::Boss_Phase_Two)
	{
		PhaseLabel = NSLOCTEXT("ArenaPlayerHUDWidget", "BossPhaseTwo", "Phase 2");
		PhaseColor = FLinearColor(1.0f, 0.45f, 0.08f, 1.0f);
	}
	else if (PhaseTag == ArenaGameplayTags::Boss_Phase_One)
	{
		PhaseLabel = NSLOCTEXT("ArenaPlayerHUDWidget", "BossPhaseOne", "Phase 1");
	}

	const bool bHasPhase = !PhaseLabel.IsEmpty();
	if (BossPhaseText)
	{
		BossPhaseText->SetText(PhaseLabel);
		BossPhaseText->SetColorAndOpacity(FSlateColor(PhaseColor));
		BossPhaseText->SetVisibility(
			bHasPhase ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (BossNameText)
	{
		BossNameText->SetColorAndOpacity(FSlateColor(PhaseColor));
		BossNameText->SetText(!BossPhaseText && bHasPhase
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BossNameWithPhaseFormat", "{0}  |  {1}"),
				Boss->GetBossDisplayName(),
				PhaseLabel)
			: Boss->GetBossDisplayName());
	}
}

// 解绑所有属性/标签委托并停止冷却刷新定时器。
void UArenaPlayerHUDWidget::UnbindFromAbilitySystem()
{
	StopBasicAttackCooldownTimer();
	StopFireballCooldownTimer();
	StopDashCooldownTimer();
	StopShieldCooldownTimer();
	StopLightningStormCooldownTimer();

	if (UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		// Attribute 和 Tag 委托都挂在 ASC 上，Widget 重建或销毁时必须移除。
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
			HealthChangedDelegateHandle.Reset();
		}

		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
			MaxHealthChangedDelegateHandle.Reset();
		}

		if (ShieldChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetShieldAttribute()).Remove(ShieldChangedDelegateHandle);
			ShieldChangedDelegateHandle.Reset();
		}

		if (EnergyChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetEnergyAttribute()).Remove(EnergyChangedDelegateHandle);
			EnergyChangedDelegateHandle.Reset();
		}

		if (MaxEnergyChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxEnergyAttribute()).Remove(MaxEnergyChangedDelegateHandle);
			MaxEnergyChangedDelegateHandle.Reset();
		}

		if (BasicAttackCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				BasicAttackCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_BasicAttack,
				EGameplayTagEventType::NewOrRemoved);
			BasicAttackCooldownTagDelegateHandle.Reset();
		}

		if (FireballCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				FireballCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_Fireball,
				EGameplayTagEventType::NewOrRemoved);
			FireballCooldownTagDelegateHandle.Reset();
		}

		if (DashCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				DashCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_Dash,
				EGameplayTagEventType::NewOrRemoved);
			DashCooldownTagDelegateHandle.Reset();
		}

		if (ShieldCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				ShieldCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_Shield,
				EGameplayTagEventType::NewOrRemoved);
			ShieldCooldownTagDelegateHandle.Reset();
		}

		if (LightningStormCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				LightningStormCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_LightningStorm,
				EGameplayTagEventType::NewOrRemoved);
			LightningStormCooldownTagDelegateHandle.Reset();
		}
	}

	BoundAbilitySystemComponent.Reset();
	BoundAttributeSet.Reset();
}

// 从当前绑定的 AttributeSet 拉取一次属性快照刷新 HUD。
void UArenaPlayerHUDWidget::RefreshAttributeValues()
{
	if (const UArenaAttributeSet* AttributeSet = BoundAttributeSet.Get())
	{
		SetHealthValues(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
		SetShieldValues(AttributeSet->GetShield());
		SetEnergyValues(AttributeSet->GetEnergy(), AttributeSet->GetMaxEnergy());
	}
}

// 刷新技能槽基础文本，显示已授予技能或锁定状态。
void UArenaPlayerHUDWidget::RefreshSkillSlotPlaceholders()
{
	if (FireballSlotText)
	{
		const bool bHasFireballAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Fireball);
		FireballSlotText->SetText(MakeFireballSlotText(bHasFireballAbility, false, 0.0f));
	}

	if (DashSlotText)
	{
		const bool bHasDashAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Dash);
		DashSlotText->SetText(MakeDashSlotText(bHasDashAbility, false, 0.0f));
	}

	if (ShieldSlotText)
	{
		const bool bHasShieldAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Shield);
		ShieldSlotText->SetText(MakeShieldSlotText(bHasShieldAbility, false, 0.0f));
	}

	if (UltimateSlotText)
	{
		const bool bHasLightningStormAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_LightningStorm);
		UltimateSlotText->SetText(MakeLightningStormSlotText(bHasLightningStormAbility, false, 0.0f));
	}
}

// 从 ASC 查询基础攻击冷却剩余时间，并维护对应刷新定时器。
void UArenaPlayerHUDWidget::RefreshBasicAttackCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetBasicAttackCooldownTime(RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_BasicAttack);
		SetBasicAttackCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartBasicAttackCooldownTimer();
		}
		else
		{
			StopBasicAttackCooldownTimer();
		}
	}
	else
	{
		SetBasicAttackCooldownValues(false, 0.0f, 0.0f);
		StopBasicAttackCooldownTimer();
	}
}

// 从 ASC 查询火球冷却剩余时间，并维护对应刷新定时器。
void UArenaPlayerHUDWidget::RefreshFireballCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_Fireball, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Fireball);
		SetFireballCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartFireballCooldownTimer();
		}
		else
		{
			StopFireballCooldownTimer();
		}
	}
	else
	{
		SetFireballCooldownValues(false, 0.0f, 0.0f);
		StopFireballCooldownTimer();
	}
}

// 从 ASC 查询冲刺冷却剩余时间，并维护对应刷新定时器。
void UArenaPlayerHUDWidget::RefreshDashCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_Dash, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Dash);
		SetDashCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartDashCooldownTimer();
		}
		else
		{
			StopDashCooldownTimer();
		}
	}
	else
	{
		SetDashCooldownValues(false, 0.0f, 0.0f);
		StopDashCooldownTimer();
	}
}

// 从 ASC 查询护盾冷却剩余时间，并维护对应刷新定时器。
void UArenaPlayerHUDWidget::RefreshShieldCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_Shield, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Shield);
		SetShieldCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartShieldCooldownTimer();
		}
		else
		{
			StopShieldCooldownTimer();
		}
	}
	else
	{
		SetShieldCooldownValues(false, 0.0f, 0.0f);
		StopShieldCooldownTimer();
	}
}

// 从 ASC 刷新闪电风暴冷却，并按标签状态维护倒计时定时器。
void UArenaPlayerHUDWidget::RefreshLightningStormCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_LightningStorm, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_LightningStorm);
		SetLightningStormCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartLightningStormCooldownTimer();
		}
		else
		{
			StopLightningStormCooldownTimer();
		}
	}
	else
	{
		SetLightningStormCooldownValues(false, 0.0f, 0.0f);
		StopLightningStormCooldownTimer();
	}
}

// 启动基础攻击冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StartBasicAttackCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(BasicAttackCooldownTimerHandle))
	{
		return;
	}

	// 冷却数字是纯表现数据，0.05 秒刷新足够平滑，也避免每帧 Tick。
	GetWorld()->GetTimerManager().SetTimer(
		BasicAttackCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshBasicAttackCooldownFromAbilitySystem,
		0.05f,
		true);
}

// 启动火球冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StartFireballCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(FireballCooldownTimerHandle))
	{
		return;
	}

	// Fireball 使用独立定时器，后续不同技能冷却刷新频率可以单独调。
	GetWorld()->GetTimerManager().SetTimer(
		FireballCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshFireballCooldownFromAbilitySystem,
		0.05f,
		true);
}

// 启动冲刺冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StartDashCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(DashCooldownTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		DashCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshDashCooldownFromAbilitySystem,
		0.05f,
		true);
}

// 启动护盾冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StartShieldCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(ShieldCooldownTimerHandle))
	{
		return;
	}

	// Shield 使用独立定时器，后续若加入护盾表现或音效可单独调整刷新策略。
	GetWorld()->GetTimerManager().SetTimer(
		ShieldCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshShieldCooldownFromAbilitySystem,
		0.05f,
		true);
}

// 启动闪电风暴冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StartLightningStormCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(LightningStormCooldownTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		LightningStormCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshLightningStormCooldownFromAbilitySystem,
		0.05f,
		true);
}

// 停止基础攻击冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopBasicAttackCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BasicAttackCooldownTimerHandle);
	}
}

// 停止火球冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopFireballCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireballCooldownTimerHandle);
	}
}

// 停止冲刺冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopDashCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DashCooldownTimerHandle);
	}
}

// 停止护盾冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopShieldCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ShieldCooldownTimerHandle);
	}
}

// 停止闪电风暴冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopLightningStormCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LightningStormCooldownTimerHandle);
	}
}

// 查询基础攻击冷却剩余时间和总时长。
bool UArenaPlayerHUDWidget::GetBasicAttackCooldownTime(float& OutRemainingTime, float& OutDuration) const
{
	return GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_BasicAttack, OutRemainingTime, OutDuration);
}

// 根据冷却标签查询当前 ASC 上最长的冷却剩余时间。
bool UArenaPlayerHUDWidget::GetCooldownTimeForTag(const FGameplayTag& CooldownTag, float& OutRemainingTime, float& OutDuration) const
{
	OutRemainingTime = 0.0f;
	OutDuration = 0.0f;

	const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !CooldownTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(CooldownTag);
	const FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<TPair<float, float>> DurationAndTimeRemaining = AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(CooldownQuery);
	if (DurationAndTimeRemaining.Num() == 0)
	{
		return false;
	}

	// 与 GameplayAbility 默认冷却查询一致：多个冷却效果存在时显示剩余时间最长的一个。
	for (const TPair<float, float>& CooldownTime : DurationAndTimeRemaining)
	{
		if (CooldownTime.Key > OutRemainingTime)
		{
			OutRemainingTime = CooldownTime.Key;
			OutDuration = CooldownTime.Value;
		}
	}

	return true;
}

// 检查玩家是否已获得带有指定输入标签的 Ability。
bool UArenaPlayerHUDWidget::HasGrantedAbilityForInputTag(const FGameplayTag& InputTag) const
{
	const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !InputTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return true;
		}
	}

	return false;
}

// Health 属性变化回调，刷新生命显示。
void UArenaPlayerHUDWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealthValues(Data.NewValue, CurrentMaxHealth);
}

// MaxHealth 属性变化回调，刷新生命最大值显示。
void UArenaPlayerHUDWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealthValues(CurrentHealth, Data.NewValue);
}

// Shield 属性变化回调，刷新护盾显示。
void UArenaPlayerHUDWidget::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	SetShieldValues(Data.NewValue);
}

// Energy 属性变化回调，刷新能量显示。
void UArenaPlayerHUDWidget::HandleEnergyChanged(const FOnAttributeChangeData& Data)
{
	SetEnergyValues(Data.NewValue, CurrentMaxEnergy);
}

// MaxEnergy 属性变化回调，刷新能量最大值显示。
void UArenaPlayerHUDWidget::HandleMaxEnergyChanged(const FOnAttributeChangeData& Data)
{
	SetEnergyValues(CurrentEnergy, Data.NewValue);
}

// 基础攻击冷却标签变化回调，启动或停止对应冷却显示。
void UArenaPlayerHUDWidget::HandleBasicAttackCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_BasicAttack && NewCount <= 0)
	{
		SetBasicAttackCooldownValues(false, 0.0f, 0.0f);
		StopBasicAttackCooldownTimer();
		return;
	}

	RefreshBasicAttackCooldownFromAbilitySystem();
}

// 火球冷却标签变化回调，启动或停止对应冷却显示。
void UArenaPlayerHUDWidget::HandleFireballCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_Fireball && NewCount <= 0)
	{
		SetFireballCooldownValues(false, 0.0f, 0.0f);
		StopFireballCooldownTimer();
		return;
	}

	RefreshFireballCooldownFromAbilitySystem();
}

// 冲刺冷却标签变化回调，启动或停止对应冷却显示。
void UArenaPlayerHUDWidget::HandleDashCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_Dash && NewCount <= 0)
	{
		SetDashCooldownValues(false, 0.0f, 0.0f);
		StopDashCooldownTimer();
		return;
	}

	RefreshDashCooldownFromAbilitySystem();
}

// 护盾冷却标签变化回调，启动或停止对应冷却显示。
void UArenaPlayerHUDWidget::HandleShieldCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_Shield && NewCount <= 0)
	{
		SetShieldCooldownValues(false, 0.0f, 0.0f);
		StopShieldCooldownTimer();
		return;
	}

	RefreshShieldCooldownFromAbilitySystem();
}

// 闪电风暴冷却标签变化时同步 R 槽状态。
void UArenaPlayerHUDWidget::HandleLightningStormCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_LightningStorm && NewCount <= 0)
	{
		SetLightningStormCooldownValues(false, 0.0f, 0.0f);
		StopLightningStormCooldownTimer();
		return;
	}

	RefreshLightningStormCooldownFromAbilitySystem();
}

// Boss Health 变化时读取同一 AttributeSet 的 MaxHealth，避免 UI 保存第二份玩法状态。
void UArenaPlayerHUDWidget::HandleBossHealthChanged(const FOnAttributeChangeData& Data)
{
	const AArenaBossCharacter* Boss = BoundBoss.Get();
	const UArenaAttributeSet* BossAttributeSet = BoundBossAttributeSet.Get();
	if (Boss && BossAttributeSet)
	{
		SetBossHealthValues(Boss->GetBossDisplayName(), Data.NewValue, BossAttributeSet->GetMaxHealth());
	}
}

// Boss MaxHealth 变化时读取最新 Health，支持后续阶段或多人缩放继续沿用同一 HUD。
void UArenaPlayerHUDWidget::HandleBossMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	const AArenaBossCharacter* Boss = BoundBoss.Get();
	const UArenaAttributeSet* BossAttributeSet = BoundBossAttributeSet.Get();
	if (Boss && BossAttributeSet)
	{
		SetBossHealthValues(Boss->GetBossDisplayName(), BossAttributeSet->GetHealth(), Data.NewValue);
	}
}

// Boss 死亡标签出现时立即隐藏面板，服务器随后清空 ActiveBoss 并触发正式解绑。
void UArenaPlayerHUDWidget::HandleBossDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag != ArenaGameplayTags::State_Dead)
	{
		return;
	}

	if (NewCount > 0)
	{
		SetBossPanelVisible(false);
	}
	else if (const AArenaBossCharacter* Boss = BoundBoss.Get())
	{
		const UArenaAttributeSet* BossAttributeSet = BoundBossAttributeSet.Get();
		SetBossHealthValues(
			Boss->GetBossDisplayName(),
			BossAttributeSet ? BossAttributeSet->GetHealth() : 0.0f,
			BossAttributeSet ? BossAttributeSet->GetMaxHealth() : 0.0f);
	}
}

// 阶段叶标签按“新标签先加、旧标签后删”更新，因此每次回调都重新解析 ASC 最终状态。
void UArenaPlayerHUDWidget::HandleBossPhaseTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Boss_Phase_One
		|| CallbackTag == ArenaGameplayTags::Boss_Phase_Two
		|| CallbackTag == ArenaGameplayTags::Boss_Phase_Three)
	{
		RefreshBossPhasePresentation();
	}
}
