#include "NPCHearingComponent.h"

#include "DrawDebugHelpers.h"

TArray<TWeakObjectPtr<UNPCHearingComponent>> UNPCHearingComponent::RegisteredComponents;

UNPCHearingComponent::UNPCHearingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	TypePriorityWeights = { { ESoundEventType::Footstep, 1.0f }, { ESoundEventType::Landing, 1.25f },
		{ ESoundEventType::Gunshot, 2.5f }, { ESoundEventType::ThrownObject, 1.5f },
		{ ESoundEventType::Door, 1.2f }, { ESoundEventType::Interaction, 0.8f } };
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

	for (FNPCHearingStimulus& Sound : SoundMemory)
	{
		Sound.Age += DeltaTime;
	}
	SoundMemory.RemoveAll([this](const FNPCHearingStimulus& Sound) { return Sound.Age >= GetMemoryDuration(Sound); });
	RefreshBestSound();

	if (IsSoundRayDebugEnabled() && GetWorld() && HasHeardSound())
	{
		const FVector OwnerLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		const FVector DebugLocation = OwnerLocation + FVector(0.0f, 0.0f, 120.0f);
		const float Radius = FMath::Lerp(16.0f, 52.0f, FMath::Clamp(CurrentSoundStrength, 0.0f, 1.0f));
		DrawDebugSphere(GetWorld(), DebugLocation, Radius, 16, FColor::Cyan, false, DebugDrawDuration, 0, 2.0f);
		DrawDebugLine(GetWorld(), DebugLocation, LastHeardPosition, FColor::Cyan, false, DebugDrawDuration, 0, 1.5f);
	}
}

void UNPCHearingComponent::HearSoundAtLocation(const FVector& SoundLocation, float Strength)
{
	if (!IsActive() || !FMath::IsFinite(Strength) || Strength <= 0.0f || SoundLocation.ContainsNaN())
	{
		return;
	}

	FNPCHearingStimulus Sound;
	Sound.Location = SoundLocation;
	Sound.Strength = FMath::Clamp(Strength, 0.0f, 2.0f);
	Sound.Event.Strength = Sound.Event.EmittedStrength = Sound.Strength;
	Sound.Event.SourceLocation = Sound.Event.HeardLocation = Sound.Event.ArrivalLocation = SoundLocation;
	Sound.Event.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	StoreSound(Sound);
}

void UNPCHearingComponent::HearSoundEvent(const FSoundRayEvent& Event)
{
	if (!IsActive() || !Event.EventId.IsValid() || !FMath::IsFinite(Event.Strength) || Event.Strength <= 0.0f
		|| !FMath::IsFinite(Event.TimeSeconds) || Event.SourceLocation.ContainsNaN() || Event.HeardLocation.ContainsNaN()) { return; }
	FNPCHearingStimulus Sound;
	Sound.Location = Event.HeardLocation;
	Sound.Strength = FMath::Clamp(Event.Strength, 0.0f, 2.0f);
	Sound.Age = GetWorld() ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - Event.TimeSeconds) : 0.0f;
	Sound.Event = Event;
	Sound.Event.Strength = Sound.Strength;
	Sound.MemoryDuration = FMath::IsFinite(Event.MemoryDuration) && Event.MemoryDuration > KINDA_SMALL_NUMBER
		? Event.MemoryDuration : GetMemoryDuration();
	if (Sound.Age < Sound.MemoryDuration) { StoreSound(Sound); }
}

