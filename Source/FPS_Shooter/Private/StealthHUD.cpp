#include "StealthHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "SoundHearingNPC.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"

namespace
{
const FLinearColor Panel(0.024f, 0.035f, 0.046f, 0.94f);
const FLinearColor Track(0.17f, 0.21f, 0.25f, 1.0f);
const FLinearColor Muted(0.64f, 0.73f, 0.78f, 1.0f);
const FLinearColor Ink(0.94f, 0.97f, 0.98f, 1.0f);
const FLinearColor Mint(0.42f, 0.93f, 0.75f, 1.0f);
const FLinearColor Amber(1.0f, 0.76f, 0.32f, 1.0f);
const FLinearColor Red(1.0f, 0.36f, 0.32f, 1.0f);
const FName RestartHitBox(TEXT("RestartMission"));

struct FStealthCanvas
{
	explicit FStealthCanvas(AHUD& InHUD, const UCanvas& Canvas)
		: HUD(InHUD), Font(GEngine->GetMediumFont()),
		  Scale(FMath::Min(Canvas.ClipX / 960.0f, Canvas.ClipY / 540.0f)),
		  Size(Canvas.ClipX / Scale, Canvas.ClipY / Scale) {}

	AHUD& HUD;
	UFont* Font;
	float Scale;
	FVector2D Size;

	void Box(const FLinearColor& Color, FVector2D At, FVector2D Extent) const
	{
		HUD.DrawRect(Color, At.X * Scale, At.Y * Scale, Extent.X * Scale, Extent.Y * Scale);
	}

	FVector2D Measure(const FString& Value, float FontScale) const
	{
		float Width = 0.0f, Height = 0.0f;
		HUD.GetTextSize(Value, Width, Height, Font, FontScale);
		return FVector2D(Width, Height);
	}

	void Text(const FString& Value, FVector2D At, const FLinearColor& Color = Ink, float FontScale = 1.0f) const
	{
		HUD.DrawText(Value, Color, At.X * Scale, At.Y * Scale, Font, FontScale * Scale);
	}

	void Right(const FString& Value, FVector2D At, const FLinearColor& Color = Ink, float FontScale = 1.0f) const
	{
		Text(Value, At - FVector2D(Measure(Value, FontScale).X, 0), Color, FontScale);
	}

	void Center(const FString& Value, FVector2D At, const FLinearColor& Color = Ink, float FontScale = 1.0f) const
	{
		Text(Value, At - Measure(Value, FontScale) * 0.5, Color, FontScale);
	}

	void Bar(FVector2D At, FVector2D Extent, float Fraction, const FLinearColor& Color) const
	{
		Box(Track, At, Extent);
		Extent.X *= FMath::Clamp(Fraction, 0.0f, 1.0f);
		if (Extent.X > 0.0) { Box(Color, At, Extent); }
	}
};

const TCHAR* StageCaption(EStealthMissionStage Stage)
{
	switch (Stage)
	{
	case EStealthMissionStage::SafeZone: return TEXT("SAFE START");
	case EStealthMissionStage::Evade: return TEXT("EVADE");
	case EStealthMissionStage::Extraction: return TEXT("EXTRACTION");
	default: return TEXT("INFILTRATION");
	}
}
}

void AStealthHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || Canvas->ClipX <= 0.0f || Canvas->ClipY <= 0.0f)
	{
		return;
	}

	const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (Mission && Mission->GetMissionState() != EStealthMissionState::Playing)
	{
		DrawMissionResult(*Mission);
		return;
	}

	DrawGuardIndicators();
	const FStealthCanvas UI(*this, *Canvas);
	const FVector2D Center = UI.Size * 0.5;
	constexpr float Margin = 22.0f;
	if (Mission)
	{
		const EStealthMissionStage Stage = Mission->GetMissionStage();
		const FLinearColor StageColor = Stage == EStealthMissionStage::Evade ? Red : Mint;
		const FVector2D Objective(Margin, Margin);
		UI.Box(Panel, Objective, FVector2D(360, 66));
		UI.Box(StageColor, Objective, FVector2D(3, 66));
		UI.Text(StageCaption(Stage), Objective + FVector2D(15, 9), StageColor, 0.8f);
		UI.Text(Mission->GetObjectiveText(), Objective + FVector2D(15, 32), Ink, 1.14f);

		const FVector2D Detection(UI.Size.X - Margin - 184, Margin);
		UI.Box(Panel, Detection, FVector2D(184, 59));
		UI.Text(TEXT("DETECTION"), Detection + FVector2D(14, 10), Muted, 0.86f);
		UI.Bar(Detection + FVector2D(14, 37), FVector2D(156, 7), Mission->GetDetectionLevel(),
			Stage == EStealthMissionStage::Evade ? Red : Amber);
	}

	const AStealthCharacter* Character = Cast<AStealthCharacter>(GetOwningPawn());
	if (!Character) { return; }

	const FString Prompt = Character->GetInteractionPrompt().ToString();
	const FLinearColor Dot = Prompt.IsEmpty()
		? FLinearColor(1.0f, 1.0f, 1.0f, FMath::Lerp(1.0f, 0.55f, Character->GetAimAlpha())) : Mint;
	UI.Box(FLinearColor(0, 0, 0, 0.8f), Center - FVector2D(3, 3), FVector2D(6, 6));
	UI.Box(Dot, Center - FVector2D(1.5, 1.5), FVector2D(3, 3));
	if (!Prompt.IsEmpty())
	{
		const float PromptWidth = FMath::Min<float>(UI.Size.X - 2 * Margin, UI.Measure(Prompt, 1.0f).X + 74.0f);
		const FVector2D At(Center.X - PromptWidth * 0.5f, Center.Y + 23);
		UI.Box(Panel, At, FVector2D(PromptWidth, 34));
		UI.Box(Mint, At + FVector2D(5, 5), FVector2D(43, 24));
		UI.Center(TEXT("E / A"), At + FVector2D(26.5, 17), Panel, 0.8f);
		const float TextScale = FMath::Min(1.0f, (PromptWidth - 65.0f) / FMath::Max(1.0, UI.Measure(Prompt, 1.0f).X));
		UI.Text(Prompt, At + FVector2D(57, 17 - UI.Measure(Prompt, TextScale).Y * 0.5), Ink, TextScale);
	}

	const float FootY = UI.Size.Y - Margin - 80;
	const FVector2D Vitals(Margin, FootY);
	UI.Box(Panel, Vitals, FVector2D(199, 80));
	UI.Text(TEXT("HEALTH"), Vitals + FVector2D(14, 10), Muted, 0.86f);
	UI.Right(FString::Printf(TEXT("%.0f"), Character->GetHealth()), Vitals + FVector2D(185, 8), Ink, 1.2f);
	const float Health = Character->GetHealthFraction();
	UI.Bar(Vitals + FVector2D(14, 36), FVector2D(171, 5), Health, Health > 0.3f ? Mint : Red);
	UI.Text(TEXT("STANCE"), Vitals + FVector2D(14, 55), Muted, 0.8f);
	UI.Right(Character->bIsCrouched ? TEXT("CROUCHED") : TEXT("STANDING"),
		Vitals + FVector2D(185, 53), Character->bIsCrouched ? Mint : Ink, 0.9f);

	const FVector2D Inventory(UI.Size.X - Margin - 199, FootY);
	UI.Box(Panel, Inventory, FVector2D(199, 80));
	UI.Text(Character->IsReloading() ? TEXT("RELOADING") : TEXT("AMMO"),
		Inventory + FVector2D(14, 10), Muted, 0.86f);
	UI.Right(FString::Printf(TEXT("%d / %d"), Character->GetAmmoInMagazine(), Character->GetReserveAmmo()),
		Inventory + FVector2D(185, 8), Ink, 1.2f);
	if (Character->IsReloading())
	{
		UI.Bar(Inventory + FVector2D(14, 36), FVector2D(171, 5), Character->GetReloadProgress(), Amber);
	}
	else
	{
		UI.Box(Track, Inventory + FVector2D(14, 38), FVector2D(171, 1));
	}
	UI.Text(TEXT("NOISE MAKERS  [Q]"), Inventory + FVector2D(14, 55), Muted, 0.8f);
	UI.Right(FString::FromInt(Character->GetNoiseMakerCount()), Inventory + FVector2D(185, 51), Mint, 1.08f);
}

