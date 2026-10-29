#include "SoundHearingNPC.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "NPCHearingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "StealthAIController.h"
#include "StealthGameMode.h"
#include "UObject/ConstructorHelpers.h"

ASoundHearingNPC::ASoundHearingNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AStealthAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnimBlueprint(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"));
	if (MannyAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(MannyAnimBlueprint.Class);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> RifleAnimBlueprint(TEXT("/Game/AnimStarterPack/UE5/UE5_UE4ASP_HeroTPP_AnimBlueprint"));
	if (RifleAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(RifleAnimBlueprint.Class);
	}

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetRelativeLocation(FVector(2.0f, 10.0f, 0.0f));
	WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/MilitaryWeapSilver/Weapons/Assault_Rifle_A.Assault_Rifle_A"));
	if (RifleMesh.Succeeded())
	{
		WeaponMesh->SetSkeletalMesh(RifleMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> RifleFireSound(TEXT("/Game/MilitaryWeapSilver/Sound/Rifle/Cues/RifleA_Fire_Cue.RifleA_Fire_Cue"));
	FireSound = RifleFireSound.Object;
	static ConstructorHelpers::FObjectFinder<UParticleSystem> RifleMuzzleFlash(TEXT("/Game/MilitaryWeapSilver/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"));
	MuzzleFlash = RifleMuzzleFlash.Object;

	HearingComponent = CreateDefaultSubobject<UNPCHearingComponent>(TEXT("HearingComponent"));
	HearingComponent->HearingRadius = 220.0f;
	HearingComponent->ForgetDurationSeconds = 12.0f;
	HearingComponent->bDrawDebug = false;
	NPCLabel = FText::FromString(TEXT("NPC"));

	SoundLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SoundLabel"));
	SoundLabel->SetupAttachment(GetRootComponent());
	SoundLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	SoundLabel->SetHorizontalAlignment(EHTA_Center);
	SoundLabel->SetVerticalAlignment(EVRTA_TextCenter);
	SoundLabel->SetTextRenderColor(FColor::Cyan);
	SoundLabel->SetWorldSize(28.0f);
	SoundLabel->SetText(FText::FromString(TEXT("Sound: 0%")));
	CurrentHealth = MaxHealth;
}

void ASoundHearingNPC::FireWeaponAt(AActor* TargetActor)
{
	if (!IsValid(TargetActor) || !GetWorld())
	{
		return;
	}
	if (const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		GameMode && (GameMode->GetMissionState() != EStealthMissionState::Playing || GameMode->IsActorProtected(TargetActor)))
	{
		return;
	}

	const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const FVector TargetLocation = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
	const FVector ShotDirection = (TargetLocation - TraceStart).GetSafeNormal();
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GuardWeaponFire), true, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TargetLocation, ECC_Visibility, QueryParams) && Hit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), FireDamage, ShotDirection, Hit,
			GetController(), this, UDamageType::StaticClass());
	}

	if (MuzzleFlash)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh, TEXT("Muzzle"));
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}
}

float ASoundHearingNPC::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f || CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}
	if (const AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		GameMode && GameMode->GetMissionState() != EStealthMissionState::Playing)
	{
		return 0.0f;
	}
	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - AppliedDamage);
	if (CurrentHealth <= 0.0f)
	{
		Destroy();
	}
	return AppliedDamage;
}

void ASoundHearingNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HearingComponent || !SoundLabel)
	{
		return;
	}

	const int32 SoundPercent = FMath::RoundToInt(FMath::Clamp(HearingComponent->CurrentSoundStrength, 0.0f, 1.0f) * 100.0f);
	const AStealthAIController* StealthController = Cast<AStealthAIController>(GetController());
	const int32 DetectionPercent = StealthController
		? FMath::RoundToInt(StealthController->GetDetectionLevel() * 100.0f)
		: 0;
	const FText StateName = StealthController
		? StealthController->GetStateDisplayName()
		: NSLOCTEXT("StealthAI", "NoController", "NO AI");
	SoundLabel->SetText(FText::Format(
		NSLOCTEXT("SoundNPC", "NPCStatus", "{0}\n{1}  Sight: {2}%\nSound: {3}%"),
		NPCLabel,
		StateName,
		FText::AsNumber(DetectionPercent),
		FText::AsNumber(SoundPercent)));

	const APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector ToCamera = CameraManager->GetCameraLocation() - SoundLabel->GetComponentLocation();
		SoundLabel->SetWorldRotation(ToCamera.Rotation());
	}

}
