#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthInteractable.h"
#include "StealthDoor.generated.h"

class USoundRayInteractionComponent;
class UStaticMeshComponent;

UCLASS()
class FPS_SHOOTER_API AStealthDoor : public AActor, public IStealthInteractable
{
	GENERATED_BODY()
public:
	AStealthDoor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AStealthCharacter* InteractingCharacter) override;
	UFUNCTION(BlueprintCallable, Category="Door")
	bool SetOpen(bool bOpen, AActor* Initiator);
	UFUNCTION(BlueprintPure, Category="Door")
	bool IsOpen() const { return bIsOpen; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USoundRayInteractionComponent> SoundEmitter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	float OpenAngle = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="0.1"))
	float OpenDuration = 0.65f;
private:
	bool bIsOpen = false;
	float OpenAlpha = 0.0f;
};
