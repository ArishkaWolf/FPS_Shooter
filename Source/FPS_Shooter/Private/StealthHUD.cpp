#include "StealthHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthPlayerController.h"

void AStealthHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (GameMode && GameMode->GetMissionState() != EStealthMissionState::Playing)
	{
		DrawMissionResult();
		return;
	}

	const float Scale = FMath::Min(Canvas->ClipX / 1280.0f, Canvas->ClipY / 720.0f);
	const float Margin = 28.0f * Scale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const AStealthCharacter* OwningCharacter = Cast<AStealthCharacter>(GetOwningPawn());
	if (!OwningCharacter || OwningCharacter->GetAimAlpha() < 0.9f)
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), CenterX - 3.0f, CenterY - 3.0f, 6.0f, 6.0f);
		DrawRect(FLinearColor::White, CenterX - 1.5f, CenterY - 1.5f, 3.0f, 3.0f);
	}

	if (GameMode)
	{
		const bool bSafe = GameMode->GetMissionStage() == EStealthMissionStage::SafeZone;
		const FLinearColor StatusColor = bSafe ? FLinearColor::Green
			: GameMode->IsPlayerPursued() ? FLinearColor(1.0f, 0.35f, 0.15f) : FLinearColor(1.0f, 0.85f, 0.3f);
		DrawRect(FLinearColor(0.02f, 0.03f, 0.04f, 0.85f), Margin, Margin, Canvas->ClipX - 2 * Margin, 116.0f * Scale);
		DrawText(GameMode->GetObjectiveText(), StatusColor, Margin + 14 * Scale, Margin + 12 * Scale, nullptr, 1.15f * Scale);
		DrawText(bSafe ? TEXT("Staging protection ends when you leave. Weapons and distractions unlock outside.")
			: GameMode->GetThreatText(), FLinearColor::White, Margin + 14 * Scale, Margin + 44 * Scale, nullptr, Scale);
		DrawText(FString::Printf(TEXT("DETECTION  %d%%    |    INTEL: %s"), FMath::RoundToInt(GameMode->GetDetectionLevel() * 100.0f),
			GameMode->HasObjective() ? TEXT("SECURED") : TEXT("NOT COLLECTED")), StatusColor,
			Margin + 14 * Scale, Margin + 76 * Scale, nullptr, Scale);
	}

	if (const AStealthCharacter* Character = Cast<AStealthCharacter>(GetOwningPawn()))
	{
		DrawRect(FLinearColor(0.02f, 0.03f, 0.04f, 0.85f), Margin, Canvas->ClipY - 100 * Scale,
			Canvas->ClipX - 2 * Margin, 78 * Scale);
		DrawText(FString::Printf(TEXT("HEALTH  %.0f    |    DISTRACTIONS [Q]  %d"), Character->GetHealth(), Character->GetNoiseMakerCount()),
			FLinearColor::White, Margin + 14 * Scale, Canvas->ClipY - 86 * Scale, nullptr, 1.1f * Scale);
		const FString AmmoText = Character->IsReloading()
			? FString::Printf(TEXT("RELOADING  %d%%"), FMath::RoundToInt(Character->GetReloadProgress() * 100.0f))
			: FString::Printf(TEXT("AMMO  %d / %d"), Character->GetAmmoInMagazine(), Character->GetReserveAmmo());
		DrawText(AmmoText,
			FLinearColor::White, Canvas->ClipX - 220 * Scale, Canvas->ClipY - 86 * Scale, nullptr, 1.1f * Scale);
		DrawText(TEXT("WASD move  |  Shift sprint  |  Ctrl/C crouch  |  E interact  |  LMB fire  |  RMB aim  |  R reload"),
			FLinearColor(0.65f, 0.72f, 0.78f), Margin + 14 * Scale, Canvas->ClipY - 52 * Scale, nullptr, 0.9f * Scale);
	}
}

void AStealthHUD::DrawMissionResult()
{
	const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	const bool bWon = GameMode->GetMissionState() == EStealthMissionState::Won;
	const float Scale = FMath::Min(Canvas->ClipX / 1280.0f, Canvas->ClipY / 720.0f);
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const FLinearColor Accent = bWon ? FLinearColor(0.25f, 0.9f, 0.55f) : FLinearColor(1.0f, 0.3f, 0.25f);
	DrawRect(FLinearColor(0.01f, 0.015f, 0.025f, 0.94f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	const auto DrawCentered = [this, CenterX, Scale](const FString& Text, float Y, const FLinearColor& Color, float FontScale)
	{
		float Width, Height;
		UFont* Font = GEngine->GetMediumFont();
		GetTextSize(Text, Width, Height, Font, FontScale * Scale);
		DrawText(Text, Color, CenterX - Width * 0.5f, Y, Font, FontScale * Scale);
	};
	DrawCentered(bWon ? TEXT("MISSION COMPLETE") : TEXT("MISSION FAILED"), CenterY - 140 * Scale, Accent, 2.0f);
	DrawCentered(bWon ? TEXT("Intel secured. You reached extraction safely.") : TEXT("Your health reached zero. Use cover and quieter routes."),
		CenterY - 78 * Scale, FLinearColor::White, 1.0f);
	const int32 Seconds = FMath::FloorToInt(GameMode->GetMissionTime());
	DrawCentered(FString::Printf(TEXT("TIME %02d:%02d   |   INTEL %s"), Seconds / 60, Seconds % 60,
		GameMode->HasObjective() ? TEXT("SECURED") : TEXT("NOT COLLECTED")),
		CenterY - 32 * Scale, FLinearColor(0.65f, 0.72f, 0.78f), 1.0f);
	const FVector2D ButtonPosition(CenterX - 190 * Scale, CenterY + 36 * Scale);
	const FVector2D ButtonSize(380 * Scale, 60 * Scale);
	DrawRect(Accent, ButtonPosition.X, ButtonPosition.Y, ButtonSize.X, ButtonSize.Y);
	DrawCentered(TEXT("RESTART LEVEL  [ENTER]"), CenterY + 55 * Scale, FLinearColor::Black, 1.1f);
	DrawCentered(TEXT("Click the button, press Enter, or press controller Start."), CenterY + 126 * Scale,
		FLinearColor(0.65f, 0.72f, 0.78f), 0.9f);
	AddHitBox(ButtonPosition, ButtonSize, TEXT("RestartMission"), true);
}

void AStealthHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	if (BoxName == TEXT("RestartMission"))
	{
		if (AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetOwningPlayerController()))
		{
			PC->RestartMission();
		}
	}
}
