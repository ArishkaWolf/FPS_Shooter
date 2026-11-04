#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUDHitBox.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NoiseMakerPickup.h"
#include "Slate/SceneViewport.h"
#include "SoundHearingNPC.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthExitZone.h"
#include "StealthGameMode.h"
#include "StealthHUD.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

namespace
{
const FIntPoint Resolutions[] = {{1024, 768}, {1920, 1080}, {2560, 1080}};
const FName RestartBox(TEXT("RestartMission"));

class FStealthHUDViewportCheck final : public IAutomationLatentCommand
{
public:
	explicit FStealthHUDViewportCheck(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 150)
		{
			Test.AddError(FString::Printf(TEXT("HUD check timed out in stage %d, resolution %d"), Stage, ResolutionIndex));
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || !World->HasBegunPlay()) { return false; }
		if (Stage == 3 && World == PreviousWorld.Get()) { return false; }
		if (World->GetTimeSeconds() < 1.5f) { return false; }

		UGameViewportClient* Client = World->GetGameViewport();
		FSceneViewport* Viewport = Client ? Client->GetGameViewport() : nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		AStealthCharacter* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		AStealthHUD* HUD = PC ? Cast<AStealthHUD>(PC->GetHUD()) : nullptr;
		AStealthGameMode* Mission = World->GetAuthGameMode<AStealthGameMode>();
		if (!Viewport || !HUD || !Player || !Mission) { Test.AddError(TEXT("Missing PIE viewport, player, mission or HUD")); return true; }

		if (Stage == 0)
		{
			if (!Prepare(*World, *PC, *Player, *Mission)) { return true; }
			Stage = 1;
			ReadyAt = Now + 0.5;
			return false;
		}
		if (Now < ReadyAt || FScreenshotRequest::IsScreenshotRequested()) { return false; }

		if (Stage == 3)
		{
			Test.TestEqual(TEXT("Click restarts a live mission"), Mission->GetMissionState(), EStealthMissionState::Playing);
			Test.TestEqual(TEXT("Restart restores health"), Player->GetHealth(), 100.0f);
			for (TActorIterator<AStealthExitZone> It(World); It; ++It)
			{
				Player->SetActorLocation(It->GetActorLocation());
				Mission->Tick(0.01f);
				Mission->CollectObjective(Player);
				It->Tick(0.01f);
				break;
			}
			Test.TestEqual(TEXT("Victory shows the mission result"), Mission->GetMissionState(), EStealthMissionState::Won);
			Stage = 4;
			ResolutionIndex = 1;
			ReadyAt = Now + 0.25;
			return false;
		}

