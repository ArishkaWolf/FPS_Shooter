#include "StealthAIController.h"

#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NPCHearingComponent.h"
#include "SoundHearingNPC.h"
#include "StealthCharacter.h"
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
	bStoppedForDeath = false;
	SetActorTickEnabled(true);
	PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	GatherPatrolPoints();
	DetectionLevel = 0.0f;
	TimeSincePlayerSeen = BIG_NUMBER;
	bHasSoundFocus = false;
	LastProcessedHearingEventId = ControlledNPC.IsValid() && ControlledNPC->HearingComponent
		? ControlledNPC->HearingComponent->HearingEventId : 0;
	SetAIState(EStealthAIState::Patrol);
}

void AStealthAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ControlledNPC.IsValid() || ControlledNPC->IsDead()) { return; }
	if (const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		Mission && Mission->GetMissionState() != EStealthMissionState::Playing)
	{
		StopMovement();
		CancelBurst();
		return;
	}
	if (!PlayerPawn.IsValid()) { PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0); }
	StateTime += DeltaSeconds;
	TimeUntilNextShot = FMath::Max(0.0f, TimeUntilNextShot - DeltaSeconds);
	UpdateSight(DeltaSeconds);
	UpdateHearing(DeltaSeconds);
	UpdateState(DeltaSeconds);
}

FText AStealthAIController::GetStateDisplayName() const
{
	switch (AIState)
	{
	case EStealthAIState::Suspicious: return NSLOCTEXT("StealthAI", "Suspicious", "SUSPICIOUS");
	case EStealthAIState::Investigate: return NSLOCTEXT("StealthAI", "Investigate", "INVESTIGATE");
	case EStealthAIState::Search: return NSLOCTEXT("StealthAI", "Search", "SEARCH");
	case EStealthAIState::Chase: return NSLOCTEXT("StealthAI", "Chase", "CHASE");
	default: return NSLOCTEXT("StealthAI", "Patrol", "PATROL");
	}
}

