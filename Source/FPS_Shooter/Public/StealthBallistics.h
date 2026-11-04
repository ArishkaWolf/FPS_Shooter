#pragma once

#include "CoreMinimal.h"
#include "StealthBallistics.generated.h"

class ACharacter;
class UMaterialInterface;
class UParticleSystem;
class USkeletalMeshComponent;
class USoundBase;

USTRUCT()
struct FStealthImpactEffects
{
	GENERATED_BODY()

	void LoadDefaults();
	void Play(const AActor& Shooter, const FHitResult& Hit) const;

private:
	UPROPERTY()
	TObjectPtr<UParticleSystem> SurfaceImpact;
	UPROPERTY()
	TObjectPtr<UParticleSystem> BodyImpact;
	UPROPERTY()
	TObjectPtr<USoundBase> SurfaceSound;
	UPROPERTY()
	TObjectPtr<USoundBase> BodySound;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BulletMark;
};

namespace StealthBallistics
{
	void ConfigureMannequinHitMesh(USkeletalMeshComponent& Mesh);
	float GetDamageMultiplier(FName Bone);
	void Fire(ACharacter& Shooter, const FVector& ClearanceStart, const FVector& Muzzle,
		const FVector& AimOrigin, const FVector& Direction, float Range, float BaseDamage,
		const FStealthImpactEffects& Effects);
}