void AStealthHUD::DrawGuardIndicators()
{
	const APlayerController* PC = GetOwningPlayerController();
	if (!PC) { return; }

	const FStealthCanvas UI(*this, *Canvas);
	FVector Eye;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(Eye, ViewRotation);
	const FVector Forward = ViewRotation.Vector();
	for (TActorIterator<ASoundHearingNPC> It(GetWorld()); It; ++It)
	{
		const ASoundHearingNPC& Guard = **It;
		const auto* AI = Cast<AStealthAIController>(Guard.GetController());
		if (!AI || Guard.IsDead()) { continue; }

		const TCHAR* Icon;
		FLinearColor Color;
		switch (AI->GetAIState())
		{
		case EStealthAIState::Suspicious: Icon = TEXT("?"); Color = Amber; break;
		case EStealthAIState::Investigate: Icon = TEXT(">"); Color = FLinearColor(0.53f, 0.81f, 1.0f); break;
		case EStealthAIState::Search: Icon = TEXT("..."); Color = FLinearColor(1.0f, 0.60f, 0.25f); break;
		case EStealthAIState::Chase: Icon = TEXT("!"); Color = Red; break;
		default: continue;
		}

		const float HalfHeight = Guard.GetSimpleCollisionHalfHeight();
		const FVector Head = Guard.GetActorLocation() + FVector(0, 0, HalfHeight * 0.75f);
		const FVector ToGuard = Head - Eye;
		if (ToGuard.SizeSquared() > FMath::Square(2500.0) || FVector::DotProduct(Forward, ToGuard) <= 0.0)
		{
			continue;
		}
		const FVector Screen = Project(Head + FVector(0, 0, 41), false);
		const FVector2D Anchor(Screen.X / UI.Scale, Screen.Y / UI.Scale);
		const FString Label = AI->GetStateDisplayName().ToString();
		const float Width = UI.Measure(Label, 0.74f).X + 36.0f;
		const FVector2D At(Anchor.X - Width * 0.5f, Anchor.Y - 22);
		if (At.X < 4 || At.Y < 4 || At.X + Width > UI.Size.X - 4 || Anchor.Y > UI.Size.Y - 4)
		{
			continue;
		}

		FCollisionQueryParams Sight(SCENE_QUERY_STAT(StealthGuardIndicator), false, GetOwningPawn());
		Sight.AddIgnoredActor(&Guard);
		if (GetWorld()->LineTraceTestByChannel(Eye, Head, ECC_Visibility, Sight)) { continue; }

		UI.Box(Panel, At, FVector2D(Width, 22));
		UI.Box(Color, At + FVector2D(3, 3), FVector2D(20, 16));
		UI.Center(Icon, At + FVector2D(13, 11), Panel, 0.82f);
		UI.Text(Label, At + FVector2D(29, 11 - UI.Measure(Label, 0.74f).Y * 0.5), Ink, 0.74f);
	}
}

void AStealthHUD::DrawMissionResult(const AStealthGameMode& Mission)
{
	const FStealthCanvas UI(*this, *Canvas);
	const FVector2D Center = UI.Size * 0.5;
	const FVector2D At = Center - FVector2D(206, 125);
	const bool bWon = Mission.GetMissionState() == EStealthMissionState::Won;
	const FLinearColor Accent = bWon ? Mint : Red;
	UI.Box(FLinearColor(0.01f, 0.02f, 0.03f, 0.82f), FVector2D::ZeroVector, UI.Size);
	UI.Box(Panel, At, FVector2D(412, 250));
	UI.Box(Accent, At, FVector2D(412, 3));

	UI.Center(bWon ? TEXT("MISSION COMPLETE") : TEXT("MISSION FAILED"),
		At + FVector2D(206, 42), Accent, 1.7f);
	UI.Center(bWon ? TEXT("Intel secured. Extraction confirmed.") : TEXT("You were taken down."),
		At + FVector2D(206, 78), Ink);
	UI.Box(Track, At + FVector2D(29, 105), FVector2D(354, 1));
	const int32 Seconds = FMath::FloorToInt(Mission.GetMissionTime());
	UI.Center(FString::Printf(TEXT("TIME  %02d:%02d      INTEL  %s"), Seconds / 60, Seconds % 60,
		Mission.HasObjective() ? TEXT("SECURED") : TEXT("MISSING")), At + FVector2D(206, 128), Muted, 0.88f);

	const FVector2D ButtonAt = At + FVector2D(29, 157);
	const FVector2D ButtonSize(354, 43);
	AddHitBox(ButtonAt * UI.Scale, ButtonSize * UI.Scale, RestartHitBox, true);
	float MouseX, MouseY;
	const APlayerController* PC = GetOwningPlayerController();
	const bool bHovered = PC && PC->GetMousePosition(MouseX, MouseY)
		&& GetHitBoxAtCoordinates(FVector2D(MouseX, MouseY), true) == GetHitBoxWithName(RestartHitBox);
	const FLinearColor ButtonColor = bHovered ? FMath::Lerp(Accent, FLinearColor::White, 0.25f) : Accent;
	UI.Box(ButtonColor, ButtonAt, ButtonSize);
	UI.Center(TEXT("Restart mission"), ButtonAt + ButtonSize * 0.5, Panel, 1.16f);
	UI.Center(TEXT("ENTER / START"), At + FVector2D(206, 224), Muted, 0.78f);
}

void AStealthHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	if (BoxName == RestartHitBox)
	{
		if (AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetOwningPlayerController()))
		{
			PC->RestartMission();
		}
	}
}
