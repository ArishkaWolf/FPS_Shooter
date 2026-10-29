#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerInput.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NPCHearingComponent.h"
#include "SoundHearingNPC.h"
#include "SoundRayEmitterComponent.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthExitZone.h"
#include "StealthGameMode.h"
#include "StealthHUD.h"
#include "StealthObjective.h"
#include "StealthPlayerController.h"
#include "Tests/AutomationCommon.h"

namespace
{
	struct FStealthTestWorld
	{
		UWorld* World;
		AStealthGameMode* Mission;
		AStealthPlayerController* Controller;
		AStealthCharacter* Player;

		FStealthTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/FPS_Shooter.StealthGameMode"), TRAVEL_Absolute);
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			Controller = World->SpawnActor<AStealthPlayerController>();
			Player = Spawn<AStealthCharacter>(FVector(0, 0, 200));
			Controller->Possess(Player);
			Mission = World->GetAuthGameMode<AStealthGameMode>();
			World->BeginPlay();
			Player->GetCharacterMovement()->DisableMovement();
			Mission->Tick(0.01f);
		}

		~FStealthTestWorld()
		{
			World->BeginTearingDown();
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		template<typename T> T* Spawn(const FVector& Location)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}

		void LeaveStaging()
		{
			Player->SetActorLocation(FVector(800, 0, 200));
			Mission->Tick(0.1f);
		}
	};

	constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthStagingTest, "FPS_Shooter.Mission.SafeStartAndHealth", TestFlags)
