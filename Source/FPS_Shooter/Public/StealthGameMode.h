#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StealthGameMode.generated.h"

UENUM(BlueprintType)
enum class EStealthMissionState : uint8
{
	Playing,
	Won,
	Lost
};

UENUM(BlueprintType)
enum class EStealthMissionStage : uint8
{
	SafeZone,
	Infiltration,
	Evade,
	Extraction
};

UCLASS()
class FPS_SHOOTER_API AStealthGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStealthGameMode();
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartPlay() override;

	UFUNCTION(BlueprintCallable, Category="Mission")
	bool CollectObjective(AActor* Collector);

	UFUNCTION(BlueprintCallable, Category="Mission")
	bool TryCompleteMission(AActor* ExitingActor);

	UFUNCTION(BlueprintCallable, Category="Mission")
	void DefeatPlayer(AActor* DefeatedActor);

	UFUNCTION(BlueprintCallable, Category="Mission")
	void RestartMission();

	UFUNCTION(BlueprintPure, Category="Mission")
	bool IsActorProtected(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category="Mission")
	bool IsPlayerPursued() const;

	UFUNCTION(BlueprintPure, Category="Mission")
	EStealthMissionStage GetMissionStage() const;

	UFUNCTION(BlueprintPure, Category="Mission")
	float GetMissionTime() const { return MissionTime; }

	FString GetObjectiveText() const;
	FString GetThreatText() const;
	float GetDetectionLevel() const;
	FVector GetSafeZoneCenter() const { return SafeZoneCenter; }

	UFUNCTION(BlueprintPure, Category="Mission")
	bool HasObjective() const { return bObjectiveCollected; }

	UFUNCTION(BlueprintPure, Category="Mission")
	EStealthMissionState GetMissionState() const { return MissionState; }

private:
	UPROPERTY(EditDefaultsOnly, Category="Mission", meta=(ClampMin="100.0"))
	float SafeZoneRadius = 350.0f;

	FVector SafeZoneCenter = FVector::ZeroVector;
	bool bSafeZoneInitialized = false;
	bool bInfiltrationStarted = false;
	bool bRestartRequested = false;
	float MissionTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category="Mission")
	EStealthMissionState MissionState = EStealthMissionState::Playing;

	UPROPERTY(VisibleAnywhere, Category="Mission")
	bool bObjectiveCollected = false;

	bool IsMissionPlayer(const AActor* Actor) const;
	void FinishMission(EStealthMissionState NewState);
};
