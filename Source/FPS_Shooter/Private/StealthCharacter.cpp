#include "StealthCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "FallingSoundSphere.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "SoundRayEmitterComponent.h"
#include "StealthGameMode.h"
#include "StealthArmsAnimInstance.h"
#include "StealthInteractable.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void AddNegatedModifier(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}

	void AddSwizzleModifier(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
}

AStealthCharacter::AStealthCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	Movement->JumpZVelocity = 0.0f;
	Movement->GetNavAgentPropertiesRef().bCanJump = false;
	JumpMaxCount = 0;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(60.0f);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(GetCapsuleComponent());
	FollowCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	FollowCamera->bUsePawnControlRotation = true;

	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FollowCamera);
	FirstPersonArms->SetRelativeLocation(HipArmsLocation);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->CastShadow = false;
	FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(FirstPersonArms, TEXT("GripPoint"));
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->CastShadow = false;

	SoundEmitter = CreateDefaultSubobject<USoundRayEmitterComponent>(TEXT("SoundEmitter"));
	SoundEmitter->Volume = WalkFootstepVolume;
	SoundEmitter->RayCount = 16;
	SoundEmitter->AttenuationPerMeter = 0.08f;
	SoundEmitter->WallAbsorption = 0.65f;
	SoundEmitter->MaxDistance = 1800.0f;
	SoundEmitter->MaxReflections = 2;
	SoundEmitter->FootstepInterval = 0.48f;
	SoundEmitter->MinSpeedForFootsteps = 25.0f;
	SoundEmitter->bDrawDebug = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		GetMesh()->SetOwnerNoSee(true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnimBlueprint(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"));
	if (MannyAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(MannyAnimBlueprint.Class);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmsMesh(TEXT("/Game/FirstPersonArms/Character/Mesh/SK_Mannequin_Arms.SK_Mannequin_Arms"));
	if (ArmsMesh.Succeeded())
	{
		FirstPersonArms->SetSkeletalMesh(ArmsMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GunMesh(TEXT("/Game/MilitaryWeapSilver/Weapons/Assault_Rifle_A.Assault_Rifle_A"));
	if (GunMesh.Succeeded())
	{
		WeaponMesh->SetSkeletalMesh(GunMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> RifleIdle(TEXT("/Game/FirstPersonArms/Animations/FP_Rifle_Idle.FP_Rifle_Idle"));
	ArmsIdleAnimation = RifleIdle.Object;
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RifleRun(TEXT("/Game/FirstPersonArms/Animations/FP_Rifle_Run.FP_Rifle_Run"));
	ArmsRunAnimation = RifleRun.Object;
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RifleFire(TEXT("/Game/FirstPersonArms/Animations/FP_Rifle_Fire.FP_Rifle_Fire"));
	ArmsFireAnimation = RifleFire.Object;
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RifleReload(TEXT("/Game/MilitaryWeapSilver/Weapons/Animations/Reload_Rifle_Hip_W.Reload_Rifle_Hip_W"));
	WeaponReloadAnimation = RifleReload.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> RifleReloadSound(TEXT("/Game/MilitaryWeapSilver/Sound/Rifle/Cues/Rifle_Reload_Cue.Rifle_Reload_Cue"));
	ReloadSound = RifleReloadSound.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> LandingImpact(TEXT("/Game/Audio/Stealth/Landing_Impact.Landing_Impact"));
	LandingSound = LandingImpact.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> RifleFireSound(TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));
	FireSound = RifleFireSound.Object;
	static ConstructorHelpers::FObjectFinder<UParticleSystem> RifleMuzzleFlash(TEXT("/Game/MilitaryWeapSilver/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"));
	MuzzleFlash = RifleMuzzleFlash.Object;

	NoiseMakerClass = AFallingSoundSphere::StaticClass();
	AmmoInMagazine = MagazineSize;
	CurrentHealth = MaxHealth;
	CreateInputMapping();
}

void AStealthCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(1.0f, MaxHealth);
	AmmoInMagazine = FMath::Max(1, MagazineSize);
	ApplyInputMapping();
	FirstPersonArms->SetRelativeLocation(HipArmsLocation);
	FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	FollowCamera->SetFieldOfView(HipFieldOfView);
	if (ArmsIdleAnimation)
	{
		FirstPersonArms->PlayAnimation(ArmsIdleAnimation, true);
		FirstPersonArms->TickAnimation(0.0f, false);
		FirstPersonArms->RefreshBoneTransforms();
	}
	CalibrateWeapon();
	FirstPersonArms->SetAnimInstanceClass(UStealthArmsAnimInstance::StaticClass());
	FirstPersonArms->AddTickPrerequisiteActor(this);
}

void AStealthCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ApplyInputMapping();
}

void AStealthCharacter::ApplyInputMapping()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->RemoveMappingContext(InputMappingContext);
				InputSubsystem->AddMappingContext(InputMappingContext, 0);
			}
		}
	}
}

void AStealthCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		GameMode && GameMode->GetMissionState() != EStealthMissionState::Playing)
	{
		return;
	}
	if (GetCharacterMovement()->IsFalling())
	{
		PeakFallSpeed = FMath::Max(PeakFallSpeed, -GetVelocity().Z);
	}
	UpdateReload(DeltaSeconds);
	bIsAiming = bWantsToAim && !bIsReloading && CanPerformMissionAction();
	UpdateMovementState();
	TimeUntilNextShot = FMath::Max(0.0f, TimeUntilNextShot - DeltaSeconds);
	FireAnimationTimeRemaining = FMath::Max(0.0f, FireAnimationTimeRemaining - DeltaSeconds);
	if (bWantsToFire && TimeUntilNextShot <= 0.0f)
	{
		FireWeapon();
	}
	UpdateFirstPersonAnimation(DeltaSeconds);
	UpdateFirstPersonPose(DeltaSeconds);
}

void AStealthCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStealthCharacter::Move);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AStealthCharacter::Look);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AStealthCharacter::StartSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AStealthCharacter::StopSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AStealthCharacter::StopSprint);
	EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AStealthCharacter::ToggleCrouch);
	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AStealthCharacter::Interact);
	EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Started, this, &AStealthCharacter::ThrowNoiseMaker);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AStealthCharacter::StartFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AStealthCharacter::StopFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Canceled, this, &AStealthCharacter::StopFire);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AStealthCharacter::ReloadWeapon);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AStealthCharacter::StartAim);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AStealthCharacter::StopAim);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Canceled, this, &AStealthCharacter::StopAim);
}

void AStealthCharacter::AddNoiseMakers(int32 Amount)
{
	NoiseMakerCount = FMath::Max(0, NoiseMakerCount + Amount);
}

