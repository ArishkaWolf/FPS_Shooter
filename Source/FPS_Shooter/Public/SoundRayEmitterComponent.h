#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
	float DebugDrawDuration = 1.0f;

	UFUNCTION(BlueprintCallable, Category="Sound Rays")
	void EmitFootstepSound();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float TimeUntilNextFootstep = 0.0f;

	bool ShouldAutoEmitFootstep() const;
	void TraceSoundRay(const FVector& RayStart, const FVector& Direction, float StartVolume, int32 ReflectionIndex) const;
	void NotifyListenersAlongRay(const FVector& RayStart, const FVector& RayEnd, float StartVolume, float EndVolume) const;
};
