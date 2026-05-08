#include "SoundHearingNPC.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "NPCHearingComponent.h"
#include "UObject/ConstructorHelpers.h"

ASoundHearingNPC::ASoundHearingNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnimBP(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"));
	if (MannyAnimBP.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(MannyAnimBP.Class);
	}

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;

	HearingComponent = CreateDefaultSubobject<UNPCHearingComponent>(TEXT("HearingComponent"));
	HearingComponent->HearingRadius = 120.0f;
	NPCLabel = FText::FromString(TEXT("NPC"));

	SoundLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SoundLabel"));
	SoundLabel->SetupAttachment(GetRootComponent());
	SoundLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	SoundLabel->SetHorizontalAlignment(EHTA_Center);
	SoundLabel->SetVerticalAlignment(EVRTA_TextCenter);
	SoundLabel->SetTextRenderColor(FColor::Cyan);
	SoundLabel->SetWorldSize(28.0f);
	SoundLabel->SetText(FText::FromString(TEXT("Sound: 0%")));
}

void ASoundHearingNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HearingComponent || !SoundLabel)
	{
		return;
	}

	const int32 SoundPercent = FMath::RoundToInt(FMath::Clamp(HearingComponent->CurrentSoundStrength, 0.0f, 1.0f) * 100.0f);
	SoundLabel->SetText(FText::Format(NSLOCTEXT("SoundNPC", "LabeledSoundPercent", "{0}\nSound: {1}%"), NPCLabel, FText::AsNumber(SoundPercent)));

	const APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector ToCamera = CameraManager->GetCameraLocation() - SoundLabel->GetComponentLocation();
		SoundLabel->SetWorldRotation(ToCamera.Rotation());
	}

	if (HearingComponent->HasHeardSound())
	{
		const FVector ToSound = HearingComponent->LastHeardPosition - GetActorLocation();
		const FRotator TargetRotation(0.0f, ToSound.Rotation().Yaw, 0.0f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, 5.0f));
	}
}
