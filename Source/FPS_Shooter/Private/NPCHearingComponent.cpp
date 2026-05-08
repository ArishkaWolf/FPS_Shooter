#include "NPCHearingComponent.h"

#include "DrawDebugHelpers.h"

TArray<TWeakObjectPtr<UNPCHearingComponent>> UNPCHearingComponent::RegisteredComponents;

UNPCHearingComponent::UNPCHearingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNPCHearingComponent::BeginPlay()
{
	Super::BeginPlay();

	RegisteredComponents.RemoveAll([](const TWeakObjectPtr<UNPCHearingComponent>& Component)
	{
		return !Component.IsValid();
	});
	RegisteredComponents.AddUnique(this);
}

void UNPCHearingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RegisteredComponents.Remove(this);
	Super::EndPlay(EndPlayReason);
}

void UNPCHearingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentSoundStrength > 0.0f)
	{
		TimeSinceLastHeard += DeltaTime;
		const float ForgetDuration = ForgetDurationSeconds > KINDA_SMALL_NUMBER
			? ForgetDurationSeconds
			: (ForgetSpeed > KINDA_SMALL_NUMBER ? 1.0f / ForgetSpeed : 60.0f);
		const float RememberAlpha = FMath::Clamp(1.0f - TimeSinceLastHeard / ForgetDuration, 0.0f, 1.0f);
		CurrentSoundStrength = RememberedSoundStrength * RememberAlpha;

		if (bDrawDebug && GetWorld())
		{
			const FVector OwnerLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
			const FVector DebugLocation = OwnerLocation + FVector(0.0f, 0.0f, 120.0f);
			const float Radius = FMath::Lerp(16.0f, 52.0f, FMath::Clamp(CurrentSoundStrength, 0.0f, 1.0f));
			DrawDebugSphere(GetWorld(), DebugLocation, Radius, 16, FColor::Cyan, false, DebugDrawDuration, 0, 2.0f);
			DrawDebugLine(GetWorld(), DebugLocation, LastHeardPosition, FColor::Cyan, false, DebugDrawDuration, 0, 1.5f);
		}
	}
}

void UNPCHearingComponent::HearSoundAtLocation(const FVector& SoundLocation, float Strength)
{
	if (Strength <= CurrentSoundStrength)
	{
		return;
	}

	LastHeardPosition = SoundLocation;
	LastReceivedSoundStrength = Strength;
	RememberedSoundStrength = Strength;
	CurrentSoundStrength = Strength;
	TimeSinceLastHeard = 0.0f;

	if (bDrawDebug && GetWorld())
	{
		const FVector OwnerLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		DrawDebugSphere(GetWorld(), OwnerLocation + FVector(0.0f, 0.0f, 120.0f), 60.0f, 20, FColor::Green, false, 0.6f, 0, 3.0f);
	}
}

const TArray<TWeakObjectPtr<UNPCHearingComponent>>& UNPCHearingComponent::GetRegisteredComponents()
{
	return RegisteredComponents;
}
