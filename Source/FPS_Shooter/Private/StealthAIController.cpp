#include "StealthAIController.h"

#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NPCHearingComponent.h"
#include "SoundHearingNPC.h"
#include "StealthGameMode.h"
#include "StealthPatrolPoint.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthAI, Log, All);

AStealthAIController::AStealthAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = false;
}

void AStealthAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ControlledNPC = Cast<ASoundHearingNPC>(InPawn);
	PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	GatherPatrolPoints();
	UE_LOG(LogStealthAI, Display, TEXT("Possessed %s with %d patrol points on route %s."),
		*GetNameSafe(InPawn),
		PatrolPoints.Num(),
		ControlledNPC.IsValid() ? *ControlledNPC->PatrolRouteName.ToString() : TEXT("None"));

	if (ControlledNPC.IsValid() && ControlledNPC->HearingComponent)
	{
		LastProcessedHearingEventId = ControlledNPC->HearingComponent->HearingEventId;
	}

	SetAIState(EStealthAIState::Patrol);
}

void AStealthAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ControlledNPC.IsValid())
	{
		return;
	}

	if (const AStealthGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AStealthGameMode>() : nullptr;
		GameMode && GameMode->GetMissionState() != EStealthMissionState::Playing)
	{
		StopMovement();
		return;
	}

	if (!PlayerPawn.IsValid())
	{
		PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	StateTime += DeltaSeconds;
	UpdateSight(DeltaSeconds);
	UpdateHearing();
	UpdateState(DeltaSeconds);
}

FText AStealthAIController::GetStateDisplayName() const
{
	switch (AIState)
	{
	case EStealthAIState::Investigate:
		return NSLOCTEXT("StealthAI", "Investigate", "INVESTIGATE");
	case EStealthAIState::Search:
		return NSLOCTEXT("StealthAI", "Search", "SEARCH");
	case EStealthAIState::Chase:
		return NSLOCTEXT("StealthAI", "Chase", "CHASE");
	default:
		return DetectionLevel > KINDA_SMALL_NUMBER
			? NSLOCTEXT("StealthAI", "Suspicious", "SUSPICIOUS")
			: NSLOCTEXT("StealthAI", "Patrol", "PATROL");
	}
}

void AStealthAIController::GatherPatrolPoints()
{
	PatrolPoints.Reset();
	if (!ControlledNPC.IsValid() || !GetWorld())
	{
		return;
	}

	for (TActorIterator<AStealthPatrolPoint> It(GetWorld()); It; ++It)
	{
		if (It->RouteName == ControlledNPC->PatrolRouteName)
		{
			PatrolPoints.Add(*It);
		}
	}

	PatrolPoints.Sort([](const TWeakObjectPtr<AStealthPatrolPoint>& Left, const TWeakObjectPtr<AStealthPatrolPoint>& Right)
	{
		return Left.IsValid() && Right.IsValid() && Left->Order < Right->Order;
	});

	PatrolIndex = PatrolPoints.IsEmpty() ? INDEX_NONE : 0;
}

void AStealthAIController::UpdateSight(float DeltaSeconds)
{
	FVector SeenLocation;
	const bool bCanSeePlayer = CanSeePlayer(SeenLocation);
	if (bCanSeePlayer)
	{
		LastKnownPlayerLocation = SeenLocation;
		TimeSincePlayerSeen = 0.0f;

		const float Distance = FVector::Dist(ControlledNPC->GetActorLocation(), SeenLocation);
		const float DistanceScale = FMath::GetMappedRangeValueClamped(FVector2D(SightRadius, 150.0f), FVector2D(0.45f, 1.35f), Distance);
		DetectionLevel = FMath::Clamp(DetectionLevel + DetectionRate * DistanceScale * DeltaSeconds, 0.0f, 1.0f);
		if (DetectionLevel >= 1.0f && AIState != EStealthAIState::Chase)
		{
			SetAIState(EStealthAIState::Chase, SeenLocation);
		}
	}
	else
	{
		TimeSincePlayerSeen += DeltaSeconds;
		if (AIState != EStealthAIState::Chase)
		{
			DetectionLevel = FMath::Max(0.0f, DetectionLevel - DetectionDecayRate * DeltaSeconds);
		}
	}
}

void AStealthAIController::UpdateHearing()
{
	UNPCHearingComponent* Hearing = ControlledNPC.IsValid() ? ControlledNPC->HearingComponent : nullptr;
	if (!Hearing || Hearing->HearingEventId == LastProcessedHearingEventId)
	{
		return;
	}

	LastProcessedHearingEventId = Hearing->HearingEventId;
	if (Hearing->LastReceivedSoundStrength < InvestigateSoundThreshold || AIState == EStealthAIState::Chase)
	{
		return;
	}

	SetAIState(EStealthAIState::Investigate, Hearing->LastHeardPosition);
}

