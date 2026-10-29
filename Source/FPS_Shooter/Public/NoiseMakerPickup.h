#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "NoiseMakerPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USoundRayInteractionComponent;

UCLASS()
class FPS_SHOOTER_API ANoiseMakerPickup : public AActor, public IStealthInteractable
{
	GENERATED_BODY()

public:
	ANoiseMakerPickup();

	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USoundRayInteractionComponent> SoundEmitter;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup", meta=(ClampMin="1"))
	int32 Amount = 1;
};
