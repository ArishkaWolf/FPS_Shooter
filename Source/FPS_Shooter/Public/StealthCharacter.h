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

UENUM(BlueprintType)
enum class EStealthArmsState : uint8
{
	Idle,
	Run,
	Crouch,
	Fire,
	Reload,
	Aim
};

UCLASS()
class FPS_SHOOTER_API AStealthCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStealthCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void AddNoiseMakers(int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetNoiseMakerCount() const { return NoiseMakerCount; }

	UFUNCTION(BlueprintPure, Category="Combat")
	float GetHealth() const { return CurrentHealth; }
	void StopMissionActions();
	bool CanPerformMissionAction() const;

	UFUNCTION(BlueprintPure, Category="Combat")
	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }

	UFUNCTION(BlueprintPure, Category="Combat")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category="First Person")
	EStealthArmsState GetArmsState() const { return ArmsState; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	float GetReloadProgress() const;

	float GetAimAlpha() const { return AimAlpha; }
	float GetShotAnimationAge() const;
	float GetLastLandingStrength() const { return LastLandingStrength; }
	UAnimSequence* GetArmsIdleAnimation() const { return ArmsIdleAnimation; }
	UAnimSequence* GetArmsRunAnimation() const { return ArmsRunAnimation; }
	UAnimSequence* GetArmsFireAnimation() const { return ArmsFireAnimation; }
	FTransform GetWeaponToHandTransform() const { return WeaponToHandTransform; }
	FVector GetMagazineGripLocation() const { return MagazineGripLocation; }
	FVector GetForegripLocation() const { return ForegripLocation; }
	FVector GetIronSightLocation() const { return IronSightLocation; }

protected:
	virtual void BeginPlay() override;
	virtual bool CanJumpInternal_Implementation() const override;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="10.0", ClampMax="120.0"))
	float HipFieldOfView = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="10.0", ClampMax="120.0"))
	float AimFieldOfView = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Aim", meta=(ClampMin="1.0"))
	float AimInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Aim", meta=(ClampMin="1.0"))
	float AimWalkSpeed = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Aim", meta=(ClampMin="1.0"))
	float AimCrouchSpeed = 125.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Aim")
	FVector HipArmsLocation = FVector(-30.0f, 0.0f, -150.0f);

	// Model-space point on top of the receiver, aligned to the camera when aiming.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Aim")
	FVector IronSightLocation = FVector(0.0f, -10.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Aim", meta=(ClampMin="15.0"))
	float AimSightDistance = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FTransform WeaponGripTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FVector MagazineGripLocation = FVector(0.0f, 12.0f, -7.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FVector ForegripLocation = FVector(0.0f, 32.0f, 3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.2"))
	float ReloadDuration = 2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UAnimSequence> WeaponReloadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> LandingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0.0"))
	float MinLandingSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound", meta=(ClampMin="1.0"))
	float HardLandingSpeed = 1000.0f;

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

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimAction;

	bool bWantsToSprint = false;
	bool bWantsToFire = false;
	bool bWantsToAim = false;
	bool bIsAiming = false;
	bool bIsReloading = false;
	EStealthArmsState ArmsState = EStealthArmsState::Idle;
	float ReloadElapsed = 0.0f;
	float AimAlpha = 0.0f;
	float CrouchAlpha = 0.0f;
	float ReloadPoseTilt = 0.0f;
	float PeakFallSpeed = 0.0f;
	float LastLandingStrength = 0.0f;
	FTransform AimArmsTransform;
	FTransform WeaponToHandTransform;
	float TimeUntilNextShot = 0.0f;
	float FireAnimationTimeRemaining = 0.0f;
	int32 AmmoInMagazine = 0;
	float CurrentHealth = 100.0f;

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
	void StartAim();
	void StopAim();
	void CancelReload();
	void UpdateReload(float DeltaSeconds);
	void CalibrateWeapon();
	void UpdateFirstPersonPose(float DeltaSeconds);
	void UpdateMovementState();
	void UpdateFirstPersonAnimation(float DeltaSeconds);
};