void AStealthCharacter::CreateInputMapping()
{
	InputMappingContext = NewObject<UInputMappingContext>(this, TEXT("StealthInputMapping"));

	MoveAction = NewObject<UInputAction>(this, TEXT("MoveAction"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	FEnhancedActionKeyMapping& Forward = InputMappingContext->MapKey(MoveAction, EKeys::W);
	AddSwizzleModifier(InputMappingContext, Forward);
	FEnhancedActionKeyMapping& Backward = InputMappingContext->MapKey(MoveAction, EKeys::S);
	AddSwizzleModifier(InputMappingContext, Backward);
	AddNegatedModifier(InputMappingContext, Backward);
	InputMappingContext->MapKey(MoveAction, EKeys::D);
	FEnhancedActionKeyMapping& Left = InputMappingContext->MapKey(MoveAction, EKeys::A);
	AddNegatedModifier(InputMappingContext, Left);
	InputMappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	LookAction = NewObject<UInputAction>(this, TEXT("LookAction"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	InputMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	InputMappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);

	SprintAction = NewObject<UInputAction>(this, TEXT("SprintAction"));
	SprintAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	InputMappingContext->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);

	CrouchAction = NewObject<UInputAction>(this, TEXT("CrouchAction"));
	CrouchAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	InputMappingContext->MapKey(CrouchAction, EKeys::C);
	InputMappingContext->MapKey(CrouchAction, EKeys::Gamepad_FaceButton_Right);

	InteractAction = NewObject<UInputAction>(this, TEXT("InteractAction"));
	InteractAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(InteractAction, EKeys::E);
	InputMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);

	ThrowAction = NewObject<UInputAction>(this, TEXT("ThrowAction"));
	ThrowAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(ThrowAction, EKeys::Q);
	InputMappingContext->MapKey(ThrowAction, EKeys::Gamepad_RightShoulder);

	FireAction = NewObject<UInputAction>(this, TEXT("FireAction"));
	FireAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	InputMappingContext->MapKey(FireAction, EKeys::Gamepad_RightTrigger);

	ReloadAction = NewObject<UInputAction>(this, TEXT("ReloadAction"));
	ReloadAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(ReloadAction, EKeys::R);
	InputMappingContext->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);

	AimAction = NewObject<UInputAction>(this, TEXT("AimAction"));
	AimAction->ValueType = EInputActionValueType::Boolean;
	InputMappingContext->MapKey(AimAction, EKeys::RightMouseButton);
	InputMappingContext->MapKey(AimAction, EKeys::Gamepad_LeftTrigger);
}

void AStealthCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementInput = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementInput.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementInput.X);
}

void AStealthCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(-LookInput.Y);
}

void AStealthCharacter::StartSprint()
{
	bWantsToSprint = true;
}

void AStealthCharacter::StopSprint()
{
	bWantsToSprint = false;
}

void AStealthCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AStealthCharacter::Interact()
{
	if (!CanPerformMissionAction())
	{
		return;
	}
	CancelReload();
	FHitResult Hit;
	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector End = Start + FollowCamera->GetForwardVector() * InteractionDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StealthInteraction), false, this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams)
		&& Hit.GetActor()
		&& Hit.GetActor()->Implements<UStealthInteractable>())
	{
		IStealthInteractable::Execute_Interact(Hit.GetActor(), this);
	}
}

void AStealthCharacter::ThrowNoiseMaker()
{
	if (!CanPerformMissionAction() || NoiseMakerCount <= 0 || !NoiseMakerClass)
	{
		return;
	}
	CancelReload();

	const FVector Direction = FollowCamera->GetForwardVector();
	const FVector SpawnLocation = FollowCamera->GetComponentLocation() + Direction * 120.0f;
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AFallingSoundSphere* NoiseMaker = GetWorld()->SpawnActor<AFallingSoundSphere>(NoiseMakerClass, SpawnLocation, Direction.Rotation(), SpawnParams))
	{
		NoiseMaker->SoundEmitter->bDrawDebug = false;
		NoiseMaker->SoundEmitter->DebugDrawDuration = 0.0f;
		NoiseMaker->CollisionComponent->SetPhysicsLinearVelocity(Direction * NoiseMakerThrowSpeed + GetVelocity());
		--NoiseMakerCount;
	}
}

void AStealthCharacter::StartFire()
{
	if (!CanPerformMissionAction() || bIsReloading)
	{
		return;
	}
	bWantsToFire = true;
	if (TimeUntilNextShot <= 0.0f)
	{
		FireWeapon();
	}
}

void AStealthCharacter::StopFire()
{
	bWantsToFire = false;
}

