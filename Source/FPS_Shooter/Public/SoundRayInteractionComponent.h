#pragma once

#include "CoreMinimal.h"
#include "SoundRayEmitterComponent.h"
#include "SoundRayInteractionComponent.generated.h"

class USoundBase;

// Add to any existing door/interactive actor; notify only after an action succeeds.
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class FPS_SHOOTER_API USoundRayInteractionComponent : public USoundRayEmitterComponent
{
	GENERATED_BODY()
public:
	USoundRayInteractionComponent();
	UFUNCTION(BlueprintCallable, Category="Sound Rays|Interaction")
	void NotifyInteraction(AActor* Initiator);
	UFUNCTION(BlueprintCallable, Category="Sound Rays|Interaction")
	void NotifyDoor(bool bOpening, AActor* Initiator);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction", meta=(ClampMin="0"))
	float InteractionVolume = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction", meta=(ClampMin="1"))
	float InteractionDistance = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction", meta=(ClampMin="0"))
	float DoorVolume = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction", meta=(ClampMin="1"))
	float DoorDistance = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction")
	FVector SoundOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction")
	TObjectPtr<USoundBase> InteractionAudio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction")
	TObjectPtr<USoundBase> DoorOpenAudio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Rays|Interaction")
	TObjectPtr<USoundBase> DoorCloseAudio;
private:
	void EmitAction(ESoundEventType Type, float Strength, float Distance, AActor* Initiator, USoundBase* Audio);
};