bool FStealthStagingTest::RunTest(const FString& Parameters)
{
	FStealthTestWorld Scene;
	TestTrue(TEXT("Starts protected in staging"), Scene.Mission->IsActorProtected(Scene.Player));
	TestEqual(TEXT("Initial stage"), Scene.Mission->GetMissionStage(), EStealthMissionStage::SafeZone);
	ASoundHearingNPC* Guard = Scene.Spawn<ASoundHearingNPC>(FVector(-100, 0, 200));
	Guard->GetCharacterMovement()->DisableMovement();
	AStealthAIController* AI = Cast<AStealthAIController>(Guard->GetController());
	if (TestNotNull(TEXT("Staging guard AI exists"), AI))
	{
		Guard->HearingComponent->HearSoundAtLocation(Scene.Player->GetActorLocation(), 0.8f);
		AI->Tick(3.0f);
		TestEqual(TEXT("Staging prevents detection and sound alerts"), AI->GetAIState(), EStealthAIState::Patrol);
		TestEqual(TEXT("Staging prevents sight accumulation"), AI->GetDetectionLevel(), 0.0f);
	}
	UGameplayStatics::ApplyDamage(Scene.Player, 30, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("Safe start prevents damage"), Scene.Player->GetHealth(), 100.0f);
	Scene.Player->GetCharacterMovement()->Velocity = FVector(0, 0, -600);
	Scene.Player->Landed(FHitResult());
	Scene.Player->GetCharacterMovement()->StopMovementImmediately();
	TestTrue(TEXT("Staging protection does not mute landing feedback"), Scene.Player->GetLastLandingStrength() > 0.0f);
	TestFalse(TEXT("No extraction without intel"), Scene.Mission->TryCompleteMission(Scene.Player));
	Scene.Mission->DefeatPlayer(Scene.Player);
	TestEqual(TEXT("A living player cannot be instantly captured"), Scene.Mission->GetMissionState(), EStealthMissionState::Playing);
	Scene.LeaveStaging();
	TestEqual(TEXT("Departure starts infiltration"), Scene.Mission->GetMissionStage(), EStealthMissionStage::Infiltration);
	UGameplayStatics::ApplyDamage(Scene.Player, 14, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("Damage consumes health"), Scene.Player->GetHealth(), 86.0f);
	Scene.Player->SetActorLocation(Scene.Mission->GetSafeZoneCenter());
	TestFalse(TEXT("Returning does not restore immunity"), Scene.Mission->IsActorProtected(Scene.Player));
	UGameplayStatics::ApplyDamage(Scene.Player, -50, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("Negative damage cannot heal"), Scene.Player->GetHealth(), 86.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthPursuitTest, "FPS_Shooter.Mission.PursuitAndEscape", TestFlags)
bool FStealthPursuitTest::RunTest(const FString& Parameters)
{
	FStealthTestWorld Scene;
	Scene.LeaveStaging();
	ASoundHearingNPC* Guard = Scene.Spawn<ASoundHearingNPC>(FVector(700, 0, 200));
	Guard->GetCharacterMovement()->DisableMovement();
	AStealthAIController* AI = Cast<AStealthAIController>(Guard->GetController());
	if (!TestNotNull(TEXT("Guard possesses C++ AI"), AI))
	{
		return false;
	}
	AI->Tick(3.0f);
	TestEqual(TEXT("Full detection starts chase"), AI->GetAIState(), EStealthAIState::Chase);
	TestTrue(TEXT("Close range does not instantly fail the mission"), Scene.Mission->GetMissionState() == EStealthMissionState::Playing);
	TestEqual(TEXT("Reaction delay before first shot"), Scene.Player->GetHealth(), 100.0f);
	Scene.Player->GetMesh()->TickAnimation(0.0f, false);
	Scene.Player->GetMesh()->RefreshBoneTransforms();
	AI->Tick(0.81f);
	TestTrue(TEXT("Guard gunfire damages health instead of instantly capturing"),
		Scene.Player->GetHealth() > 0.0f && Scene.Player->GetHealth() < 100.0f);
	TestTrue(TEXT("Chase blocks extraction"), Scene.Mission->IsPlayerPursued());
	TestTrue(TEXT("Collecting intel during pursuit is allowed"), Scene.Mission->CollectObjective(Scene.Player));
	TestFalse(TEXT("Cannot extract while pursued"), Scene.Mission->TryCompleteMission(Scene.Player));
	Scene.Player->SetActorLocation(FVector(5000, 0, 200));
	AI->Tick(2.5f);
	TestEqual(TEXT("Lost sight transitions to search"), AI->GetAIState(), EStealthAIState::Search);
	TestTrue(TEXT("Search still blocks extraction"), Scene.Mission->IsPlayerPursued());
	AI->Tick(7.1f);
	TestEqual(TEXT("Search expires and patrol resumes"), AI->GetAIState(), EStealthAIState::Patrol);
	TestFalse(TEXT("Player can evade pursuit"), Scene.Mission->IsPlayerPursued());
	TestTrue(TEXT("Extraction succeeds after escape"), Scene.Mission->TryCompleteMission(Scene.Player));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthObjectiveTest, "FPS_Shooter.Mission.ObjectiveExitAndVictory", TestFlags)
bool FStealthObjectiveTest::RunTest(const FString& Parameters)
{
	FStealthTestWorld Scene;
	Scene.LeaveStaging();
	AActor* Other = Scene.Spawn<AActor>(FVector::ZeroVector);
	TestFalse(TEXT("Non-player cannot collect intel"), Scene.Mission->CollectObjective(Other));
	AStealthExitZone* Exit = Scene.Spawn<AStealthExitZone>(Scene.Player->GetActorLocation());
	Scene.Player->UpdateOverlaps();
	Exit->Tick(0.1f);
	TestEqual(TEXT("Exit is locked before intel"), Scene.Mission->GetMissionState(), EStealthMissionState::Playing);
	AStealthObjective* Objective = Scene.Spawn<AStealthObjective>(Scene.Player->GetActorLocation() + FVector(150, 0, 0));
	Objective->Interact_Implementation(Scene.Player);
	TestTrue(TEXT("Interaction collects intel"), Scene.Mission->HasObjective());
	TestTrue(TEXT("Collected objective is removed"), Objective->IsActorBeingDestroyed());
	TestFalse(TEXT("Intel cannot be collected twice"), Scene.Mission->CollectObjective(Scene.Player));
	TestFalse(TEXT("Non-player cannot extract"), Scene.Mission->TryCompleteMission(Other));
	Exit->Tick(0.1f);
	TestEqual(TEXT("Standing in exit wins when requirements change"), Scene.Mission->GetMissionState(), EStealthMissionState::Won);
	UGameplayStatics::ApplyDamage(Scene.Player, 1000, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("Victory prevents further damage"), Scene.Player->GetHealth(), 100.0f);
	Scene.Mission->DefeatPlayer(Scene.Player);
	TestEqual(TEXT("Victory is final"), Scene.Mission->GetMissionState(), EStealthMissionState::Won);
	Scene.Controller->RestartMission();
	TestFalse(TEXT("Restart queues level reload after victory"), GEngine->GetWorldContextFromWorldChecked(Scene.World).TravelURL.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthDefeatTest, "FPS_Shooter.Mission.DefeatAndRestart", TestFlags)
bool FStealthDefeatTest::RunTest(const FString& Parameters)
{
	FStealthTestWorld Scene;
	Scene.LeaveStaging();
	UGameplayStatics::ApplyDamage(Scene.Player, 1000, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("Lethal damage clamps health to zero"), Scene.Player->GetHealth(), 0.0f);
	TestEqual(TEXT("Zero health loses mission"), Scene.Mission->GetMissionState(), EStealthMissionState::Lost);
	TestFalse(TEXT("Dead player cannot collect"), Scene.Mission->CollectObjective(Scene.Player));
	TestFalse(TEXT("Dead player cannot extract"), Scene.Mission->TryCompleteMission(Scene.Player));
	TestFalse(TEXT("Post-result actions disabled"), Scene.Player->CanPerformMissionAction());
	Scene.Controller->RestartMission();
	TestFalse(TEXT("Restart queues level reload after defeat"), GEngine->GetWorldContextFromWorldChecked(Scene.World).TravelURL.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthHearingTest, "FPS_Shooter.Mission.SoundInvestigation", TestFlags)
bool FStealthHearingTest::RunTest(const FString& Parameters)
{
	FStealthTestWorld Scene;
	Scene.LeaveStaging();
	ASoundHearingNPC* Guard = Scene.Spawn<ASoundHearingNPC>(FVector(3500, 0, 200));
	Guard->GetCharacterMovement()->DisableMovement();
	AStealthAIController* AI = Cast<AStealthAIController>(Guard->GetController());
	if (!TestNotNull(TEXT("Guard AI exists"), AI))
	{
		return false;
	}
	Scene.Player->SetActorLocation(FVector(3000, 0, 200));
	AActor* SoundSource = Scene.Player;
	USoundRayEmitterComponent* Emitter = NewObject<USoundRayEmitterComponent>(SoundSource);
	SoundSource->AddInstanceComponent(Emitter);
	Emitter->RegisterComponent();
	Emitter->bAutoEmitFootsteps = false;
	Emitter->bDirectionalSound = true;
	Emitter->bDrawDebug = false;
	Emitter->Volume = 0.7f;
	Emitter->MaxDistance = 1000.0f;
	Emitter->EmitFootstepSound();
	TestTrue(TEXT("Sound rays reach the hearing component"), Guard->HearingComponent->HearingEventId > 0);
	AI->Tick(0.1f);
	TestTrue(TEXT("Sound rays still cause a hearing reaction"), AI->GetAIState() == EStealthAIState::Investigate
		|| AI->GetAIState() == EStealthAIState::Search);
	TestFalse(TEXT("Distraction alone is not a confirmed pursuit"), Scene.Mission->IsPlayerPursued());
	Guard->HearingComponent->HearSoundAtLocation(FVector(3000, 0, 200), 1.0f);
	AI->Tick(0.1f);
	TestEqual(TEXT("New distant sound starts investigation"), AI->GetAIState(), EStealthAIState::Investigate);
	AI->Tick(7.1f);
	TestEqual(TEXT("Unreachable sound investigation times out"), AI->GetAIState(), EStealthAIState::Search);
	AI->Tick(7.1f);
	TestEqual(TEXT("Guard eventually returns to patrol"), AI->GetAIState(), EStealthAIState::Patrol);
	return true;
}

// Exercise the authored map with a real local controller, HUD, navigation and actual level reloads.
class FStealthLevelLifecycleCommand : public IAutomationLatentCommand
{
public:
	explicit FStealthLevelLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				World = Context.World();
				break;
			}
		}
		if (FPlatformTime::Seconds() - StageStarted > 45.0)
		{
			Test->AddError(FString::Printf(TEXT("Level lifecycle timed out at stage %d"), Stage));
			return true;
		}
		if (!World || !World->HasBegunPlay() || World == PreviousWorld.Get() || World->GetTimeSeconds() < 0.5f)
		{
			return false;
		}
		if (Stage == 0 && (World->GetTimeSeconds() < 2.0f || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)))
		{
			return false;
		}
		AStealthGameMode* Mission = World->GetAuthGameMode<AStealthGameMode>();
		AStealthPlayerController* PC = Cast<AStealthPlayerController>(World->GetFirstPlayerController());
		AStealthCharacter* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		if (!Test->TestNotNull(TEXT("Authored level uses stealth game mode"), Mission)
			|| !Test->TestNotNull(TEXT("Authored level uses stealth controller"), PC)
			|| !Test->TestNotNull(TEXT("Authored level uses stealth player"), Player))
		{
			return true;
		}
		Test->TestEqual(TEXT("Reload starts a fresh mission"), Mission->GetMissionState(), EStealthMissionState::Playing);
		Test->TestEqual(TEXT("Reload restores health"), Player->GetHealth(), 100.0f);
		Test->TestEqual(TEXT("Reload restores magazine"), Player->GetAmmoInMagazine(), 30);
		Test->TestEqual(TEXT("Reload restores reserve ammo"), Player->GetReserveAmmo(), 90);
		Test->TestEqual(TEXT("Reload restores distractions"), Player->GetNoiseMakerCount(), 3);
		Test->TestFalse(TEXT("Reload resets intel"), Mission->HasObjective());
		Test->TestTrue(TEXT("Reload restores safe start"), Mission->IsActorProtected(Player));
		Test->TestFalse(TEXT("Reload clears pursuit"), Mission->IsPlayerPursued());
		Test->TestFalse(TEXT("Reload resumes gameplay"), UGameplayStatics::IsGamePaused(World));
		AStealthHUD* HUD = Cast<AStealthHUD>(PC->GetHUD());
		if (!Test->TestNotNull(TEXT("C++ HUD is active on actual level"), HUD))
		{
			return true;
		}
		AStealthObjective* Objective = nullptr;
		AStealthExitZone* Exit = nullptr;
		for (TActorIterator<AStealthObjective> It(World); It; ++It) { Objective = *It; break; }
		for (TActorIterator<AStealthExitZone> It(World); It; ++It) { Exit = *It; break; }
		if (!Test->TestNotNull(TEXT("Intel exists on map after reload"), Objective)
			|| !Test->TestNotNull(TEXT("Exit exists on map after reload"), Exit))
		{
			return true;
		}
		if (Stage == 0)
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (Test->TestNotNull(TEXT("Level has navigation system"), Navigation))
			{
				for (const AActor* Destination : { static_cast<AActor*>(Objective), static_cast<AActor*>(Exit) })
				{
					FNavLocation Projected;
					const bool bProjected = Navigation->ProjectPointToNavigation(Destination->GetActorLocation(), Projected, FVector(200, 200, 300));
					Test->TestTrue(TEXT("Mission destination projects onto NavMesh"), bProjected);
					UNavigationPath* Path = bProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(World,
						Player->GetActorLocation(), Projected.Location) : nullptr;
					Test->TestTrue(TEXT("Mission destination has a complete walkable route"), Path && Path->IsValid() && !Path->IsPartial());
				}
			}
			Player->SetActorLocation(Player->GetActorLocation() + FVector(500, 0, 0));
			Mission->Tick(0.1f);
			UGameplayStatics::ApplyDamage(Player, 1000, nullptr, nullptr, UDamageType::StaticClass());
			Test->TestEqual(TEXT("Actual level reaches defeat"), Mission->GetMissionState(), EStealthMissionState::Lost);
			Test->TestTrue(TEXT("Result pauses the world"), UGameplayStatics::IsGamePaused(World));
			Test->TestTrue(TEXT("Result reveals mouse cursor"), PC->bShowMouseCursor);
			PreviousWorld = World;
			HUD->NotifyHitBoxClick(TEXT("RestartMission"));
		}
		else if (Stage == 1)
		{
			Player->SetActorLocation(Objective->GetActorLocation() - FVector(150, 0, 70));
			Mission->Tick(0.1f);
			const auto PressAndRelease = [PC](FKey Key)
			{
				PC->InputKey(FInputKeyParams(Key, IE_Pressed, 1.0));
				PC->PlayerTick(0.016f);
				PC->InputKey(FInputKeyParams(Key, IE_Released, 0.0));
				PC->PlayerTick(0.016f);
			};
			PressAndRelease(EKeys::LeftMouseButton);
			Test->TestEqual(TEXT("Player fire input consumes one round"), Player->GetAmmoInMagazine(), 29);
			PressAndRelease(EKeys::R);
			Test->TestTrue(TEXT("Reload starts an animated interval"), Player->IsReloading());
			Test->TestEqual(TEXT("Reload does not instantly create ammo"), Player->GetAmmoInMagazine(), 29);
			Player->Tick(2.3f);
			Test->TestEqual(TEXT("Reload input fills magazine"), Player->GetAmmoInMagazine(), 30);
			Test->TestEqual(TEXT("Reload consumes reserve ammo"), Player->GetReserveAmmo(), 89);
			PressAndRelease(EKeys::Q);
			Test->TestEqual(TEXT("Distraction input throws a noise maker"), Player->GetNoiseMakerCount(), 2);
			PC->InputKey(FInputKeyParams(EKeys::RightMouseButton, IE_Pressed, 1.0));
			PC->PlayerTick(0.016f);
			Player->Tick(0.25f);
			Test->TestTrue(TEXT("Aim input reduces camera FOV"), Player->FindComponentByClass<UCameraComponent>()->FieldOfView < 90.0f);
			PC->InputKey(FInputKeyParams(EKeys::RightMouseButton, IE_Released, 0.0));
			PC->PlayerTick(0.016f);
			Objective->Interact_Implementation(Player);
			Test->TestTrue(TEXT("Actual map intel collected"), Mission->HasObjective());
			Player->SetActorLocation(Exit->GetActorLocation());
			Player->UpdateOverlaps();
			Exit->Tick(0.1f);
			Test->TestEqual(TEXT("Actual map reaches victory"), Mission->GetMissionState(), EStealthMissionState::Won);
			Test->TestTrue(TEXT("Victory pauses the world"), UGameplayStatics::IsGamePaused(World));
			PreviousWorld = World;
			PC->InputKey(FInputKeyParams(EKeys::Enter, IE_Pressed, 1.0));
		}
		else
		{
			return true;
		}
		++Stage;
		StageStarted = FPlatformTime::Seconds();
		return false;
	}

private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<UWorld> PreviousWorld;
	int32 Stage = 0;
	double StageStarted = FPlatformTime::Seconds();
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthLevelLifecycleTest, "FPS_Shooter.Mission.LevelLifecycle", TestFlags)
bool FStealthLevelLifecycleTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true))
	{
		AddError(TEXT("Failed to load stealth map for PIE testing."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStealthLevelLifecycleCommand(this));
	return true;
}

#endif
