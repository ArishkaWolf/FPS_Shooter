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

UCLASS()
class FPS_SHOOTER_API AStealthGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStealthGameMode();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Mission")
	bool CollectObjective(AActor* Collector);

	UFUNCTION(BlueprintCallable, Category="Mission")
	bool TryCompleteMission(AActor* ExitingActor);

	UFUNCTION(BlueprintCallable, Category="Mission")
	void DefeatPlayer(AActor* DefeatedActor);

	UFUNCTION(BlueprintPure, Category="Mission")
	bool HasObjective() const { return bObjectiveCollected; }

	UFUNCTION(BlueprintPure, Category="Mission")
	EStealthMissionState GetMissionState() const { return MissionState; }

private:
	UPROPERTY(VisibleAnywhere, Category="Mission")
	EStealthMissionState MissionState = EStealthMissionState::Playing;

	UPROPERTY(VisibleAnywhere, Category="Mission")
	bool bObjectiveCollected = false;

	void FinishMission(EStealthMissionState NewState);
};
