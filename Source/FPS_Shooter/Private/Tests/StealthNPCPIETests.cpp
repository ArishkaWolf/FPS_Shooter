#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NPCHearingComponent.h"
#include "SoundHearingNPC.h"
#include "SoundRayEmitterComponent.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

// Exercise actual character movement, path following, in-place turns and evaluated poses.
// No search-point teleportation or direct AI state mutation is used.
class FStealthNPCPIECommand : public IAutomationLatentCommand
{
public:
	explicit FStealthNPCPIECommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (FPlatformTime::Seconds() - WallStarted > 150.0)
		{
			Test->AddError(FString::Printf(TEXT("NPC PIE timed out at stage %d"), Stage));
			return true;
		}
		if (!World || World->WorldType != EWorldType::PIE || !World->HasBegunPlay()
			|| World->GetTimeSeconds() < 2.0f || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) { return false; }
		APlayerController* PC = World->GetFirstPlayerController();
		AStealthCharacter* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		if (!Player) { Test->AddError(TEXT("No player in NPC PIE")); return true; }
		if (Stage > 0 && !Guard.IsValid()) { Test->AddError(TEXT("NPC disappeared during PIE")); return true; }
		if (Stage == 0)
		{
			for (TActorIterator<ASoundHearingNPC> It(World); It; ++It)
			{
				It->GetCharacterMovement()->DisableMovement();
				if (AController* Controller = It->GetController()) { Controller->SetActorTickEnabled(false); }
			}
			for (USoundRayEmitterComponent* Emitter : TInlineComponentArray<USoundRayEmitterComponent*>(Player))
			{
				Emitter->bAutoEmitFootsteps = false;
			}
			Player->SetActorLocation(Player->GetActorLocation() + FVector(10000, 0, 0));
			Player->GetCharacterMovement()->DisableMovement();
			Player->SetActorHiddenInGame(true);
			World->GetAuthGameMode<AStealthGameMode>()->Tick(0.01f);
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FVector Origin, Start;
			bool bFound = false;
			// Find a real open region of the authored level, rather than assuming empty space.
			for (float X = 500; X <= 2500 && !bFound; X += 500)
			{
				for (float Y = 500; Y <= 2500 && !bFound; Y += 500)
				{
					FNavLocation Center, Begin;
					if (!Nav || !Nav->ProjectPointToNavigation(FVector(X, Y, 150), Center, FVector(100, 100, 600))
						|| !Nav->ProjectPointToNavigation(Center.Location - FVector(300, 0, 0), Begin, FVector(100, 100, 100))) { continue; }
					int32 Reachable = 0;
					for (int32 Index = 0; Index < 4; ++Index)
					{
						FNavLocation Neighbor;
						const FVector Candidate = Center.Location + FRotator(0, Index * 90.0f, 0).Vector() * 450.0f;
						if (!Nav->ProjectPointToNavigation(Candidate, Neighbor, FVector(100, 100, 100))) { continue; }
						UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Begin.Location, Neighbor.Location);
						if (Path && Path->IsValid() && !Path->IsPartial()) { ++Reachable; }
					}
					if (Reachable >= 4) { Origin = Center.Location; Start = Begin.Location; bFound = true; }
				}
			}
			if (!Test->TestTrue(TEXT("Authored NavMesh has a reachable search region"), bFound)) { return true; }
			const FTransform SpawnTransform(Start + FVector(0, 0, 98));
			Guard = World->SpawnActorDeferred<ASoundHearingNPC>(ASoundHearingNPC::StaticClass(), SpawnTransform,
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Guard->PatrolRouteName = TEXT("NPC_Automation_Only");
			Guard->NPCLabel = FText::FromString(TEXT("NPC QA"));
			Guard->FinishSpawning(SpawnTransform);
			AI = CastChecked<AStealthAIController>(Guard->GetController());
			Guard->FireDamage = 0.0f;
			Guard->HearingComponent->HearSoundAtLocation(Origin + FVector(0, 0, 98), 0.8f);
			PreviousLocation = Guard->GetActorLocation();
			Test->AddInfo(FString::Printf(TEXT("PIE search start %s, evidence %s"), *PreviousLocation.ToString(), *Origin.ToString()));
			Next(World);
			return false;
		}
		Travelled += FVector::Dist2D(PreviousLocation, Guard->GetActorLocation());
		PreviousLocation = Guard->GetActorLocation();
		const float Age = World->GetTimeSeconds() - StageStarted;
		switch (Stage)
		{
		case 1:
			if (AI->GetAIState() == EStealthAIState::Investigate)
			{
				Test->AddInfo(TEXT("PIE Patrol -> Investigate")); Next(World);
			}
			else if (Age > 2.0f) { Test->AddError(TEXT("Strong sound did not start PIE investigation")); return true; }
			break;
		case 2:
			if (AI->GetAIState() == EStealthAIState::Search)
			{
				Test->TestTrue(TEXT("Search plans multiple reachable points"), AI->GetSearchPointCount() >= 3);
				Test->AddInfo(FString::Printf(TEXT("PIE Investigate -> Search, planned %d points"), AI->GetSearchPointCount()));
				Next(World);
			}
			else if (Age > 9.0f) { Test->AddError(TEXT("PIE investigation failed to reach Search")); return true; }
			break;
		case 3:
			if (AI->IsLookingAround())
			{
				if (AI->GetSearchPointsVisited() != LastVisited)
				{
					LastVisited = AI->GetSearchPointsVisited();
					InspectionLocations.Add(Guard->GetActorLocation());
					LastLookYaw = Guard->GetActorRotation().Yaw;
				}
				else
				{
					LookRotation += FMath::Abs(FMath::FindDeltaAngleDegrees(LastLookYaw, Guard->GetActorRotation().Yaw));
					LastLookYaw = Guard->GetActorRotation().Yaw;
					bStoppedToLook |= Guard->GetVelocity().Size2D() < 5.0f;
				}
			}
			if (AI->GetAIState() == EStealthAIState::Patrol)
			{
				Test->TestTrue(TEXT("Search physically visits at least three locations"), InspectionLocations.Num() >= 3);
				for (int32 Index = 1; Index < InspectionLocations.Num(); ++Index)
				{
					Test->TestTrue(TEXT("Inspection positions are spatially distinct"),
						FVector::Dist2D(InspectionLocations[Index - 1], InspectionLocations[Index]) > 120.0f);
				}
				Test->TestTrue(TEXT("Guard walks between investigation/search points"), Travelled > 600.0f);
				Test->TestTrue(TEXT("Guard stops at search points"), bStoppedToLook);
				Test->TestTrue(TEXT("Guard turns while inspecting"), LookRotation > 30.0f);
				Test->AddInfo(FString::Printf(TEXT("PIE Search -> Patrol: %d stops, %.0f cm walked, %.0f degrees scanned"),
					InspectionLocations.Num(), Travelled, LookRotation));
				AI->SetActorTickEnabled(false);
				AI->StopMovement();
				Guard->GetCharacterMovement()->DisableMovement();
				Guard->SetActorRotation(FRotator::ZeroRotator);
				Camera = World->SpawnActor<ACameraActor>(Guard->GetActorLocation() + FVector(300, 160, 65), FRotator::ZeroRotator);
				Camera->SetActorRotation((Guard->GetActorLocation() + FVector(0, 0, 20) - Camera->GetActorLocation()).Rotation());
				PC->SetViewTarget(Camera.Get());
				Next(World);
			}
			else if (Age > 24.0f) { Test->AddError(TEXT("PIE Search never returned to Patrol")); return true; }
			break;
		case 4:
			if (Age < 0.3f) { break; }
			IdleHand = Guard->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_Component);
			IdleSpine = Guard->GetMesh()->GetSocketTransform(TEXT("spine_03"), RTS_Component);
			Test->TestTrue(TEXT("Rifle grip stays in the right hand"), FVector::Dist(
				Guard->WeaponMesh->GetSocketLocation(TEXT("Grip_Bone")), Guard->GetMesh()->GetSocketLocation(TEXT("hand_r"))) < 1.0f);
			Capture(TEXT("NPC_Idle"));
			Next(World);
			break;
		case 5:
			if (Age < 0.1f) { break; }
			Guard->FireWeaponAt(Camera.Get());
			Next(World);
			break;
		case 6:
			if (Age < 0.07f) { break; }
			Test->TestFalse(TEXT("Recoil changes evaluated arm pose"), Guard->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_Component).Equals(IdleHand, 0.01f));
			Capture(TEXT("NPC_Fire"));
			Next(World);
			break;
		case 7:
			if (Age < 0.1f) { break; }
			UGameplayStatics::ApplyDamage(Guard.Get(), 20, nullptr, nullptr, UDamageType::StaticClass());
			Next(World);
			break;
		case 8:
			if (Age < 0.18f) { break; }
			Test->TestTrue(TEXT("Hit reaction advances in PIE"), Guard->GetHitAnimationAge() >= 0.17f);
			Test->TestFalse(TEXT("Hit reaction changes evaluated torso pose"),
				Guard->GetMesh()->GetSocketTransform(TEXT("spine_03"), RTS_Component).Equals(IdleSpine, 0.01f));
			Capture(TEXT("NPC_Hit"));
			Next(World);
			break;
		case 9:
			if (Age < 0.1f) { break; }
			StandingPelvis = Guard->GetMesh()->GetSocketLocation(TEXT("pelvis"));
			UGameplayStatics::ApplyDamage(Guard.Get(), 200, nullptr, nullptr, UDamageType::StaticClass());
			Next(World);
			break;
		case 10:
			if (Age < 2.4f) { break; }
			Test->TestTrue(TEXT("Death remains visible after clip completes"), Guard->IsDead() && !Guard->IsActorBeingDestroyed());
			Test->TestTrue(TEXT("Evaluated death pose lowers body"), Guard->GetMesh()->GetSocketLocation(TEXT("pelvis")).Z < StandingPelvis.Z - 20.0f);
			Capture(TEXT("NPC_Death"));
			Next(World);
			break;
		case 11:
			if (Age < 0.1f) { break; }
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ASoundHearingNPC> Guard;
	TWeakObjectPtr<AStealthAIController> AI;
	TWeakObjectPtr<ACameraActor> Camera;
	int32 Stage = 0, LastVisited = 0;
	float StageStarted = 0, Travelled = 0, LookRotation = 0, LastLookYaw = 0;
	bool bStoppedToLook = false;
	FVector PreviousLocation, StandingPelvis;
	FTransform IdleHand, IdleSpine;
	TArray<FVector> InspectionLocations;
	double WallStarted = FPlatformTime::Seconds();
	void Next(UWorld* World) { ++Stage; StageStarted = World->GetTimeSeconds(); }
	void Capture(const TCHAR* Name)
	{
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("NPCWork/Screenshots");
		IFileManager::Get().MakeDirectory(*Directory, true);
		FScreenshotRequest::RequestScreenshot(Directory / FString(Name) + TEXT(".png"), false, false);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthNPCPIETest, "FPS_Shooter.NPC.SearchNavigationAndAnimationPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStealthNPCPIETest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true))
	{
		AddError(TEXT("Failed to load mission map for NPC PIE"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStealthNPCPIECommand(this));
	return true;
}

#endif
