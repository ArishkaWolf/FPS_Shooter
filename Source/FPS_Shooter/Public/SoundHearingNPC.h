#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SoundHearingNPC.generated.h"

class UNPCHearingComponent;
class UParticleSystem;
class USkeletalMeshComponent;
class USoundBase;
class UTextRenderComponent;

UCLASS()
class FPS_SHOOTER_API ASoundHearingNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ASoundHearingNPC();
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	void FireWeaponAt(AActor* TargetActor);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UParticleSystem> MuzzleFlash;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	float CurrentHealth = 100.0f;
};
