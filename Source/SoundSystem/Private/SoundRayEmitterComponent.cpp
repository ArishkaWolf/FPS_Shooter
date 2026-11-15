#include "SoundRayEmitterComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NPCHearingComponent.h"

USoundRayEmitterComponent::USoundRayEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USoundRayEmitterComponent::BeginPlay()
{
	Super::BeginPlay();
	TimeUntilNextFootstep = FootstepInterval;
}

void USoundRayEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAutoEmitFootsteps)
	{
		return;
	}

	TimeUntilNextFootstep -= DeltaTime;
	if (TimeUntilNextFootstep <= 0.0f && ShouldAutoEmitFootstep())
	{
		EmitFootstepSound();
		TimeUntilNextFootstep = FootstepInterval;
	}
}

bool USoundRayEmitterComponent::ShouldAutoEmitFootstep() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	if (CharacterOwner)
	{
		const UCharacterMovementComponent* Movement = CharacterOwner->GetCharacterMovement();
		return Movement && Movement->IsMovingOnGround() && CharacterOwner->GetVelocity().Size2D() >= MinSpeedForFootsteps;
	}

	return Owner->GetVelocity().Size2D() >= MinSpeedForFootsteps;
}

void USoundRayEmitterComponent::EmitFootstepSound()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || RayCount <= 0 || Volume <= 0.0f || MaxDistance <= 0.0f)
	{
		return;
	}

	const FVector Origin = Owner->GetActorLocation();
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal();
	const int32 ClampedRayCount = FMath::Max(1, RayCount);
	const int32 RaysToEmit = bDirectionalSound ? 1 : ClampedRayCount;

	for (int32 RayIndex = 0; RayIndex < RaysToEmit; ++RayIndex)
	{
		const float AngleDegrees = 360.0f * static_cast<float>(RayIndex) / static_cast<float>(ClampedRayCount);
		const FVector FlatDirection = bDirectionalSound
			? Forward
			: FVector(FMath::Cos(FMath::DegreesToRadians(AngleDegrees)), FMath::Sin(FMath::DegreesToRadians(AngleDegrees)), 0.0f);
		const FVector Direction = FlatDirection.RotateAngleAxis(RayPitchDegrees, FVector::CrossProduct(FVector::UpVector, FlatDirection).GetSafeNormal()).GetSafeNormal();

		TraceSoundRay(Origin, Direction.GetSafeNormal(), Volume, 0);
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(World, Origin, 32.0f, 16, FColor::White, false, DebugDrawDuration, 0, 2.5f);
	}
}

void USoundRayEmitterComponent::TraceSoundRay(const FVector& RayStart, const FVector& Direction, float StartVolume, int32 ReflectionIndex) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || StartVolume <= 0.0f || MaxDistance <= 0.0f || ReflectionIndex > MaxReflections)
	{
		return;
	}

	const FVector TraceEnd = RayStart + Direction * MaxDistance;

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SoundRayTrace), false, Owner);
	const bool bHit = World->LineTraceSingleByChannel(Hit, RayStart, TraceEnd, ECC_Visibility, QueryParams);

	const FVector RayEnd = bHit ? Hit.ImpactPoint : TraceEnd;
	const float DistanceMeters = FVector::Distance(RayStart, RayEnd) / 100.0f;
	const float EndVolume = FMath::Max(0.0f, StartVolume - DistanceMeters * AttenuationPerMeter);

	NotifyListenersAlongRay(RayStart, RayEnd, StartVolume, EndVolume);

	if (bDrawDebug)
	{
		const FColor RayColor = bHit ? FColor::Orange : FColor::Yellow;
		DrawDebugLine(World, RayStart, RayEnd, RayColor, false, DebugDrawDuration, 0, 2.0f + ReflectionIndex);
		DrawDebugSphere(World, RayEnd, 18.0f, 12, bHit ? FColor::Red : FColor::Yellow, false, DebugDrawDuration, 0, 2.0f);
	}

	if (!bHit || ReflectionIndex >= MaxReflections || EndVolume <= 0.0f)
	{
		return;
	}

	const float ReflectedVolume = EndVolume * FMath::Clamp(1.0f - WallAbsorption, 0.0f, 1.0f);
	if (ReflectedVolume <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ReflectedDirection = FMath::GetReflectionVector(Direction, Hit.ImpactNormal).GetSafeNormal();
	const FVector ReflectedStart = Hit.ImpactPoint + ReflectedDirection * 4.0f;
	TraceSoundRay(ReflectedStart, ReflectedDirection, ReflectedVolume, ReflectionIndex + 1);
}

void USoundRayEmitterComponent::NotifyListenersAlongRay(const FVector& RayStart, const FVector& RayEnd, float StartVolume, float EndVolume) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Ray = RayEnd - RayStart;
	const float RayLengthSquared = Ray.SizeSquared();
	if (RayLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (const TWeakObjectPtr<UNPCHearingComponent>& ListenerPtr : UNPCHearingComponent::GetRegisteredComponents())
	{
		UNPCHearingComponent* Listener = ListenerPtr.Get();
		if (!Listener || Listener->GetWorld() != World || Listener->GetOwner() == GetOwner())
		{
			continue;
		}

		const FVector ListenerLocation = Listener->GetOwner()
			? Listener->GetOwner()->GetActorLocation()
			: FVector::ZeroVector;

		const float Alpha = FMath::Clamp(FVector::DotProduct(ListenerLocation - RayStart, Ray) / RayLengthSquared, 0.0f, 1.0f);
		const FVector ClosestPoint = RayStart + Ray * Alpha;
		const float DistanceToRay = FVector::Dist(ListenerLocation, ClosestPoint);
		if (DistanceToRay > Listener->HearingRadius)
		{
			continue;
		}

		const float ReachedVolume = FMath::Lerp(StartVolume, EndVolume, Alpha);
		if (ReachedVolume <= 0.0f)
		{
			continue;
		}

		// Use the closest point on the simulated ray as the heard position so AI can react to where the sound arrived.
		Listener->ForgetSpeed = ForgetSpeed;
		Listener->ForgetDurationSeconds = ForgetSpeed > KINDA_SMALL_NUMBER ? 1.0f / ForgetSpeed : 60.0f;
		Listener->HearSoundAtLocation(ClosestPoint, ReachedVolume);
	}
}
