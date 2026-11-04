#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "StealthObjective.generated.h"

class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundRayInteractionComponent;

UCLASS()
class FPS_SHOOTER_API AStealthObjective : public AActor, public IStealthInteractable
{
	GENERATED_BODY()

public:
	AStealthObjective();
	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USoundRayInteractionComponent> SoundEmitter;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> LabelComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> LightComponent;
};