void UNPCHearingComponent::StoreSound(FNPCHearingStimulus Incoming)
{
	// Typed reflections share an emission identity; separate sources/types never coalesce.
	FNPCHearingStimulus* Existing = SoundMemory.FindByPredicate([&](const FNPCHearingStimulus& Sound)
	{
		if (Incoming.Event.EventId.IsValid()) { return Sound.Event.EventId == Incoming.Event.EventId; }
		return !Sound.Event.EventId.IsValid() && Sound.Age < 0.3f
			&& FVector::DistSquared(Sound.Location, Incoming.Location) < FMath::Square(100.0f);
	});
	if (Existing)
	{
		if (Incoming.Strength > Existing->Strength)
		{
			Incoming.Age = FMath::Max(Incoming.Age, Existing->Age);
			Incoming.Id = ++HearingEventId;
			*Existing = Incoming;
		}
	}
	else
	{
		Incoming.Id = ++HearingEventId;
		SoundMemory.Add(Incoming);
	}
	if (SoundMemory.Num() > 16)
	{
		int32 Weakest = 0;
		for (int32 Index = 1; Index < SoundMemory.Num(); ++Index)
		{
			if (GetSoundPriority(SoundMemory[Index]) < GetSoundPriority(SoundMemory[Weakest])) { Weakest = Index; }
		}
		SoundMemory.RemoveAt(Weakest);
	}
	RefreshBestSound();

	if (IsSoundRayDebugEnabled() && GetWorld())
	{
		const FVector OwnerLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		DrawDebugSphere(GetWorld(), OwnerLocation + FVector(0.0f, 0.0f, 120.0f), 60.0f, 20, FColor::Green, false, 0.6f, 0, 3.0f);
	}
}

float UNPCHearingComponent::GetMemoryDuration() const
{
	return ForgetDurationSeconds > KINDA_SMALL_NUMBER ? ForgetDurationSeconds
		: ForgetSpeed > KINDA_SMALL_NUMBER ? 1.0f / ForgetSpeed : 60.0f;
}

float UNPCHearingComponent::GetMemoryDuration(const FNPCHearingStimulus& Sound) const
{
	return Sound.MemoryDuration > KINDA_SMALL_NUMBER ? Sound.MemoryDuration : GetMemoryDuration();
}

float UNPCHearingComponent::GetSoundPriority(const FNPCHearingStimulus& Sound) const
{
	const FVector Position = Sound.Event.EventId.IsValid() ? Sound.Event.SourceLocation : Sound.Location;
	const float Distance = GetOwner() ? FVector::Dist(GetOwner()->GetActorLocation(), Position) : 0.0f;
	const float Freshness = FMath::Square(FMath::Clamp(1.0f - Sound.Age / GetMemoryDuration(Sound), 0.0f, 1.0f));
	const float* TypeWeight = TypePriorityWeights.Find(Sound.Event.Type);
	return Sound.Strength * (TypeWeight ? FMath::Max(0.0f, *TypeWeight) : 1.0f) * Freshness / (1.0f + Distance / 1200.0f);
}

bool UNPCHearingComponent::GetBestSound(FNPCHearingStimulus& OutSound) const
{
	float BestPriority = KINDA_SMALL_NUMBER;
	bool bFound = false;
	for (const FNPCHearingStimulus& Sound : SoundMemory)
	{
		const float Priority = GetSoundPriority(Sound);
		if (Priority > BestPriority || (bFound && FMath::IsNearlyEqual(Priority, BestPriority) && Sound.Id > OutSound.Id))
		{
			BestPriority = Priority;
			OutSound = Sound;
			bFound = true;
		}
	}
	return bFound;
}

void UNPCHearingComponent::RefreshBestSound()
{
	FNPCHearingStimulus Best;
	if (GetBestSound(Best))
	{
		LastHeardPosition = Best.Location;
		LastReceivedSoundStrength = Best.Strength;
		LastHeardEvent = Best.Event;
		CurrentSoundStrength = Best.Strength * FMath::Clamp(1.0f - Best.Age / GetMemoryDuration(Best), 0.0f, 1.0f);
	}
	else
	{
		CurrentSoundStrength = 0.0f;
		LastReceivedSoundStrength = 0.0f;
		LastHeardEvent = FSoundRayEvent();
	}
}

void UNPCHearingComponent::ClearSoundMemory()
{
	SoundMemory.Reset();
	RefreshBestSound();
}

const TArray<TWeakObjectPtr<UNPCHearingComponent>>& UNPCHearingComponent::GetRegisteredComponents()
{
	return RegisteredComponents;
}
