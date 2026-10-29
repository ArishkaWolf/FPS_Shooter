#include "StealthDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SoundRayInteractionComponent.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "UObject/ConstructorHelpers.h"

AStealthDoor::AStealthDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Hinge")));
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(GetRootComponent());
	DoorMesh->SetMobility(EComponentMobility::Movable);
	DoorMesh->SetRelativeLocation(FVector(0, 50, 100));
	DoorMesh->SetRelativeScale3D(FVector(0.16f, 1.0f, 2.0f));
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	DoorMesh->SetStaticMesh(Cube.Object);
	SoundEmitter = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SoundEmitter"));
	SoundEmitter->SoundOffset = FVector(0, 50, 100);
}

void AStealthDoor::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	if (IsValid(InteractingCharacter) && InteractingCharacter->CanPerformMissionAction())
	{
		SetOpen(!bIsOpen, InteractingCharacter);
	}
}

bool AStealthDoor::SetOpen(bool bOpen, AActor* Initiator)
{
	if (bIsOpen == bOpen) { return false; }
	if (const AStealthGameMode* Mission = GetWorld() ? GetWorld()->GetAuthGameMode<AStealthGameMode>() : nullptr;
		Mission && Mission->GetMissionState() != EStealthMissionState::Playing) { return false; }
	bIsOpen = bOpen;
	SoundEmitter->NotifyDoor(bIsOpen, Initiator);
	return true;
}

void AStealthDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Previous = OpenAlpha;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, bIsOpen ? 1.0f : 0.0f, DeltaSeconds, 1.0f / FMath::Max(0.1f, OpenDuration));
	if (!FMath::IsNearlyEqual(Previous, OpenAlpha))
	{
		// Rotate the leaf around the hinge, not around its mesh center.
		const FRotator Rotation(0, OpenAngle * FMath::SmoothStep(0.0f, 1.0f, OpenAlpha), 0);
		DoorMesh->SetRelativeLocation(Rotation.RotateVector(FVector(0, 50, 100)));
		DoorMesh->SetRelativeRotation(Rotation);
	}
}
