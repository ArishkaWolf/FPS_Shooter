#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPCHearingComponent.generated.h"

UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class FPS_SHOOTER_API UNPCHearingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPCHearingComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing")
	float ForgetSpeed = 1.0f / 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing", meta=(ClampMin="0.1"))
	float ForgetDurationSeconds = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing")
	float HearingRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	float DebugDrawDuration = 0.25f;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	FVector LastHeardPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	float CurrentSoundStrength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	float LastReceivedSoundStrength = 0.0f;

	UFUNCTION(BlueprintCallable, Category="Hearing")
	void HearSoundAtLocation(const FVector& SoundLocation, float Strength);

	UFUNCTION(BlueprintPure, Category="Hearing")
	bool HasHeardSound() const { return CurrentSoundStrength > KINDA_SMALL_NUMBER; }

	static const TArray<TWeakObjectPtr<UNPCHearingComponent>>& GetRegisteredComponents();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	static TArray<TWeakObjectPtr<UNPCHearingComponent>> RegisteredComponents;

	float RememberedSoundStrength = 0.0f;
	float TimeSinceLastHeard = 0.0f;
};
