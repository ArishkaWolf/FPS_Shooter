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
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	Movement->JumpZVelocity = 500.0f;
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
	FirstPersonArms->SetRelativeLocation(FVector(-30.0f, 0.0f, -150.0f));
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->CastShadow = false;

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

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GunMesh(TEXT("/Game/FPWeapon/Mesh/SK_FPGun.SK_FPGun"));
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
	static ConstructorHelpers::FObjectFinder<USoundBase> RifleFireSound(TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));
	FireSound = RifleFireSound.Object;
	static ConstructorHelpers::FObjectFinder<UParticleSystem> RifleMuzzleFlash(TEXT("/Game/MilitaryWeapSilver/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"));
	MuzzleFlash = RifleMuzzleFlash.Object;

	NoiseMakerClass = AFallingSoundSphere::StaticClass();
	AmmoInMagazine = MagazineSize;
	CreateInputMapping();
}

void AStealthCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyInputMapping();
	FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (ArmsIdleAnimation)
	{
		FirstPersonArms->PlayAnimation(ArmsIdleAnimation, true);
	}
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
	UpdateMovementState();
	UpdateFirstPersonAnimation(DeltaSeconds);
	TimeUntilNextShot = FMath::Max(0.0f, TimeUntilNextShot - DeltaSeconds);
	if (bWantsToFire && TimeUntilNextShot <= 0.0f)
	{
		FireWeapon();
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(2402, 0.0f, FColor::White,
			FString::Printf(TEXT("AMMO %d / %d"), AmmoInMagazine, ReserveAmmo));
	}
}

void AStealthCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStealthCharacter::Move);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AStealthCharacter::Look);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AStealthCharacter::StartSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AStealthCharacter::StopSprint);
	EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AStealthCharacter::ToggleCrouch);
	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AStealthCharacter::Interact);
	EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Started, this, &AStealthCharacter::ThrowNoiseMaker);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AStealthCharacter::StartFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AStealthCharacter::StopFire);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AStealthCharacter::ReloadWeapon);
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
	if (NoiseMakerCount <= 0 || !NoiseMakerClass)
	{
		return;
	}

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
	if (AmmoInMagazine <= 0)
	{
		bWantsToFire = false;
		return;
	}

	--AmmoInMagazine;
	TimeUntilNextShot = FireInterval;

	if (ArmsFireAnimation)
	{
		FirstPersonArms->PlayAnimation(ArmsFireAnimation, false);
		FireAnimationTimeRemaining = ArmsFireAnimation->GetPlayLength();
		bArmsPlayingRun = GetVelocity().SizeSquared2D() <= FMath::Square(20.0f);
	}
	if (MuzzleFlash)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh, TEXT("Muzzle"));
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
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), WeaponDamage, ShotDirection, Hit,
			GetController(), this, UDamageType::StaticClass());
	}
}

void AStealthCharacter::ReloadWeapon()
{
	const int32 NeededAmmo = MagazineSize - AmmoInMagazine;
	const int32 AmmoToLoad = FMath::Min(NeededAmmo, ReserveAmmo);
	AmmoInMagazine += AmmoToLoad;
	ReserveAmmo -= AmmoToLoad;
}

void AStealthCharacter::UpdateFirstPersonAnimation(float DeltaSeconds)
{
	if (FireAnimationTimeRemaining > 0.0f)
	{
		FireAnimationTimeRemaining = FMath::Max(0.0f, FireAnimationTimeRemaining - DeltaSeconds);
		return;
	}

	const bool bShouldPlayRun = GetVelocity().SizeSquared2D() > FMath::Square(20.0f);
	if (bShouldPlayRun == bArmsPlayingRun)
	{
		return;
	}

	bArmsPlayingRun = bShouldPlayRun;
	UAnimSequence* Animation = bArmsPlayingRun ? ArmsRunAnimation : ArmsIdleAnimation;
	if (Animation)
	{
		FirstPersonArms->PlayAnimation(Animation, true);
	}
}

void AStealthCharacter::UpdateMovementState()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bIsCrouched)
	{
		Movement->MaxWalkSpeed = CrouchSpeed;
		SoundEmitter->Volume = CrouchFootstepVolume;
		SoundEmitter->FootstepInterval = 0.62f;
		return;
	}

	const bool bCanSprint = bWantsToSprint && GetVelocity().SizeSquared2D() > 1.0f;
	Movement->MaxWalkSpeed = bCanSprint ? SprintSpeed : WalkSpeed;
	SoundEmitter->Volume = bCanSprint ? SprintFootstepVolume : WalkFootstepVolume;
	SoundEmitter->FootstepInterval = bCanSprint ? 0.32f : 0.48f;
}
