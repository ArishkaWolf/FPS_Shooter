#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoundRayEvent.h"
#include "SoundRayEmitterComponent.generated.h"

UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class FPS_SHOOTER_API USoundRayEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USoundRayEmitterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays")
	bool bDirectionalSound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="0.0"))
	float Volume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="1"))
	int32 RayCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="0.0"))
	float AttenuationPerMeter = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="0.0"))
	float WallAbsorption = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays")
	float RayPitchDegrees = -8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="0.0"))
	float ForgetSpeed = 1.0f / 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="0"))
	int32 MaxReflections = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays", meta=(ClampMin="1.0"))
	float MaxDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps")
	bool bAutoEmitFootsteps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps", meta=(ClampMin="0.05"))
	float FootstepInterval = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps", meta=(ClampMin="0.0"))
	float MinSpeedForFootsteps = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	bool bDrawDebug = false; // Legacy asset compatibility; use SoundRays.Debug at runtime.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	float DebugDrawDuration = 1.0f;

	UFUNCTION(BlueprintCallable, Category="Sound Rays")
	void EmitFootstepSound();

	// Negative overrides use this component's existing Volume and MaxDistance.
	UFUNCTION(BlueprintCallable, Category="Sound Rays")
	void EmitSoundEvent(ESoundEventType Type, float VolumeOverride = -1.0f, float DistanceOverride = -1.0f, AActor* SourceOwner = nullptr);

	UFUNCTION(BlueprintCallable, Category="Sound Rays")
	void EmitSoundEventAtLocation(ESoundEventType Type, FVector Location, float VolumeOverride = -1.0f,
		float DistanceOverride = -1.0f, AActor* SourceOwner = nullptr);

	UFUNCTION(BlueprintPure, Category="Sound Rays")
	FSoundRayEvent GetLastEmittedEvent() const { return LastEmittedEvent; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float TimeUntilNextFootstep = 0.0f;
	UPROPERTY(Transient)
	FSoundRayEvent LastEmittedEvent;

	bool ShouldAutoEmitFootstep() const;
	void TraceSoundRay(const FVector& RayStart, const FVector& Direction, float StartVolume, int32 ReflectionIndex,
		const FSoundRayEvent& Event, float RayDistance) const;
	void NotifyListenersAlongRay(const FVector& RayStart, const FVector& RayEnd, float StartVolume, float EndVolume,
		const FSoundRayEvent& Event, int32 ReflectionIndex) const;
};
