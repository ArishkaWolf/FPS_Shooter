#include "StealthGameMode.h"

#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "StealthCharacter.h"

AStealthGameMode::AStealthGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AStealthCharacter::StaticClass();
}

void AStealthGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GEngine)
	{
		return;
	}

	FString Message;
	FColor Color = FColor::White;
	switch (MissionState)
	{
	case EStealthMissionState::Won:
		Message = TEXT("MISSION COMPLETE\nYou reached the extraction point");
		Color = FColor::Green;
		break;
	case EStealthMissionState::Lost:
		Message = TEXT("MISSION FAILED\nThe guard caught you");
		Color = FColor::Red;
		break;
	default:
		Message = bObjectiveCollected
			? TEXT("OBJECTIVE SECURED - Reach the green exit")
			: TEXT("OBJECTIVE - Steal the yellow intel (E)");
		Color = bObjectiveCollected ? FColor::Green : FColor::Yellow;
		break;
	}

	GEngine->AddOnScreenDebugMessage(2401, 0.0f, Color, Message);
}

bool AStealthGameMode::CollectObjective(AActor* Collector)
{
	if (MissionState != EStealthMissionState::Playing || bObjectiveCollected || !IsValid(Collector))
	{
		return false;
	}

	bObjectiveCollected = true;
	return true;
}

bool AStealthGameMode::TryCompleteMission(AActor* ExitingActor)
{
	if (MissionState != EStealthMissionState::Playing || !bObjectiveCollected || !IsValid(ExitingActor))
	{
		return false;
	}

	FinishMission(EStealthMissionState::Won);
	return true;
}

void AStealthGameMode::DefeatPlayer(AActor* DefeatedActor)
{
	if (MissionState == EStealthMissionState::Playing && IsValid(DefeatedActor))
	{
		FinishMission(EStealthMissionState::Lost);
	}
}

void AStealthGameMode::FinishMission(EStealthMissionState NewState)
{
	MissionState = NewState;
	if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}
}