void AStealthAIController::GatherPatrolPoints()
{
	PatrolPoints.Reset();
	if (!ControlledNPC.IsValid() || !GetWorld()) { return; }
	for (TActorIterator<AStealthPatrolPoint> It(GetWorld()); It; ++It)
	{
		if (It->RouteName == ControlledNPC->PatrolRouteName) { PatrolPoints.Add(*It); }
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
	const float Visibility = GetPlayerVisibility(SeenLocation);
	bPlayerVisible = Visibility > 0.0f;
	if (bPlayerVisible)
	{
		LastKnownPlayerLocation = SeenLocation;
		TimeSincePlayerSeen = 0.0f;
		const float Distance = FVector::Dist(ControlledNPC->GetActorLocation(), SeenLocation);
		const float DistanceScale = FMath::GetMappedRangeValueClamped(
			FVector2D(SightRadius, 150.0f), FVector2D(0.45f, 1.35f), Distance);
		DetectionLevel = FMath::Clamp(DetectionLevel + DetectionRate * DistanceScale * Visibility * DeltaSeconds, 0.0f, 1.0f);
		if (DetectionLevel >= 1.0f && AIState != EStealthAIState::Chase)
		{
			SetAIState(EStealthAIState::Chase, SeenLocation);
		}
		else if (AIState == EStealthAIState::Patrol && DetectionLevel >= SuspiciousDetectionThreshold)
		{
			bHasSoundFocus = false;
			SetAIState(EStealthAIState::Suspicious, SeenLocation);
		}
		if (AIState == EStealthAIState::Suspicious)
		{
			SearchOrigin = SeenLocation;
			SetFocalPoint(SeenLocation);
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

void AStealthAIController::UpdateHearing(float DeltaSeconds)
{
	SoundFocusAge += DeltaSeconds;
	UNPCHearingComponent* Hearing = ControlledNPC->HearingComponent;
	if (!Hearing) { return; }
	if (const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		Mission && Mission->IsActorProtected(PlayerPawn.Get()))
	{
		LastProcessedHearingEventId = Hearing->HearingEventId;
		Hearing->ClearSoundMemory();
		return;
	}
	FNPCHearingStimulus Sound;
	if (!Hearing->GetBestSound(Sound) || Sound.Id <= LastProcessedHearingEventId) { return; }
	// Consume each selected observation once: old evidence cannot restart a completed search.
	LastProcessedHearingEventId = Sound.Id;
	if (AIState == EStealthAIState::Chase || bPlayerVisible || Sound.Age > 4.0f) { return; }
	const float Priority = Hearing->GetSoundPriority(Sound);
	if (Priority < SuspiciousSoundThreshold) { return; }
	if (bHasSoundFocus && AIState != EStealthAIState::Patrol)
	{
		const FNPCHearingStimulus Active { SearchOrigin, SoundFocusStrength, SoundFocusAge, 0 };
		// Hysteresis avoids oscillation between similar reflections and footsteps.
		if (Priority < Hearing->GetSoundPriority(Active) * 1.15f) { return; }
	}
	const bool bSameArea = bHasSoundFocus && FVector::DistSquared2D(SearchOrigin, Sound.Location) < FMath::Square(120.0f);
	bHasSoundFocus = true;
	SoundFocusStrength = Sound.Strength;
	SoundFocusAge = Sound.Age;
	if (bSameArea && AIState != EStealthAIState::Patrol
		&& !(AIState == EStealthAIState::Suspicious && Priority >= InvestigateSoundThreshold)) { return; }
	// A sound can redirect a confirmed search but must not erase its pursuit flag.
	const EStealthAIState Reaction = bSearchingForPlayer ? EStealthAIState::Search
		: Priority >= InvestigateSoundThreshold ? EStealthAIState::Investigate : EStealthAIState::Suspicious;
	SetAIState(Reaction, Sound.Location);
}

void AStealthAIController::UpdateState(float DeltaSeconds)
{
	switch (AIState)
	{
	case EStealthAIState::Patrol:
		if (PatrolIndex == INDEX_NONE) { break; }
		if (WaitTimeRemaining > 0.0f)
		{
			WaitTimeRemaining -= DeltaSeconds;
			if (WaitTimeRemaining <= 0.0f) { AdvancePatrol(); }
		}
		else if (PatrolPoints.IsValidIndex(PatrolIndex) && PatrolPoints[PatrolIndex].IsValid()
			&& HasReachedDestination(PatrolPoints[PatrolIndex]->GetActorLocation()))
		{
			StopMovement();
			WaitTimeRemaining = PatrolPoints[PatrolIndex]->WaitTime;
			if (WaitTimeRemaining <= 0.0f) { AdvancePatrol(); }
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle) { MoveToCurrentPatrolPoint(); }
		break;
	case EStealthAIState::Suspicious:
		if (StateTime >= SuspiciousDuration && !bPlayerVisible)
		{
			SetAIState(EStealthAIState::Investigate, SearchOrigin);
		}
		break;
	case EStealthAIState::Investigate:
		if (HasReachedDestination(SearchOrigin) || StateTime >= 7.0f)
		{
			SetAIState(EStealthAIState::Search, SearchOrigin);
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToLocation(SearchOrigin, 65.0f, true, true, true, false, nullptr, false);
		}
		break;
	case EStealthAIState::Search:
		UpdateSearch(DeltaSeconds);
		break;
	case EStealthAIState::Chase:
		if (TimeSincePlayerSeen > LoseSightDelay || !PlayerPawn.IsValid())
		{
			DetectionLevel = 0.6f;
			SetAIState(EStealthAIState::Search, LastKnownPlayerLocation);
			break;
		}
		if (bPlayerVisible && FVector::Dist(ControlledNPC->GetActorLocation(), LastKnownPlayerLocation) <= FireRange)
		{
			StopMovement();
			SetTurningInPlace(true);
			SetFocus(PlayerPawn.Get());
			const FVector AimDirection = (LastKnownPlayerLocation - ControlledNPC->GetActorLocation()).GetSafeNormal2D();
			const bool bFacingTarget = FVector::DotProduct(ControlledNPC->GetActorForwardVector(), AimDirection)
				>= FMath::Cos(FMath::DegreesToRadians(10.0f));
			if (bFacingTarget && TimeUntilNextShot <= 0.0f)
			{
				if (ShotsRemainingInBurst == 0) { ShotsRemainingInBurst = FMath::Max(1, BurstSize); }
				if (ControlledNPC->FireWeaponAt(PlayerPawn.Get()))
				{
					--ShotsRemainingInBurst;
					TimeUntilNextShot = ShotsRemainingInBurst > 0 ? BurstShotInterval : BurstPause;
				}
				else { CancelBurst(); }
			}
		}
		else
		{
			CancelBurst();
			ClearFocus(EAIFocusPriority::Gameplay);
			SetTurningInPlace(false);
			MoveToLocation(LastKnownPlayerLocation, 65.0f, true, true, true, false, nullptr, false);
		}
		break;
	}
}

void AStealthAIController::SetAIState(EStealthAIState NewState, const FVector& FocusLocation)
{
	const EStealthAIState PreviousState = AIState;
	CancelBurst();
	if (NewState == EStealthAIState::Chase)
	{
		bSearchingForPlayer = true;
		bHasSoundFocus = false;
		TimeUntilNextShot = FireInterval;
	}
	else if (NewState == EStealthAIState::Patrol)
	{
		bSearchingForPlayer = false;
		bHasSoundFocus = false;
		DetectionLevel = 0.0f;
		if (ControlledNPC.IsValid() && ControlledNPC->HearingComponent)
		{
			LastProcessedHearingEventId = ControlledNPC->HearingComponent->HearingEventId;
		}
	}
	AIState = NewState;
	ClearFocus(EAIFocusPriority::Gameplay);
	StateTime = 0.0f;
	WaitTimeRemaining = 0.0f;
	bSearchLooking = false;
	StopMovement();
	SetTurningInPlace(false);
	if (PreviousState != NewState)
	{
		UE_LOG(LogStealthAI, Display, TEXT("%s: %s -> %s"), *GetNameSafe(GetPawn()),
			*UEnum::GetValueAsString(PreviousState), *UEnum::GetValueAsString(NewState));
	}
	if (!ControlledNPC.IsValid()) { return; }
	switch (AIState)
	{
	case EStealthAIState::Patrol:
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->PatrolSpeed;
		MoveToCurrentPatrolPoint();
		break;
	case EStealthAIState::Suspicious:
		SearchOrigin = FocusLocation;
		SetTurningInPlace(true);
		SetFocalPoint(FocusLocation);
		break;
	case EStealthAIState::Investigate:
		SearchOrigin = FocusLocation;
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->InvestigateSpeed;
		MoveToLocation(SearchOrigin, 65.0f, true, true, true, false, nullptr, false);
		break;
	case EStealthAIState::Search:
		SearchOrigin = FocusLocation;
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->InvestigateSpeed;
		BuildSearchPoints();
		MoveToSearchPoint();
		break;
	case EStealthAIState::Chase:
		LastKnownPlayerLocation = FocusLocation;
		ControlledNPC->GetCharacterMovement()->MaxWalkSpeed = ControlledNPC->ChaseSpeed;
		MoveToLocation(LastKnownPlayerLocation, 65.0f, true, true, true, false, nullptr, false);
		break;
	}
}

void AStealthAIController::MoveToCurrentPatrolPoint()
{
	if (PatrolPoints.IsValidIndex(PatrolIndex) && PatrolPoints[PatrolIndex].IsValid())
	{
		MoveToActor(PatrolPoints[PatrolIndex].Get(), 60.0f, true, true, true, nullptr, false);
	}
}

void AStealthAIController::AdvancePatrol()
{
	if (PatrolPoints.IsEmpty()) { return; }
	PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
	MoveToCurrentPatrolPoint();
}

void AStealthAIController::BuildSearchPoints()
{
	SearchPoints.Reset();
	SearchPointIndex = 0;
	SearchPointsVisited = 0;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		SearchPoints.Add(ControlledNPC->GetActorLocation());
		return;
	}
	const float BaseAngle = (SearchOrigin - ControlledNPC->GetActorLocation()).Rotation().Yaw;
	for (int32 Index = 0; Index < SearchPointCount; ++Index)
	{
		const float Angle = BaseAngle + (Index - 1) * 360.0f / FMath::Max(1, SearchPointCount - 1);
		const FVector Candidate = SearchOrigin + (Index == 0 ? FVector::ZeroVector
			: FRotator(0, Angle, 0).Vector() * SearchRadius * (Index % 2 == 0 ? 0.65f : 1.0f));
		FNavLocation Projected;
		if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(160, 160, 250))) { continue; }
		if (SearchPoints.ContainsByPredicate([&](const FVector& Point)
			{ return FVector::DistSquared2D(Point, Projected.Location) < FMath::Square(140.0f); })) { continue; }
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),
			ControlledNPC->GetActorLocation(), Projected.Location, ControlledNPC.Get());
		if (Path && Path->IsValid() && !Path->IsPartial()) { SearchPoints.Add(Projected.Location); }
	}
	if (SearchPoints.IsEmpty()) { SearchPoints.Add(ControlledNPC->GetActorLocation()); }
}

