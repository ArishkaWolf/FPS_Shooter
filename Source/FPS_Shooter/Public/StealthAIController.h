#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "StealthAIController.generated.h"

class AStealthPatrolPoint;
class ASoundHearingNPC;

UENUM(BlueprintType)
enum class EStealthAIState : uint8
{
	Patrol,
	Investigate,
	Search,
	Chase
};

UCLASS()
class FPS_SHOOTER_API AStealthAIController : public AAIController
{
	GENERATED_BODY()

public:
	AStealthAIController();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Stealth AI")
	EStealthAIState GetAIState() const { return AIState; }

	UFUNCTION(BlueprintPure, Category="Stealth AI")
	float GetDetectionLevel() const { return DetectionLevel; }

	UFUNCTION(BlueprintPure, Category="Stealth AI")
	bool HasActivePursuit() const { return AIState == EStealthAIState::Chase || bSearchingForPlayer; }

	UFUNCTION(BlueprintPure, Category="Stealth AI")
	FText GetStateDisplayName() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth AI")
	EStealthAIState AIState = EStealthAIState::Patrol;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth AI", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DetectionLevel = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight", meta=(ClampMin="0.0"))
	float SightRadius = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight", meta=(ClampMin="1.0", ClampMax="179.0"))
	float SightAngleDegrees = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight", meta=(ClampMin="0.0"))
	float DetectionRate = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight", meta=(ClampMin="0.0"))
	float DetectionDecayRate = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sight", meta=(ClampMin="0.0"))
	float LoseSightDelay = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hearing", meta=(ClampMin="0.0"))
	float InvestigateSoundThreshold = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Search", meta=(ClampMin="0.0"))
	float SearchDuration = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Search", meta=(ClampMin="0.0"))
	float SearchRadius = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float FireRange = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.1"))
	float FireInterval = 0.8f;

private:
	TWeakObjectPtr<ASoundHearingNPC> ControlledNPC;
	TWeakObjectPtr<APawn> PlayerPawn;
	TArray<TWeakObjectPtr<AStealthPatrolPoint>> PatrolPoints;

	int32 PatrolIndex = INDEX_NONE;
	int32 LastProcessedHearingEventId = 0;
	float StateTime = 0.0f;
	float WaitTimeRemaining = 0.0f;
	float TimeSincePlayerSeen = BIG_NUMBER;
	float TimeUntilNextShot = 0.0f;
	bool bSearchingForPlayer = false;
	FVector LastKnownPlayerLocation = FVector::ZeroVector;
	FVector SearchOrigin = FVector::ZeroVector;

	void GatherPatrolPoints();
	void UpdateSight(float DeltaSeconds);
	void UpdateHearing();
	void UpdateState(float DeltaSeconds);
	void SetAIState(EStealthAIState NewState, const FVector& FocusLocation = FVector::ZeroVector);
	void MoveToCurrentPatrolPoint();
	void AdvancePatrol();
	void MoveToRandomSearchPoint();
	bool CanSeePlayer(FVector& OutPlayerLocation) const;
	bool HasReachedDestination(const FVector& Destination, float AcceptanceRadius = 110.0f) const;
};
