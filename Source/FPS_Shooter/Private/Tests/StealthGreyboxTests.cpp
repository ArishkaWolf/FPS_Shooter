#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NoiseMakerPickup.h"
#include "NPCHearingComponent.h"
#include "SoundHearingNPC.h"
#include "SoundRayInteractionComponent.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthDoor.h"
#include "StealthExitZone.h"
#include "StealthGameMode.h"
#include "StealthObjective.h"
#include "StealthPatrolPoint.h"
#include "Tests/AutomationCommon.h"

namespace
{
class FGreyboxWalkthrough final : public IAutomationLatentCommand
{
public:
	explicit FGreyboxWalkthrough(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (FPlatformTime::Seconds() - Started > 330)
		{
			Test.AddError(FString::Printf(TEXT("Greybox walkthrough timed out at stage %d, path point %d"), Stage, Cursor));
			return true;
		}
		if (!World || World->WorldType != EWorldType::PIE || World->GetTimeSeconds() < 2) { return false; }
		APlayerController* PC = World->GetFirstPlayerController();
		AStealthCharacter* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		AStealthGameMode* Mission = World->GetAuthGameMode<AStealthGameMode>();
		if (!Player || !Mission) { Test.AddError(TEXT("No stealth player or mission")); return true; }
		if (Mission->GetMissionState() == EStealthMissionState::Lost)
		{
			Test.AddError(FString::Printf(TEXT("Player defeated at stage %d, %s"), Stage, *Player->GetActorLocation().ToString()));
			return true;
		}
		const double Now = World->GetTimeSeconds();
		if (Stage <= 2 && UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) { return false; }

		switch (Stage)
		{
		case 0:
			if (!Prepare(*World, *Player, *Mission)) { return true; }
			Gate->SetOpen(true, Player);
			StageStarted = Now;
			++Stage;
			return false;
		case 1:
		{
			FNPCHearingStimulus Heard;
			bGateHeard |= GateGuard->HearingComponent->GetBestSound(Heard)
				&& Heard.Event.EventId == Gate->SoundEmitter->GetLastEmittedEvent().EventId;
			if (Now - StageStarted < 2) { return false; }
			const UNavigationPath* OpenPath = FindPath(*World, GateWest, GateEast);
			if ((!OpenPath || OpenPath->GetPathLength() > 1000) && Now - StageStarted < 8) { return false; }
			Test.TestTrue(TEXT("Opened gate provides a local crossing"), OpenPath && OpenPath->GetPathLength() < 1000);
			Test.TestTrue(TEXT("Gate emits a Door event received by its patrol"), bGateHeard);
			if (const UNavigationPath* Short = FindPath(*World, Player->GetActorLocation(), Quiet.Last()))
			{
				Test.TestTrue(TEXT("Gate route is shorter than service route"), Short->GetPathLength() < QuietLength * 0.75);
				Test.AddInfo(FString::Printf(TEXT("Entry distances: %.1f m service, %.1f m through open gate"), QuietLength / 100, Short->GetPathLength() / 100));
			}
			else { Test.AddError(TEXT("No path from start through open gate to intel")); }
			Gate->SetOpen(false, Player);
			StageStarted = Now;
			++Stage;
			return false;
		}
		case 2:
			if (Now - StageStarted < 2) { return false; }
			Player->Crouch();
			UGameplayStatics::SetGlobalTimeDilation(World, 4.0f);
			Route = Approach;
			++Stage;
			return false;
		case 3:
			if (!Walk(*Player)) { return false; }
			Test.TestFalse(TEXT("Player left protected staging on foot"), Mission->IsActorProtected(Player));
			StageStarted = Now;
			++Stage;
			return false;
		case 4:
		{
			const bool bAtRetreat = FVector::Dist2D(IntelGuard->GetActorLocation(), Retreat) < 145;
			bSeenOutsideRetreat |= !bAtRetreat;
			if (!bAtRetreat || !bSeenOutsideRetreat)
			{
				if (Now - StageStarted > 95) { Test.AddError(TEXT("Intel patrol never opened its passage")); return true; }
				return false;
			}
			Test.TestFalse(TEXT("Player has no confirmed pursuit at the covered approach"), Mission->IsPlayerPursued());
			for (TActorIterator<ASoundHearingNPC> It(World); It; ++It)
			{
				FCollisionQueryParams Query(SCENE_QUERY_STAT(GreyboxIntelWindow), true, *It);
				Query.AddIgnoredActor(Player);
				const FVector Eye = It->GetActorLocation() + FVector(0, 0, 70);
				for (const FVector& Point : { Threshold, Quiet.Last() })
				{
					const FVector Target = Point + FVector(0, 0, 120);
					Test.TestTrue(TEXT("Intel threshold and approach are clear of guard sight during the window"),
						FVector::Dist2D(Eye, Target) > 1400 || World->LineTraceTestByChannel(Eye, Target, ECC_Visibility, Query));
				}
			}
			Route = Breach;
			Cursor = 0;
			++Stage;
			return false;
		}
		case 5:
		{
			if (!Walk(*Player)) { return false; }
			const UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>();
			PC->SetControlRotation((Objective->GetActorLocation() - Camera->GetComponentLocation()).Rotation());
			++Stage;
			return false;
		}
		case 6:
			PC->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
			PC->PlayerTick(0.016f);
			PC->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
			PC->PlayerTick(0.016f);
			if (!Test.TestTrue(TEXT("Player collects existing intel via interaction input"), Mission->HasObjective())) { return true; }
			Route = Return;
			Cursor = 0;
			++Stage;
			return false;
		case 7:
			if (Mission->GetMissionState() == EStealthMissionState::Won)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				Test.TestEqual(TEXT("Quiet route completed without damage"), Player->GetHealth(), 100.0f);
				Test.AddInfo(FString::Printf(TEXT("Continuous crouched route to intel and extraction: %.1f game seconds"), Now - Began));
				return true;
			}
			if (Walk(*Player)) { Test.AddError(TEXT("Reached exit without victory")); return true; }
			return false;
		}
		return true;
	}

private:
	static UNavigationPath* FindPath(UWorld& World, const FVector& From, const FVector& To)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		FNavLocation A, B;
		const FVector Projection(80, 80, 220);
		if (!Nav || !Nav->ProjectPointToNavigation(From, A, Projection)
			|| !Nav->ProjectPointToNavigation(To, B, Projection)) { return nullptr; }
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(&World, A.Location, B.Location);
		return Path && Path->IsValid() && !Path->IsPartial() ? Path : nullptr;
	}

	bool Append(UWorld& World, TArray<FVector>& Out, const FVector& From, const FVector& To, double* Length = nullptr)
	{
		const UNavigationPath* Path = FindPath(World, From, To);
		if (!Path)
		{
			Test.AddError(FString::Printf(TEXT("No complete path %s -> %s"), *From.ToString(), *To.ToString()));
			return false;
		}
		Out.Append(Path->PathPoints);
		if (Length) { *Length += Path->GetPathLength(); }
		return true;
	}

	bool Prepare(UWorld& World, AStealthCharacter& Player, AStealthGameMode& Mission)
	{
		Test.TestTrue(TEXT("Starts in protected staging"), Mission.IsActorProtected(&Player));
		Began = World.GetTimeSeconds();
		TMap<FName, FVector> Markers;
		int32 Supplies = 0, Distractions = 0;
		AStealthExitZone* Exit = nullptr;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			for (FName Tag : It->Tags) { Markers.Add(Tag, It->GetActorLocation()); }
			if (It->ActorHasTag(TEXT("Greybox.Short.Gate"))) { Gate = Cast<AStealthDoor>(*It); }
			Supplies += It->IsA<ANoiseMakerPickup>() ? 1 : 0;
			Distractions += It->ActorHasTag(TEXT("Greybox.Distraction")) ? 1 : 0;
			if (auto* Found = Cast<AStealthObjective>(*It)) { Objective = Found; }
			if (auto* Found = Cast<AStealthExitZone>(*It)) { Exit = Found; }
		}
		Test.TestTrue(TEXT("Supplies and two optional sound sources are authored"), Supplies >= 1 && Distractions >= 2);
		TMap<FName, TArray<AStealthPatrolPoint*>> Patrols;
		for (TActorIterator<AStealthPatrolPoint> It(&World); It; ++It) { Patrols.FindOrAdd(It->RouteName).Add(*It); }
		TSet<FName> GuardRoutes;
		int32 GuardCount = 0;
		for (TActorIterator<ASoundHearingNPC> It(&World); It; ++It)
		{
			++GuardCount;
			GuardRoutes.Add(It->PatrolRouteName);
			if (It->PatrolRouteName == TEXT("Intel")) { IntelGuard = *It; }
			if (It->PatrolRouteName == TEXT("Gate")) { GateGuard = *It; }
			if (auto* Points = Patrols.Find(It->PatrolRouteName); Points && Points->Num() > 1)
			{
				Points->Sort([](const AStealthPatrolPoint& A, const AStealthPatrolPoint& B) { return A.Order < B.Order; });
				const FVector First = (*Points)[0]->GetActorLocation();
				Test.TestTrue(TEXT("Guard starts on or can reach its first patrol point"),
					FVector::Dist2D(It->GetActorLocation(), First) <= 100 || FindPath(World, It->GetActorLocation(), First));
				for (int32 I = 0; I < Points->Num(); ++I)
				{
					Test.TestNotNull(TEXT("Every patrol segment has complete navigation"), FindPath(World,
						(*Points)[I]->GetActorLocation(), (*Points)[(I + 1) % Points->Num()]->GetActorLocation()));
				}
			}
			else { Test.AddError(TEXT("Guard has no patrol loop")); }
		}
		Test.TestTrue(TEXT("Four guards have four distinct patrol routes"), GuardCount == 4 && GuardRoutes.Num() == 4);
		GateWest = Markers.FindRef(TEXT("Greybox.Short.West"));
		GateEast = Markers.FindRef(TEXT("Greybox.Short.East"));
		Retreat = Markers.FindRef(TEXT("Greybox.Intel.Retreat"));
		Threshold = Markers.FindRef(TEXT("Greybox.Intel.Threshold"));
		for (int32 Index = 1; const FVector* Point = Markers.Find(FName(*FString::Printf(TEXT("Greybox.Quiet.%02d"), Index))); ++Index)
		{
			Quiet.Add(*Point);
		}
		if (!Test.TestTrue(TEXT("Mission actors and both route definitions exist"), Gate && GateGuard && IntelGuard
			&& Objective && Exit && Quiet.Num() >= 6 && !GateWest.IsNearlyZero() && !GateEast.IsNearlyZero()
			&& !Retreat.IsNearlyZero() && !Threshold.IsNearlyZero())) { return false; }
		const UNavigationPath* ClosedPath = FindPath(World, GateWest, GateEast);
		Test.TestTrue(TEXT("Closed gate forces the service detour"), ClosedPath && ClosedPath->GetPathLength() > 10000);
		const int32 WaitIndex = Quiet.Num() - 4;
		FVector From = Player.GetActorLocation();
		for (int32 I = 0; I < Quiet.Num(); ++I)
		{
			if (!Append(World, I <= WaitIndex ? Approach : Breach, From, Quiet[I], &QuietLength)) { return false; }
			From = Quiet[I];
		}
		for (int32 I = Quiet.Num() - 2; I >= 0; --I)
		{
			if (!Append(World, Return, From, Quiet[I])) { return false; }
			From = Quiet[I];
		}
		return Append(World, Return, From, Exit->GetActorLocation());
	}

	bool Walk(AStealthCharacter& Player)
	{
		while (Route.IsValidIndex(Cursor))
		{
			FVector Delta = Route[Cursor] - Player.GetActorLocation();
			Delta.Z = 0;
			if (Delta.SizeSquared() > FMath::Square(34.0))
			{
				Player.AddMovementInput(Delta.GetSafeNormal(), FMath::Clamp(Delta.Size() / 65.0, 0.4, 1.0));
				return false;
			}
			++Cursor;
		}
		Player.GetCharacterMovement()->StopMovementImmediately();
		return true;
	}

	FAutomationTestBase& Test;
	AStealthDoor* Gate = nullptr;
	ASoundHearingNPC* GateGuard = nullptr;
	ASoundHearingNPC* IntelGuard = nullptr;
	AStealthObjective* Objective = nullptr;
	FVector GateWest, GateEast, Retreat, Threshold;
	TArray<FVector> Quiet, Approach, Breach, Return, Route;
	int32 Stage = 0, Cursor = 0;
	double Started = FPlatformTime::Seconds(), StageStarted = 0, Began = 0, QuietLength = 0;
	bool bGateHeard = false, bSeenOutsideRetreat = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthGreyboxTest, "FPS_Shooter.Mission.GreyboxWalkthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStealthGreyboxTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true))
	{
		AddError(TEXT("Unable to open ThirdPersonMap in PIE"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FGreyboxWalkthrough(*this));
	return true;
}

#endif
