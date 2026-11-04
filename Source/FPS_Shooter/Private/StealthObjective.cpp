#include "StealthObjective.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "SoundRayInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

AStealthObjective::AStealthObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	SoundEmitter = CreateDefaultSubobject<USoundRayInteractionComponent>(TEXT("SoundEmitter"));

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(55.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	SetRootComponent(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeScale3D(FVector(0.55f, 0.35f, 0.18f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelComponent"));
	LabelComponent->SetupAttachment(RootComponent);
	LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetTextRenderColor(FColor::Yellow);
	LabelComponent->SetWorldSize(32.0f);
	LabelComponent->SetText(FText::FromString(TEXT("INTEL [E]")));

	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetLightColor(FLinearColor(1.0f, 0.65f, 0.05f));
	LightComponent->SetIntensity(1200.0f);
	LightComponent->SetAttenuationRadius(350.0f);
}

void AStealthObjective::Interact_Implementation(AStealthCharacter* InteractingCharacter)
{
	AStealthGameMode* GameMode = Cast<AStealthGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode && GameMode->CollectObjective(InteractingCharacter))
	{
		SoundEmitter->NotifyInteraction(InteractingCharacter);
		Destroy();
	}
}
