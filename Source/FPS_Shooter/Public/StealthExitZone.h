#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StealthExitZone.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UPrimitiveComponent;
class UTextRenderComponent;

UCLASS()
class FPS_SHOOTER_API AStealthExitZone : public AActor
{
	GENERATED_BODY()

public:
	AStealthExitZone();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> TriggerComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> LabelComponent;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> LightComponent;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