FVector AStealthAIController::GetCurrentSearchPoint() const
{
	return SearchPoints.IsValidIndex(SearchPointIndex) ? SearchPoints[SearchPointIndex] : SearchOrigin;
}

void AStealthAIController::MoveToSearchPoint()
{
	SearchPointTime = 0.0f;
	bSearchLooking = false;
	ClearFocus(EAIFocusPriority::Gameplay);
	SetTurningInPlace(false);
	if (MoveToLocation(GetCurrentSearchPoint(), 60.0f, true, true, true, false, nullptr, false)
		== EPathFollowingRequestResult::Failed) { BeginSearchLook(); }
}

void AStealthAIController::BeginSearchLook()
{
	StopMovement();
	SetTurningInPlace(true);
	bSearchLooking = true;
	SearchPointTime = 0.0f;
	LookBaseYaw = ControlledNPC->GetActorRotation().Yaw;
	++SearchPointsVisited;
}

void AStealthAIController::UpdateSearch(float DeltaSeconds)
{
	if (StateTime >= SearchDuration)
	{
		SetAIState(EStealthAIState::Patrol);
		return;
	}
	SearchPointTime += DeltaSeconds;
	if (bSearchLooking)
	{
		const float Sweep = FMath::Sin(SearchPointTime / SearchLookDuration * 2.0f * PI) * 65.0f;
		SetFocalPoint(ControlledNPC->GetActorLocation() + FRotator(0, LookBaseYaw + Sweep, 0).Vector() * 500.0f);
		if (SearchPointTime >= SearchLookDuration)
		{
			if (++SearchPointIndex >= SearchPoints.Num()) { SetAIState(EStealthAIState::Patrol); }
			else { MoveToSearchPoint(); }
		}
	}
	else if (HasReachedDestination(GetCurrentSearchPoint()) || SearchPointTime >= SearchPointTimeout
		|| GetMoveStatus() == EPathFollowingStatus::Idle) { BeginSearchLook(); }
}

