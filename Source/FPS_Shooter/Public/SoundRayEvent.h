#pragma once

#include "CoreMinimal.h"
#include "SoundRayEvent.generated.h"

UENUM(BlueprintType)
enum class ESoundEventType : uint8
{
	Footstep,
	Landing,
	Gunshot,
	ThrownObject,
	Door,
	Interaction
};

// One immutable identity per emission, shared by all rays and reflections.
// Weak actor references do not keep a consumed pickup or projectile alive.
USTRUCT(BlueprintType)
struct FPS_SHOOTER_API FSoundRayEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	FGuid EventId;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	ESoundEventType Type = ESoundEventType::Footstep;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	float Strength = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	float EmittedStrength = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	FVector SourceLocation = FVector::ZeroVector;
	// Apparent origin of the arriving segment: emitter or last reflecting wall.
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	FVector HeardLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	FVector ArrivalLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	double TimeSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	TWeakObjectPtr<AActor> SourceActor;
	// Actor responsible for the sound, e.g. the player who threw a noise maker.
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	TWeakObjectPtr<AActor> SourceOwner;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	float MemoryDuration = 60.0f;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	float RayDistance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Sound Rays")
	int32 ReflectionCount = 0;
};

// SoundRays.Debug 0/1 is the sole runtime switch, including legacy serialized debug flags.
FPS_SHOOTER_API bool IsSoundRayDebugEnabled();
