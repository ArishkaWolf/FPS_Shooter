#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "StealthCharacter.generated.h"

class AFallingSoundSphere;
class UAnimSequence;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UParticleSystem;
class USkeletalMeshComponent;
class USoundBase;
class USoundRayEmitterComponent;

UCLASS()
class FPS_SHOOTER_API AStealthCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStealthCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void AddNoiseMakers(int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetNoiseMakerCount() const { return NoiseMakerCount; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USoundRayEmitterComponent> SoundEmitter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
	float WalkSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
	float SprintSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
	float CrouchSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0.0"))
	float WalkFootstepVolume = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0.0"))
	float SprintFootstepVolume = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0.0"))
	float CrouchFootstepVolume = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="0.0"))
	float InteractionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0"))
	int32 NoiseMakerCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0.0"))
	float NoiseMakerThrowSpeed = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	TSubclassOf<AFallingSoundSphere> NoiseMakerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0"))
	int32 ReserveAmmo = 90;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.01"))
	float FireInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.0"))
	float WeaponDamage = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.0"))
	float WeaponRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UAnimSequence> ArmsIdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UAnimSequence> ArmsRunAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UAnimSequence> ArmsFireAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UParticleSystem> MuzzleFlash;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ThrowAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReloadAction;

	bool bWantsToSprint = false;
	bool bWantsToFire = false;
	float TimeUntilNextShot = 0.0f;
	float FireAnimationTimeRemaining = 0.0f;
	int32 AmmoInMagazine = 0;
	bool bArmsPlayingRun = false;

	void CreateInputMapping();
	void ApplyInputMapping();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void ToggleCrouch();
	void Interact();
	void ThrowNoiseMaker();
	void StartFire();
	void StopFire();
	void FireWeapon();
	void ReloadWeapon();
	void UpdateMovementState();
	void UpdateFirstPersonAnimation(float DeltaSeconds);
};
