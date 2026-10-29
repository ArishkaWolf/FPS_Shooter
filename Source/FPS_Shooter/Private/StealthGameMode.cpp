#include "StealthGameMode.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthHUD.h"
#include "StealthPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthMission, Log, All);

AStealthGameMode::AStealthGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AStealthCharacter::StaticClass();
	HUDClass = AStealthHUD::StaticClass();
	PlayerControllerClass = AStealthPlayerController::StaticClass();
}

void AStealthGameMode::StartPlay()
{
	// Initialize before Super starts AI and other actors.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		SafeZoneCenter = Player->GetActorLocation();
		bSafeZoneInitialized = true;
	}
	Super::StartPlay();
	// Replace stale prototype tiles using the current mission bounds so all destinations are reachable.
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		Navigation->Build();
	}
}

void AStealthGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MissionState != EStealthMissionState::Playing)
	{
		return;
	}
	MissionTime += DeltaSeconds;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && !bSafeZoneInitialized)
	{
		SafeZoneCenter = Player->GetActorLocation();
		bSafeZoneInitialized = true;
	}
	if (Player && !bInfiltrationStarted && !IsActorProtected(Player))
	{
		bInfiltrationStarted = true;
		UE_LOG(LogStealthMission, Display, TEXT("Player left staging area. Infiltration started."));
	}
}

bool AStealthGameMode::IsMissionPlayer(const AActor* Actor) const
{
	const AStealthCharacter* Player = Cast<AStealthCharacter>(Actor);
	return IsValid(Player) && Player == UGameplayStatics::GetPlayerPawn(this, 0) && Player->GetHealth() > 0.0f;
}

bool AStealthGameMode::IsActorProtected(const AActor* Actor) const
{
	// Protection ends permanently on first departure: returning cannot grant invulnerability.
	return IsValid(Actor) && bSafeZoneInitialized && !bInfiltrationStarted
		&& FVector::DistSquared2D(Actor->GetActorLocation(), SafeZoneCenter) <= FMath::Square(SafeZoneRadius)
		&& FMath::Abs(Actor->GetActorLocation().Z - SafeZoneCenter.Z) < 250.0f;
}

bool AStealthGameMode::IsPlayerPursued() const
{
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(It->GetPawn()) && It->HasActivePursuit())
		{
			return true;
		}
	}
	return false;
}

float AStealthGameMode::GetDetectionLevel() const
{
	float Detection = 0.0f;
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		if (IsValid(It->GetPawn()))
		{
			Detection = FMath::Max(Detection, It->GetDetectionLevel());
		}
	}
	return Detection;
}

EStealthMissionStage AStealthGameMode::GetMissionStage() const
{
	if (!bInfiltrationStarted && IsActorProtected(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		return EStealthMissionStage::SafeZone;
	}
	if (IsPlayerPursued())
	{
		return EStealthMissionStage::Evade;
	}
	return bObjectiveCollected ? EStealthMissionStage::Extraction : EStealthMissionStage::Infiltration;
}

FString AStealthGameMode::GetObjectiveText() const
{
	switch (GetMissionStage())
	{
	case EStealthMissionStage::SafeZone:
		return FString::Printf(TEXT("SAFE START - Leave the %.1fm staging area. Steal INTEL [E], then reach EXIT."), SafeZoneRadius / 100.0f);
	case EStealthMissionStage::Evade:
		return TEXT("BREAK CONTACT - Hide behind cover until the guards finish searching.");
	case EStealthMissionStage::Extraction:
		return TEXT("INTEL SECURED - Reach the green EXIT to extract.");
	default:
		return TEXT("INFILTRATE - Find yellow INTEL. Aim at it and press [E] to collect.");
	}
}

FString AStealthGameMode::GetThreatText() const
{
	bool bInvestigating = false;
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		if (!IsValid(It->GetPawn()))
		{
			continue;
		}
		if (It->GetAIState() == EStealthAIState::Chase)
		{
			return TEXT("DETECTED - Guard opening fire. Break line of sight!");
		}
		bInvestigating |= It->GetAIState() != EStealthAIState::Patrol;
	}
	return IsPlayerPursued() ? TEXT("SEARCHING - Stay hidden until the search ends.")
		: bInvestigating ? TEXT("GUARD INVESTIGATING A SOUND")
		: TEXT("HIDDEN - Crouch for quiet footsteps. [Q] throws a distraction.");
}

bool AStealthGameMode::CollectObjective(AActor* Collector)
{
	if (MissionState != EStealthMissionState::Playing || bObjectiveCollected || !IsMissionPlayer(Collector)
		|| IsActorProtected(Collector))
	{
		return false;
	}
	bInfiltrationStarted = true;
	bObjectiveCollected = true;
	UE_LOG(LogStealthMission, Display, TEXT("Intel secured."));
	return true;
}

bool AStealthGameMode::TryCompleteMission(AActor* ExitingActor)
{
	if (MissionState != EStealthMissionState::Playing || !bObjectiveCollected || !IsMissionPlayer(ExitingActor)
		|| IsPlayerPursued())
	{
		return false;
	}
	FinishMission(EStealthMissionState::Won);
	return true;
}

void AStealthGameMode::DefeatPlayer(AActor* DefeatedActor)
{
	const AStealthCharacter* Player = Cast<AStealthCharacter>(DefeatedActor);
	if (MissionState == EStealthMissionState::Playing && IsValid(Player)
		&& Player == UGameplayStatics::GetPlayerPawn(this, 0) && Player->GetHealth() <= 0.0f)
	{
		FinishMission(EStealthMissionState::Lost);
	}
}

void AStealthGameMode::FinishMission(EStealthMissionState NewState)
{
	if (MissionState != EStealthMissionState::Playing || NewState == EStealthMissionState::Playing)
	{
		return;
	}
	MissionState = NewState;
	UE_LOG(LogStealthMission, Display, TEXT("Mission %s after %.1f seconds."),
		NewState == EStealthMissionState::Won ? TEXT("WON") : TEXT("LOST"), MissionTime);
	if (AStealthCharacter* Player = Cast<AStealthCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->StopMissionActions();
	}
	for (TActorIterator<AStealthAIController> It(GetWorld()); It; ++It)
	{
		It->StopMovement();
		It->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->ShowMissionResult();
	}
}

void AStealthGameMode::RestartMission()
{
	if (MissionState == EStealthMissionState::Playing || bRestartRequested)
	{
		return;
	}
	bRestartRequested = true;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetPause(false);
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