void AStealthAIController::UpdateState(float DeltaSeconds)
{
	if (!ControlledNPC.IsValid())
	{
		return;
	}

	switch (AIState)
	{
	case EStealthAIState::Patrol:
		if (PatrolIndex == INDEX_NONE)
		{
			break;
		}

		if (WaitTimeRemaining > 0.0f)
		{
			WaitTimeRemaining -= DeltaSeconds;
			if (WaitTimeRemaining <= 0.0f)
			{
				AdvancePatrol();
			}
		}
		else if (PatrolPoints.IsValidIndex(PatrolIndex) && PatrolPoints[PatrolIndex].IsValid()
			&& HasReachedDestination(PatrolPoints[PatrolIndex]->GetActorLocation()))
		{
			StopMovement();
			WaitTimeRemaining = PatrolPoints[PatrolIndex]->WaitTime;
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToCurrentPatrolPoint();
		}
		break;

	case EStealthAIState::Investigate:
		if (HasReachedDestination(SearchOrigin))
		{
			SetAIState(EStealthAIState::Search, SearchOrigin);
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToLocation(SearchOrigin, 90.0f, true, true, true, false, nullptr, true);
		}
		break;

	case EStealthAIState::Search:
		if (StateTime >= SearchDuration)
		{
			DetectionLevel = 0.0f;
			SetAIState(EStealthAIState::Patrol);
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToRandomSearchPoint();
		}
		break;

	case EStealthAIState::Chase:
		if (PlayerPawn.IsValid()
			&& FVector::DistSquared(ControlledNPC->GetActorLocation(), PlayerPawn->GetActorLocation()) <= FMath::Square(CaptureDistance))
		{
			FVector SeenLocation;
			if (CanSeePlayer(SeenLocation))
			{
				if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
				{
					GameMode->DefeatPlayer(PlayerPawn.Get());
				}
				StopMovement();
				break;
			}
		}

		if (TimeSincePlayerSeen <= LoseSightDelay && PlayerPawn.IsValid())
		{
			MoveToActor(PlayerPawn.Get(), 100.0f, true, true, true, nullptr, true);
		}
		else
		{
			DetectionLevel = 0.6f;
			SetAIState(EStealthAIState::Search, LastKnownPlayerLocation);
		}
		break;
	}
}

void AStealthAIController::SetAIState(EStealthAIState NewState, const FVector& FocusLocation)
{
	const EStealthAIState PreviousState = AIState;
	AIState = NewState;
	StateTime = 0.0f;
	WaitTimeRemaining = 0.0f;
	StopMovement();
	if (PreviousState != NewState)
	{
		UE_LOG(LogStealthAI, Display, TEXT("%s state changed from %d to %d."),
			*GetNameSafe(GetPawn()),
			static_cast<int32>(PreviousState),
			static_cast<int32>(NewState));
	}

	if (!ControlledNPC.IsValid())
	{
		return;
	}

	switch (AIState)
	{
	case EStealthAIState::Patrol:
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->PatrolSpeed;
		MoveToCurrentPatrolPoint();
		break;
	case EStealthAIState::Investigate:
		SearchOrigin = FocusLocation;
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->InvestigateSpeed;
		MoveToLocation(SearchOrigin, 90.0f, true, true, true, false, nullptr, true);
		break;
	case EStealthAIState::Search:
		SearchOrigin = FocusLocation;
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->InvestigateSpeed;
		MoveToRandomSearchPoint();
		break;
	case EStealthAIState::Chase:
		LastKnownPlayerLocation = FocusLocation;
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->ChaseSpeed;
		if (PlayerPawn.IsValid())
		{
			MoveToActor(PlayerPawn.Get(), 100.0f, true, true, true, nullptr, true);
		}
		break;
	}
}

void AStealthAIController::MoveToCurrentPatrolPoint()
{
	if (!PatrolPoints.IsValidIndex(PatrolIndex) || !PatrolPoints[PatrolIndex].IsValid())
	{
		return;
	}

	MoveToActor(PatrolPoints[PatrolIndex].Get(), 75.0f, true, true, true, nullptr, true);
}

void AStealthAIController::AdvancePatrol()
{
	if (PatrolPoints.IsEmpty())
	{
		return;
	}

	PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
	MoveToCurrentPatrolPoint();
}

void AStealthAIController::MoveToRandomSearchPoint()
{
	if (!GetWorld())
	{
		return;
	}

	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation SearchLocation;
		if (NavigationSystem->GetRandomReachablePointInRadius(SearchOrigin, SearchRadius, SearchLocation))
		{
			MoveToLocation(SearchLocation.Location, 80.0f, true, true, true, false, nullptr, true);
		}
	}
}

bool AStealthAIController::CanSeePlayer(FVector& OutPlayerLocation) const
{
	if (!ControlledNPC.IsValid() || !PlayerPawn.IsValid() || !GetWorld())
	{
		return false;
	}

	const FVector EyeLocation = ControlledNPC->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const FVector TargetLocation = PlayerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
	const FVector ToPlayer = TargetLocation - EyeLocation;
	if (ToPlayer.SizeSquared() > FMath::Square(SightRadius))
	{
		return false;
	}

	const FVector DirectionToPlayer = ToPlayer.GetSafeNormal();
	const float MinSightDot = FMath::Cos(FMath::DegreesToRadians(SightAngleDegrees * 0.5f));
	if (FVector::DotProduct(ControlledNPC->GetActorForwardVector(), DirectionToPlayer) < MinSightDot)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StealthSight), true, ControlledNPC.Get());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TargetLocation, ECC_Visibility, QueryParams);
	if (bHit && Hit.GetActor() != PlayerPawn.Get())
	{
		return false;
	}

	OutPlayerLocation = PlayerPawn->GetActorLocation();
	return true;
}

bool AStealthAIController::HasReachedDestination(const FVector& Destination, float AcceptanceRadius) const
{
	return ControlledNPC.IsValid()
		&& FVector::DistSquared2D(ControlledNPC->GetActorLocation(), Destination) <= FMath::Square(AcceptanceRadius);
}