void AStealthCharacter::FireWeapon()
{
	if (!CanPerformMissionAction() || bIsReloading || AmmoInMagazine <= 0)
	{
		bWantsToFire = false;
		return;
	}

	--AmmoInMagazine;
	TimeUntilNextShot = FireInterval;

	if (ArmsFireAnimation)
	{
		FireAnimationTimeRemaining = ArmsFireAnimation->GetPlayLength();
	}
	ArmsState = EStealthArmsState::Fire;
	if (MuzzleFlash)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh, TEXT("MuzzleFlash"));
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, FollowCamera->GetComponentLocation());
	}

	const float PreviousVolume = SoundEmitter->Volume;
	SoundEmitter->Volume = 1.6f;
	SoundEmitter->EmitFootstepSound();
	SoundEmitter->Volume = PreviousVolume;

	const FVector TraceStart = FollowCamera->GetComponentLocation();
	const FVector ShotDirection = FollowCamera->GetForwardVector();
	const FVector TraceEnd = TraceStart + ShotDirection * WeaponRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponFire), true, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) && Hit.GetActor())
	{
		const FString BoneName = Hit.BoneName.ToString().ToLower();
		const float HitMultiplier = BoneName.Contains(TEXT("head")) ? 3.0f
			: (BoneName.Contains(TEXT("spine")) || BoneName.Contains(TEXT("neck"))) ? 1.25f : 1.0f;
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), WeaponDamage * HitMultiplier, ShotDirection, Hit,
			GetController(), this, UDamageType::StaticClass());
	}
}

void AStealthCharacter::ReloadWeapon()
{
	if (!CanPerformMissionAction() || bIsReloading || AmmoInMagazine >= MagazineSize || ReserveAmmo <= 0)
	{
		return;
	}
	bIsReloading = true;
	ReloadElapsed = 0.0f;
	bWantsToFire = false;
	bIsAiming = false;
	FireAnimationTimeRemaining = 0.0f;
	ArmsState = EStealthArmsState::Reload;
	if (WeaponReloadAnimation)
	{
		WeaponMesh->PlayAnimation(WeaponReloadAnimation, false);
		WeaponMesh->SetPlayRate(WeaponReloadAnimation->GetPlayLength() / FMath::Max(0.2f, ReloadDuration));
	}
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}
}

void AStealthCharacter::StartAim()
{
	bWantsToAim = CanPerformMissionAction();
	bIsAiming = bWantsToAim && !bIsReloading;
}

void AStealthCharacter::StopAim()
{
	bIsAiming = false;
	bWantsToAim = false;
}

float AStealthCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!CanPerformMissionAction() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	const float AppliedDamage = FMath::Clamp(
		Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser), 0.0f, CurrentHealth);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - AppliedDamage);
	if (CurrentHealth <= 0.0f)
	{
		if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
		{
			GameMode->DefeatPlayer(this);
		}
	}
	return AppliedDamage;
}

bool AStealthCharacter::CanPerformMissionAction() const
{
	const AStealthGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AStealthGameMode>() : nullptr;
	return CurrentHealth > 0.0f && (!GameMode ||
		(GameMode->GetMissionState() == EStealthMissionState::Playing && !GameMode->IsActorProtected(this)));
}

void AStealthCharacter::StopMissionActions()
{
	bWantsToFire = false;
	bWantsToSprint = false;
	bIsAiming = false;
	bWantsToAim = false;
	CancelReload();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SoundEmitter->SetComponentTickEnabled(false);
}

void AStealthCharacter::UpdateFirstPersonAnimation(float DeltaSeconds)
{
	ArmsState = bIsReloading ? EStealthArmsState::Reload
		: FireAnimationTimeRemaining > 0.0f ? EStealthArmsState::Fire
		: bIsAiming ? EStealthArmsState::Aim
		: bIsCrouched ? EStealthArmsState::Crouch
		: GetVelocity().SizeSquared2D() > FMath::Square(20.0f) ? EStealthArmsState::Run : EStealthArmsState::Idle;
}

