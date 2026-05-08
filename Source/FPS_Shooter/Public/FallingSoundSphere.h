#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallingSoundSphere.generated.h"

class USoundRayEmitterComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class FPS_SHOOTER_API AFallingSoundSphere : public AActor
{
	GENERATED_BODY()

public:
	AFallingSoundSphere();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USoundRayEmitterComponent> SoundEmitter;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

private:
	bool bEmittedSound = false;
};
