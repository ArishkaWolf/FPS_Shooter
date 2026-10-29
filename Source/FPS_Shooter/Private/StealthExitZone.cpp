#include "StealthExitZone.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"

AStealthExitZone::AStealthExitZone()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerComponent"));
	TriggerComponent->InitBoxExtent(FVector(140.0f, 140.0f, 110.0f));
	TriggerComponent->SetCollisionProfileName(TEXT("Trigger"));
	SetRootComponent(TriggerComponent);

	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LabelComponent"));
	LabelComponent->SetupAttachment(RootComponent);
	LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetWorldSize(38.0f);

	LightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
	LightComponent->SetupAttachment(RootComponent);
	LightComponent->SetAttenuationRadius(450.0f);
	LightComponent->SetIntensity(1600.0f);
}

void AStealthExitZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AStealthGameMode* GameMode = Cast<AStealthGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bUnlocked = GameMode && GameMode->HasObjective();
	const bool bPursued = GameMode && GameMode->IsPlayerPursued();
	const FColor ExitColor = !bUnlocked ? FColor::Red : bPursued ? FColor::Yellow : FColor::Green;
	LabelComponent->SetText(FText::FromString(!bUnlocked ? TEXT("EXIT - INTEL REQUIRED")
		: bPursued ? TEXT("EXIT - LOSE THE GUARDS") : TEXT("EXIT - READY")));
	LabelComponent->SetTextRenderColor(ExitColor);
	LightComponent->SetLightColor(FLinearColor(ExitColor));

	// Recheck occupants when intel or pursuit changes; no need to leave and re-enter the trigger.
	if (bUnlocked && !bPursued)
	{
		AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && TriggerComponent->IsOverlappingActor(Player))
		{
			if (AStealthGameMode* MutableGameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
			{
				MutableGameMode->TryCompleteMission(Player);
			}
		}
	}
}

void AStealthExitZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->IsA<AStealthCharacter>())
	{
		return;
	}

	if (AStealthGameMode* GameMode = Cast<AStealthGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->TryCompleteMission(OtherActor);
	}
}

void AStealthExitZone::BeginPlay()
{
	Super::BeginPlay();
	TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AStealthExitZone::OnTriggerBeginOverlap);
}