		const FIntPoint Size = Resolutions[ResolutionIndex];
		if (Viewport->GetSizeXY() != Size)
		{
			Viewport->SetFixedViewportSize(Size.X, Size.Y);
			ReadyAt = Now + 0.25;
			return false;
		}
		if (Stage == 1)
		{
			Test.TestEqual(TEXT("Only the focused pickup supplies the prompt"), Player->GetInteractionPrompt().ToString(), FString(TEXT("Take noise makers (+1)")));
			Capture(TEXT("Playing"), Size);
			if (++ResolutionIndex == UE_ARRAY_COUNT(Resolutions))
			{
				ResolutionIndex = 0;
				Stage = 2;
			}
		}
		else if (Stage == 2 && Mission->GetMissionState() == EStealthMissionState::Playing)
		{
			UGameplayStatics::ApplyDamage(Player, 1000, nullptr, nullptr, UDamageType::StaticClass());
			Test.TestEqual(TEXT("Defeat shows the mission result"), Mission->GetMissionState(), EStealthMissionState::Lost);
		}
		else if (Stage == 2 && !Mission->IsMissionResultReady())
		{
			return false;
		}
		else
		{
			const double Scale = FMath::Min(Size.X / 960.0, Size.Y / 540.0);
			const FVector2D ButtonCenter(Size.X * 0.5, Size.Y * 0.5 + 53.5 * Scale);
			const FHUDHitBox* Button = HUD->GetHitBoxWithName(RestartBox);
			Test.TestTrue(TEXT("Restart hit box is under its visible center"), Button && Button->Contains(ButtonCenter));
			if (Stage == 4)
			{
				Capture(TEXT("Victory"), Size);
				Stage = 5;
			}
			else if (Stage == 5)
			{
				Viewport->SetFixedViewportSize(0, 0);
				return true;
			}
			else if (!bCapturedResult)
			{
				Capture(TEXT("Defeat"), Size);
				bCapturedResult = true;
			}
			else if (ResolutionIndex < UE_ARRAY_COUNT(Resolutions) - 1)
			{
				++ResolutionIndex;
				bCapturedResult = false;
			}
			else
			{
				PreviousWorld = World;
				Test.TestTrue(TEXT("A click in the button dispatches restart"),
					HUD->UpdateAndDispatchHitBoxClickEvents(ButtonCenter, IE_Pressed));
				Stage = 3;
			}
		}
		ReadyAt = Now + 0.25;
		return false;
	}

private:
	bool Prepare(UWorld& World, APlayerController& PC, AStealthCharacter& Player, AStealthGameMode& Mission)
	{
		FVector Clearing = FVector::ZeroVector;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("Greybox.QA.Movement"))) { Clearing = It->GetActorLocation(); break; }
		}
		if (Clearing.IsNearlyZero()) { Test.AddError(TEXT("Missing HUD test clearing")); return false; }
		for (TActorIterator<AStealthAIController> It(&World); It; ++It) { It->SetActorTickEnabled(false); }
		Player.SetActorLocation(Clearing);
		PC.SetControlRotation(FRotator::ZeroRotator);
		Mission.Tick(0.01f);
		Test.TestTrue(TEXT("Empty view has no interaction prompt"), Player.GetInteractionPrompt().IsEmpty());
		UGameplayStatics::ApplyDamage(&Player, 28, nullptr, nullptr, UDamageType::StaticClass());
		Player.Crouch();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform PickupTransform(FRotator::ZeroRotator, Clearing + FVector(270, 0, 22), FVector(0.65f));
		World.SpawnActor<ANoiseMakerPickup>(ANoiseMakerPickup::StaticClass(), PickupTransform, Params);
		const EStealthAIState States[] = {EStealthAIState::Suspicious, EStealthAIState::Investigate, EStealthAIState::Search, EStealthAIState::Chase};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(States); ++Index)
		{
			const FVector Position = Clearing + FVector(550, (Index - 1.5) * 155, 0);
			const bool bChase = States[Index] == EStealthAIState::Chase;
			const FRotator Facing(0, bChase ? (Clearing - Position).Rotation().Yaw : 0, 0);
			ASoundHearingNPC* Guard = World.SpawnActor<ASoundHearingNPC>(Position, Facing, Params);
			AStealthAIController* AI = Guard ? Cast<AStealthAIController>(Guard->GetController()) : nullptr;
			if (!Test.TestNotNull(TEXT("Guard fixture has its gameplay controller"), AI)) { return false; }
			Guard->GetCharacterMovement()->DisableMovement();
			if (!bChase)
			{
				Guard->HearingComponent->HearSoundAtLocation(Position + FVector(Index == 2 ? 40 : 500, 0, 0), Index == 0 ? 0.06f : 0.8f);
			}
			AI->Tick(bChase ? 4.0f : 0.02f);
			Test.TestEqual(TEXT("Fixture enters its gameplay alert state"), AI->GetAIState(), States[Index]);
			AI->SetActorTickEnabled(false);
		}
		return true;
	}

	void Capture(const TCHAR* Screen, FIntPoint Size)
	{
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("InterfaceWork/Screenshots");
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Name = FString::Printf(TEXT("%s-%dx%d.png"), Screen, Size.X, Size.Y);
		FScreenshotRequest::RequestScreenshot(Directory / Name, false, false);
		Test.AddInfo(FString::Printf(TEXT("Rendered HUD %s at %dx%d"), Screen, Size.X, Size.Y));
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<UWorld> PreviousWorld;
	int32 Stage = 0, ResolutionIndex = 0;
	double Started = FPlatformTime::Seconds(), ReadyAt = 0;
	bool bCapturedResult = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthHUDViewportTest, "FPS_Shooter.UI.ResponsiveHUDPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStealthHUDViewportTest::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		AddInfo(TEXT("HUD screenshot test requires a rendered PIE viewport"));
		return true;
	}
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FStealthHUDViewportCheck(*this));
	return true;
}

#endif
