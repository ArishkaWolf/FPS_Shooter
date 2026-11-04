#include "SoundRayInteractionComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "StealthGameMode.h"
#include "UObject/ConstructorHelpers.h"

USoundRayInteractionComponent::USoundRayInteractionComponent()
{
	bAutoEmitFootsteps = false;
	RayCount = 16;
	RayPitchDegrees = 0.0f;
	static ConstructorHelpers::FObjectFinder<USoundBase> Pickup(TEXT("/Game/MilitaryWeapSilver/Sound/Rifle/Cues/Rifle_AmmoPickup_Cue"));
	InteractionAudio = Pickup.Object;
	// Existing impact is a usable latch default; each door can supply its own opening/closing audio.
	static ConstructorHelpers::FObjectFinder<USoundBase> Impact(TEXT("/Game/Audio/Stealth/Landing_Impact"));
	DoorOpenAudio = DoorCloseAudio = Impact.Object;
}

void USoundRayInteractionComponent::NotifyInteraction(AActor* Initiator)
{
	EmitAction(ESoundEventType::Interaction, InteractionVolume, InteractionDistance, Initiator, InteractionAudio);
}

void USoundRayInteractionComponent::NotifyDoor(bool bOpening, AActor* Initiator)
{
	EmitAction(ESoundEventType::Door, DoorVolume, DoorDistance, Initiator, bOpening ? DoorOpenAudio.Get() : DoorCloseAudio.Get());
}

void USoundRayInteractionComponent::EmitAction(ESoundEventType Type, float Strength, float Distance, AActor* Initiator, USoundBase* Audio)
{
	if (!GetOwner() || !GetWorld()) { return; }
	if (const AStealthGameMode* Mission = GetWorld()->GetAuthGameMode<AStealthGameMode>();
		Mission && Mission->GetMissionState() != EStealthMissionState::Playing) { return; }
	const FVector Location = GetOwner()->GetActorTransform().TransformPosition(SoundOffset);
	EmitSoundEventAtLocation(Type, Location, Strength, Distance, Initiator);
	if (Audio && Strength > 0.0f) { UGameplayStatics::PlaySoundAtLocation(this, Audio, Location, FMath::Clamp(Strength, 0.0f, 1.0f)); }
}
