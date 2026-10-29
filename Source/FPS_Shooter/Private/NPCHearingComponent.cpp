#include "NPCHearingComponent.h"

#include "DrawDebugHelpers.h"

TArray<TWeakObjectPtr<UNPCHearingComponent>> UNPCHearingComponent::RegisteredComponents;

UNPCHearingComponent::UNPCHearingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
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
	SoundMemory.RemoveAll([this](const FNPCHearingStimulus& Sound) { return Sound.Age >= GetMemoryDuration(); });
	RefreshBestSound();

	if (bDrawDebug && GetWorld() && HasHeardSound())
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

	Strength = FMath::Clamp(Strength, 0.0f, 2.0f);
	// Combine nearby rays in one propagation burst without extending its lifetime.
	FNPCHearingStimulus* Existing = SoundMemory.FindByPredicate([&](const FNPCHearingStimulus& Sound)
	{
		return Sound.Age < 0.3f && FVector::DistSquared(Sound.Location, SoundLocation) < FMath::Square(100.0f);
	});
	if (Existing)
	{
		if (Strength > Existing->Strength)
		{
			Existing->Strength = Strength;
			Existing->Location = SoundLocation;
			Existing->Id = ++HearingEventId;
		}
	}
	else
	{
		SoundMemory.Add({ SoundLocation, Strength, 0.0f, ++HearingEventId });
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

	if (bDrawDebug && GetWorld())
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

float UNPCHearingComponent::GetSoundPriority(const FNPCHearingStimulus& Sound) const
{
	const float Distance = GetOwner() ? FVector::Dist(GetOwner()->GetActorLocation(), Sound.Location) : 0.0f;
	const float Freshness = FMath::Square(FMath::Clamp(1.0f - Sound.Age / GetMemoryDuration(), 0.0f, 1.0f));
	return Sound.Strength * Freshness / (1.0f + Distance / 1200.0f);
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
		CurrentSoundStrength = Best.Strength * FMath::Clamp(1.0f - Best.Age / GetMemoryDuration(), 0.0f, 1.0f);
	}
	else
	{
		CurrentSoundStrength = 0.0f;
		LastReceivedSoundStrength = 0.0f;
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
