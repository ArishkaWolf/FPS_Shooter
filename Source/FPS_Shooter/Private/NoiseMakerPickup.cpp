#include "NoiseMakerPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StealthCharacter.h"
#include "SoundRayInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

ANoiseMakerPickup::ANoiseMakerPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	SoundEmitter = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SoundEmitter"));

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(35.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	SetRootComponent(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.45f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}
}

void ANoiseMakerPickup::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	if (!IsValid(InteractingCharacter))
	{
		return;
	}

	InteractingCharacter->AddNoiseMakers(Amount);
	SoundEmitter->NotifyInteraction(InteractingCharacter);
	Destroy();
}
