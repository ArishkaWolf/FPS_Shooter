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
#include "StealthAIController.h"
#include "UObject/ConstructorHelpers.h"

ASoundHearingNPC::ASoundHearingNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AStealthAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnimBlueprint(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny"));
	if (MannyAnimBlueprint.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(MannyAnimBlueprint.Class);
	}

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

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
