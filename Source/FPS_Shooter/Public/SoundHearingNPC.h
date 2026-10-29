#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SoundHearingNPC.generated.h"

class UNPCHearingComponent;
class UParticleSystem;
class USkeletalMeshComponent;
class USoundBase;
class UTextRenderComponent;
class UAnimSequence;

UCLASS()
class FPS_SHOOTER_API ASoundHearingNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ASoundHearingNPC();
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	bool FireWeaponAt(AActor* TargetActor);
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsDead() const { return bDead; }
	UFUNCTION(BlueprintPure, Category="Combat")
	float GetHealth() const { return CurrentHealth; }
	int32 GetShotsFired() const { return ShotsFired; }
	FVector GetLastShotDirection() const { return LastShotDirection; }
	FVector GetLastShotAimDirection() const { return LastShotAimDirection; }
	float GetFireAnimationAge() const { return FireAnimationAge; }
	float GetHitAnimationAge() const { return HitAnimationAge; }
	float GetDeathAnimationAge() const { return DeathAnimationAge; }
	UAnimSequence* GetIdleAnimation() const { return IdleAnimation; }
	UAnimSequence* GetWalkAnimation() const { return WalkAnimation; }
	UAnimSequence* GetRunAnimation() const { return RunAnimation; }
	UAnimSequence* GetFireAnimation() const { return FireAnimation; }
	UAnimSequence* GetHitAnimation() const { return HitAnimation; }
	UAnimSequence* GetDeathAnimation() const { return DeathAnimation; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPCHearingComponent> HearingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTextRenderComponent> SoundLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing")
	FText NPCLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol")
	FName PatrolRouteName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float PatrolSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float InvestigateSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float ChaseSpeed = 430.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0"))
	float FireDamage = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0", ClampMax="12.0"))
	float FireSpreadDegrees = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="1.0"))
	float WeaponRange = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UParticleSystem> MuzzleFlash;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TObjectPtr<UAnimSequence> RunAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TObjectPtr<UAnimSequence> FireAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TObjectPtr<UAnimSequence> HitAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	TObjectPtr<UAnimSequence> DeathAnimation;

private:
	float CurrentHealth = 100.0f;
	bool bDead = false;
	int32 ShotsFired = 0;
	float FireAnimationAge = BIG_NUMBER;
	float HitAnimationAge = BIG_NUMBER;
	float DeathAnimationAge = 0.0f;
	FVector LastShotDirection = FVector::ForwardVector;
	FVector LastShotAimDirection = FVector::ForwardVector;
	void Die();
};
