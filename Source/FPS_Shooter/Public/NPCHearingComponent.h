#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoundRayEvent.h"
#include "NPCHearingComponent.generated.h"

// The ray emitter supplies an attenuated strength and the point where the ray was heard.
// Keep that contract: this is evidence, not omniscient knowledge of the sound source.
struct FNPCHearingStimulus
{
	FVector Location = FVector::ZeroVector;
	float Strength = 0.0f;
	float Age = 0.0f;
	int32 Id = 0;
	FSoundRayEvent Event;
	float MemoryDuration = 0.0f;
};

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
	bool bDrawDebug = false; // Legacy flag; visualization is controlled by SoundRays.Debug.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	float DebugDrawDuration = 0.25f;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	FVector LastHeardPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	float CurrentSoundStrength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	float LastReceivedSoundStrength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	int32 HearingEventId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Hearing")
	FSoundRayEvent LastHeardEvent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing")
	TMap<ESoundEventType, float> TypePriorityWeights;

	UFUNCTION(BlueprintCallable, Category="Hearing")
	void HearSoundAtLocation(const FVector& SoundLocation, float Strength);

	UFUNCTION(BlueprintCallable, Category="Hearing")
	void HearSoundEvent(const FSoundRayEvent& Event);

	UFUNCTION(BlueprintPure, Category="Hearing")
	bool HasHeardSound() const { return CurrentSoundStrength > KINDA_SMALL_NUMBER; }

	bool GetBestSound(FNPCHearingStimulus& OutSound) const;
	int32 GetRememberedSoundCount() const { return SoundMemory.Num(); }
	float GetSoundPriority(const FNPCHearingStimulus& Sound) const;
	void ClearSoundMemory();

	static const TArray<TWeakObjectPtr<UNPCHearingComponent>>& GetRegisteredComponents();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	static TArray<TWeakObjectPtr<UNPCHearingComponent>> RegisteredComponents;

	// Bounded, coalesced memory prevents reflected rays from flooding the AI.
	TArray<FNPCHearingStimulus> SoundMemory;
	float GetMemoryDuration() const;
	float GetMemoryDuration(const FNPCHearingStimulus& Sound) const;
	void StoreSound(FNPCHearingStimulus Sound);
	void RefreshBestSound();
};