void AStealthCharacter::UpdateMovementState()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bIsCrouched)
	{
		Movement->MaxWalkSpeed = bIsAiming ? FMath::Min(CrouchSpeed, AimCrouchSpeed) : CrouchSpeed;
		Movement->MaxWalkSpeedCrouched = Movement->MaxWalkSpeed;
		SoundEmitter->Volume = CrouchFootstepVolume;
		SoundEmitter->FootstepInterval = 0.62f;
		return;
	}

	const bool bCanSprint = bWantsToSprint && !bIsAiming && !bIsReloading && GetVelocity().SizeSquared2D() > 1.0f;
	Movement->MaxWalkSpeed = bIsAiming ? FMath::Min(WalkSpeed, AimWalkSpeed)
		: bIsReloading ? WalkSpeed * 0.7f : bCanSprint ? SprintSpeed : WalkSpeed;
	SoundEmitter->Volume = bCanSprint ? SprintFootstepVolume : WalkFootstepVolume;
	SoundEmitter->FootstepInterval = bCanSprint ? 0.32f : 0.48f;
}

void AStealthCharacter::CalibrateWeapon()
{
	// Assault_Rifle_A points along model +Y. The old GripPoint socket was authored for SK_FPGun.
	const FTransform Hand = FirstPersonArms->GetSocketTransform(TEXT("hand_r"), RTS_World);
	const FTransform OldGrip = FirstPersonArms->GetSocketTransform(TEXT("GripPoint"), RTS_World).GetRelativeTransform(Hand);
	const FQuat WeaponWorldRotation = FRotationMatrix::MakeFromYZ(FollowCamera->GetForwardVector(), FollowCamera->GetUpVector()).ToQuat();
	const FQuat GripRotation = Hand.GetRotation().Inverse() * WeaponWorldRotation * WeaponGripTransform.GetRotation();
	const FVector ModelGrip = WeaponMesh->GetSocketTransform(TEXT("Grip_Bone"), RTS_Component).GetLocation();
	WeaponMesh->AttachToComponent(FirstPersonArms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
	WeaponMesh->SetRelativeTransform(FTransform(GripRotation,
		OldGrip.GetLocation() + WeaponGripTransform.GetLocation() - GripRotation.RotateVector(ModelGrip), WeaponGripTransform.GetScale3D()));
	WeaponToHandTransform = WeaponMesh->GetRelativeTransform();

	const FTransform WeaponInArms = WeaponMesh->GetComponentTransform().GetRelativeTransform(FirstPersonArms->GetComponentTransform());
	const FQuat DesiredCameraRotation = FRotationMatrix::MakeFromYZ(FVector::ForwardVector, FVector::UpVector).ToQuat();
	const FQuat ArmsRotation = DesiredCameraRotation * WeaponInArms.GetRotation().Inverse();
	const FVector ArmsLocation = FVector(AimSightDistance, 0, 0) - ArmsRotation.RotateVector(WeaponInArms.TransformPosition(IronSightLocation));
	AimArmsTransform = FTransform(ArmsRotation, ArmsLocation);
}

void AStealthCharacter::UpdateFirstPersonPose(float DeltaSeconds)
{
	AimAlpha = FMath::FInterpTo(AimAlpha, bIsAiming ? 1.0f : 0.0f, DeltaSeconds, AimInterpSpeed);
	CrouchAlpha = FMath::FInterpTo(CrouchAlpha, bIsCrouched ? 1.0f : 0.0f, DeltaSeconds, 9.0f);
	FollowCamera->SetFieldOfView(FMath::Lerp(HipFieldOfView, AimFieldOfView, AimAlpha));
	FTransform Target;
	Target.Blend(FTransform(FQuat::Identity, HipArmsLocation), AimArmsTransform, AimAlpha);
	FVector Offset(0, -CrouchAlpha, -2.0f * CrouchAlpha);
	Offset *= 1.0f - AimAlpha;
	const float TargetTilt = bIsReloading ? FMath::SmoothStep(0.0f, 0.16f, GetReloadProgress())
		* (1.0f - FMath::SmoothStep(0.82f, 1.0f, GetReloadProgress())) : 0.0f;
	ReloadPoseTilt = FMath::FInterpTo(ReloadPoseTilt, TargetTilt, DeltaSeconds, 18.0f);
	Offset += FVector(12, -10, 25) * ReloadPoseTilt;
	// Rotate around the grip, not the skeleton origin at the character's feet.
	const FVector Pivot = FirstPersonArms->GetSocketTransform(TEXT("hand_r"), RTS_Component).GetLocation();
	const FVector PivotInCamera = Target.TransformPosition(Pivot);
	const FQuat TiltedRotation = FQuat::Slerp(FQuat::Identity, FQuat(FRotator(-8, 10, -20)), ReloadPoseTilt) * Target.GetRotation();
	Target.SetRotation(TiltedRotation);
	Target.SetLocation(PivotInCamera - TiltedRotation.RotateVector(Pivot));
	Target.AddToTranslation(Offset);
	FirstPersonArms->SetRelativeTransform(Target);
}

float AStealthCharacter::GetReloadProgress() const
{
	return bIsReloading ? FMath::Clamp(ReloadElapsed / FMath::Max(0.2f, ReloadDuration), 0.0f, 1.0f) : 0.0f;
}

float AStealthCharacter::GetShotAnimationAge() const
{
	return FireAnimationTimeRemaining > 0.0f && ArmsFireAnimation
		? ArmsFireAnimation->GetPlayLength() - FireAnimationTimeRemaining : BIG_NUMBER;
}

void AStealthCharacter::CancelReload()
{
	if (bIsReloading)
	{
		bIsReloading = false;
		ReloadElapsed = 0.0f;
		WeaponMesh->SetAnimation(nullptr);
	}
}

void AStealthCharacter::UpdateReload(float DeltaSeconds)
{
	if (!bIsReloading)
	{
		return;
	}
	ReloadElapsed += DeltaSeconds;
	if (ReloadElapsed >= FMath::Max(0.2f, ReloadDuration))
	{
		const int32 AmmoToLoad = FMath::Min(FMath::Max(0, MagazineSize - AmmoInMagazine), ReserveAmmo);
		AmmoInMagazine += AmmoToLoad;
		ReserveAmmo -= AmmoToLoad;
		CancelReload();
	}
}

bool AStealthCharacter::CanJumpInternal_Implementation() const
{
	return false;
}

void AStealthCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (GetCharacterMovement()->IsFalling() && PrevMovementMode != MOVE_Falling)
	{
		PeakFallSpeed = FMath::Max(0.0f, -GetVelocity().Z);
	}
}

void AStealthCharacter::Landed(const FHitResult& Hit)
{
	const float ImpactSpeed = FMath::Max(PeakFallSpeed, -GetVelocity().Z);
	Super::Landed(Hit);
	PeakFallSpeed = 0.0f;
	LastLandingStrength = 0.0f;
	const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
	if (CurrentHealth <= 0.0f || ImpactSpeed < MinLandingSpeed
		|| (Mission && Mission->GetMissionState() != EStealthMissionState::Playing))
	{
		return;
	}
	const float ImpactAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(MinLandingSpeed, FMath::Max(MinLandingSpeed + 1.0f, HardLandingSpeed)), FVector2D(0, 1), ImpactSpeed);
	LastLandingStrength = FMath::Lerp(0.18f, 1.2f, ImpactAlpha * ImpactAlpha);
	if (LandingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LandingSound, GetActorLocation(),
			FMath::Clamp(LastLandingStrength, 0.18f, 1.0f), FMath::Lerp(1.12f, 0.82f, ImpactAlpha));
	}
	const float PreviousVolume = SoundEmitter->Volume;
	SoundEmitter->Volume = LastLandingStrength;
	SoundEmitter->EmitFootstepSound();
	SoundEmitter->Volume = PreviousVolume;
}