void AStealthAIController::SetTurningInPlace(bool bEnabled)
{
	if (!ControlledNPC.IsValid()) { return; }
	ControlledNPC->GetCharacterMovement()->bOrientRotationToMovement = !bEnabled;
	ControlledNPC->GetCharacterMovement()->bUseControllerDesiredRotation = bEnabled;
}

void AStealthAIController::CancelBurst()
{
	if (ShotsRemainingInBurst > 0) { TimeUntilNextShot = FMath::Max(TimeUntilNextShot, BurstPause); }
	ShotsRemainingInBurst = 0;
}

void AStealthAIController::ReactToDamage(AActor* DamageSource)
{
	if (!ControlledNPC.IsValid() || ControlledNPC->IsDead() || !IsValid(DamageSource)) { return; }
	if (!PlayerPawn.IsValid()) { PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0); }
	if (DamageSource != PlayerPawn.Get()) { return; }
	FVector Seen;
	if (CanSeePlayer(Seen))
	{
		DetectionLevel = 1.0f;
		TimeSincePlayerSeen = 0.0f;
		SetAIState(EStealthAIState::Chase, Seen);
	}
	else
	{
		bHasSoundFocus = false;
		SetAIState(bSearchingForPlayer ? EStealthAIState::Search : EStealthAIState::Investigate, DamageSource->GetActorLocation());
	}
}

void AStealthAIController::StopForDeath()
{
	bStoppedForDeath = true;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	CancelBurst();
	bSearchingForPlayer = false;
	DetectionLevel = 0.0f;
	SetActorTickEnabled(false);
}

float AStealthAIController::GetPlayerVisibility(FVector& OutPlayerLocation) const
{
	if (!ControlledNPC.IsValid() || ControlledNPC->IsDead() || !PlayerPawn.IsValid() || !GetWorld()) { return 0.0f; }
	if (const AStealthCharacter* Player = Cast<AStealthCharacter>(PlayerPawn.Get()); Player && Player->GetHealth() <= 0.0f) { return 0.0f; }
	if (const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		Mission && Mission->IsActorProtected(PlayerPawn.Get())) { return 0.0f; }
	const FVector Eye = ControlledNPC->GetActorLocation() + FVector(0, 0, 70);
	const FVector Target = PlayerPawn->GetActorLocation();
	const FVector ToPlayer = Target - Eye;
	if (ToPlayer.SizeSquared() > FMath::Square(SightRadius)) { return 0.0f; }
	const float Dot = FVector::DotProduct(ControlledNPC->GetActorForwardVector().GetSafeNormal2D(), ToPlayer.GetSafeNormal2D());
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(SightAngleDegrees * 0.5f));
	if (Dot < MinDot) { return 0.0f; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthSight), true, ControlledNPC.Get());
	int32 VisibleSamples = 0;
	for (float Height : { -25.0f, 20.0f, 60.0f })
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target + FVector(0, 0, Height), ECC_Visibility, Params)
			|| Hit.GetActor() == PlayerPawn.Get()) { ++VisibleSamples; }
	}
	if (VisibleSamples == 0) { return 0.0f; }
	OutPlayerLocation = Target;
	const float Peripheral = FMath::GetMappedRangeValueClamped(FVector2D(MinDot, 0.95f), FVector2D(0.3f, 1.0f), Dot);
	return VisibleSamples / 3.0f * Peripheral;
}

bool AStealthAIController::CanSeePlayer(FVector& OutPlayerLocation) const
{
	return GetPlayerVisibility(OutPlayerLocation) > 0.0f;
}

bool AStealthAIController::HasReachedDestination(const FVector& Destination, float AcceptanceRadius) const
{
	return ControlledNPC.IsValid()
		&& FVector::DistSquared2D(ControlledNPC->GetActorLocation(), Destination) <= FMath::Square(AcceptanceRadius);
}
