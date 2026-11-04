#include "StealthBallistics.h"

#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

void StealthBallistics::ConfigureMannequinHitMesh(USkeletalMeshComponent& Mesh)
{
	static ConstructorHelpers::FObjectFinder<UPhysicsAsset> PhysicsAsset(
		TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
	if (PhysicsAsset.Succeeded())
	{
		Mesh.SetPhysicsAsset(PhysicsAsset.Object);
	}
	Mesh.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh.SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// The owner-hidden player mesh must follow the pose for hits as well.
	Mesh.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

float StealthBallistics::GetDamageMultiplier(FName Bone)
{
	if (Bone == TEXT("head")) { return 2.5f; }
	const FString Name = Bone.ToString();
	if (Name.StartsWith(TEXT("upperarm_")) || Name.StartsWith(TEXT("lowerarm_"))
		|| Name.StartsWith(TEXT("hand_")))
	{
		return 0.75f;
	}
	if (Name.StartsWith(TEXT("thigh_")) || Name.StartsWith(TEXT("calf_"))
		|| Name.StartsWith(TEXT("foot_")))
	{
		return 0.65f;
	}
	return 1.0f;
}

void FStealthImpactEffects::LoadDefaults()
{
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Dust(
		TEXT("/Game/MilitaryWeapSilver/FX/P_Impact_Stone_Small_01.P_Impact_Stone_Small_01"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Sparks(
		TEXT("/Game/MilitaryWeapSilver/FX/P_Impact_Metal_Small_01.P_Impact_Metal_Small_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Surface(
		TEXT("/Game/MilitaryWeapSilver/Sound/Rifle/Cues/Rifle_ImpactSurface_Cue.Rifle_ImpactSurface_Cue"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Body(
		TEXT("/Game/MilitaryWeapSilver/Sound/Rifle/Cues/Rifle_ImpactBody_Cue.Rifle_ImpactBody_Cue"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mark(
		TEXT("/Game/Combat/M_BulletImpact.M_BulletImpact"));
	SurfaceImpact = Dust.Object;
	BodyImpact = Sparks.Object;
	SurfaceSound = Surface.Object;
	BodySound = Body.Object;
	BulletMark = Mark.Object;
}

void FStealthImpactEffects::Play(const AActor& Shooter, const FHitResult& Hit) const
{
	const bool bHitCharacter = Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>();
	const FRotator SurfaceRotation = Hit.ImpactNormal.Rotation();
	if (UParticleSystem* Particle = bHitCharacter ? BodyImpact : SurfaceImpact)
	{
		UGameplayStatics::SpawnEmitterAtLocation(Shooter.GetWorld(), Particle, Hit.ImpactPoint, SurfaceRotation);
	}
	if (USoundBase* Sound = bHitCharacter ? BodySound : SurfaceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(&Shooter, Sound, Hit.ImpactPoint, 0.6f);
	}
	if (!bHitCharacter && BulletMark && Hit.GetComponent())
	{
		if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAttached(BulletMark, FVector(3.0f, 4.5f, 4.5f),
			Hit.GetComponent(), NAME_None, Hit.ImpactPoint, SurfaceRotation, EAttachLocation::KeepWorldPosition, 10.0f))
		{
			Decal->SetWorldScale3D(FVector::OneVector);
		}
	}
}

void StealthBallistics::Fire(ACharacter& Shooter, const FVector& ClearanceStart, const FVector& Muzzle,
	const FVector& AimOrigin, const FVector& Direction, float Range, float BaseDamage,
	const FStealthImpactEffects& Effects)
{
	UWorld* World = Shooter.GetWorld();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StealthBallistics), true, &Shooter);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, ClearanceStart, Muzzle, ECC_Visibility, Query))
	{
		const FVector RangeEnd = AimOrigin + Direction * Range;
		FHitResult AimHit;
		World->LineTraceSingleByChannel(AimHit, AimOrigin, RangeEnd, ECC_Visibility, Query);
		Hit = AimHit;
		if (!AimOrigin.Equals(Muzzle))
		{
			// Keep the sights on target while requiring a clear path from the actual barrel.
			const FVector Endpoint = AimHit.bBlockingHit ? AimHit.ImpactPoint + Direction * 2.0f : RangeEnd;
			FHitResult MuzzleHit;
			if (FVector::DotProduct(Endpoint - Muzzle, Direction) > 0.0f
				&& World->LineTraceSingleByChannel(MuzzleHit, Muzzle, Endpoint, ECC_Visibility, Query))
			{
				Hit = MuzzleHit;
			}
		}
	}

	if (Hit.bBlockingHit)
	{
		Effects.Play(Shooter, Hit);
		if (AActor* Target = Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(Target, BaseDamage * GetDamageMultiplier(Hit.BoneName),
				Direction, Hit, Shooter.GetController(), &Shooter, UDamageType::StaticClass());
		}
	}
}
