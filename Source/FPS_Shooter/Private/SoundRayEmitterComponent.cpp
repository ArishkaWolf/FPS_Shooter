#include "SoundRayEmitterComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
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
	EmitSoundEvent(ESoundEventType::Footstep);
}

void USoundRayEmitterComponent::EmitSoundEvent(ESoundEventType Type, float VolumeOverride, float DistanceOverride, AActor* SourceOwner)
{
	if (GetOwner()) { EmitSoundEventAtLocation(Type, GetOwner()->GetActorLocation(), VolumeOverride, DistanceOverride, SourceOwner); }
}

void USoundRayEmitterComponent::EmitSoundEventAtLocation(ESoundEventType Type, FVector Location,
	float VolumeOverride, float DistanceOverride, AActor* SourceOwner)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	const float EventVolume = VolumeOverride < 0.0f ? Volume : VolumeOverride;
	const float RayDistance = DistanceOverride < 0.0f ? MaxDistance : DistanceOverride;
	if (!World || !Owner || RayCount <= 0 || !FMath::IsFinite(EventVolume) || EventVolume <= 0.0f
		|| !FMath::IsFinite(RayDistance) || RayDistance <= 0.0f || Location.ContainsNaN())
	{
		return;
	}

	FSoundRayEvent Event;
	Event.EventId = FGuid::NewGuid();
	Event.Type = Type;
	Event.Strength = Event.EmittedStrength = EventVolume;
	Event.SourceLocation = Event.HeardLocation = Event.ArrivalLocation = Location;
	Event.TimeSeconds = World->GetTimeSeconds();
	Event.SourceActor = Owner;
	AActor* ResponsibleActor = IsValid(SourceOwner) ? SourceOwner
		: Cast<APawn>(Owner) ? Owner
		: IsValid(Owner->GetInstigator()) ? Owner->GetInstigator()
		: IsValid(Owner->GetOwner()) ? Owner->GetOwner() : Owner;
	// A possessed pawn's AActor::Owner is its controller, not the physical sound source.
	if (AController* SourceController = Cast<AController>(ResponsibleActor); SourceController && SourceController->GetPawn())
	{
		ResponsibleActor = SourceController->GetPawn();
	}
	Event.SourceOwner = ResponsibleActor;
	Event.MemoryDuration = ForgetSpeed > KINDA_SMALL_NUMBER ? 1.0f / ForgetSpeed : 60.0f;
	Event.RayDistance = RayDistance;
	LastEmittedEvent = Event;
	const FVector Origin = Location;
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal();
	const float OwnerYawDegrees = Owner->GetActorRotation().Yaw;
	const int32 ClampedRayCount = FMath::Max(1, RayCount);
	const int32 RaysToEmit = bDirectionalSound ? 1 : ClampedRayCount;

	for (int32 RayIndex = 0; RayIndex < RaysToEmit; ++RayIndex)
	{
		const float AngleDegrees = OwnerYawDegrees + 360.0f * static_cast<float>(RayIndex) / static_cast<float>(ClampedRayCount);
		const FVector FlatDirection = bDirectionalSound
			? Forward
			: FVector(FMath::Cos(FMath::DegreesToRadians(AngleDegrees)), FMath::Sin(FMath::DegreesToRadians(AngleDegrees)), 0.0f);
		const FVector Direction = FlatDirection.RotateAngleAxis(RayPitchDegrees, FVector::CrossProduct(FVector::UpVector, FlatDirection).GetSafeNormal()).GetSafeNormal();

		TraceSoundRay(Origin, Direction.GetSafeNormal(), EventVolume, 0, Event, RayDistance);
	}

	if (IsSoundRayDebugEnabled())
	{
		DrawDebugSphere(World, Origin, 32.0f, 16, FColor::White, false, DebugDrawDuration, 0, 2.5f);
	}
}

void USoundRayEmitterComponent::TraceSoundRay(const FVector& RayStart, const FVector& Direction, float StartVolume, int32 ReflectionIndex,
	const FSoundRayEvent& Event, float RayDistance) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || StartVolume <= 0.0f || RayDistance <= 0.0f || ReflectionIndex > MaxReflections)
	{
		return;
	}

	const FVector TraceEnd = RayStart + Direction * RayDistance;

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SoundRayTrace), false, Owner);
	const bool bHit = World->LineTraceSingleByChannel(Hit, RayStart, TraceEnd, ECC_Visibility, QueryParams);

	const FVector RayEnd = bHit ? Hit.ImpactPoint : TraceEnd;
	const float DistanceMeters = FVector::Distance(RayStart, RayEnd) / 100.0f;
	const float EndVolume = FMath::Max(0.0f, StartVolume - DistanceMeters * AttenuationPerMeter);

	NotifyListenersAlongRay(RayStart, RayEnd, StartVolume, EndVolume, Event, ReflectionIndex);

	if (IsSoundRayDebugEnabled())
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
	TraceSoundRay(ReflectedStart, ReflectedDirection, ReflectedVolume, ReflectionIndex + 1, Event, RayDistance);
}

void USoundRayEmitterComponent::NotifyListenersAlongRay(const FVector& RayStart, const FVector& RayEnd, float StartVolume, float EndVolume,
	const FSoundRayEvent& Event, int32 ReflectionIndex) const
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
		if (!Listener || !Listener->IsActive() || Listener->GetWorld() != World || Listener->GetOwner() == GetOwner())
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
		// A hearing radius must not let a listener on the far side of a wall hear through it.
		FHitResult OcclusionHit;
		FCollisionQueryParams HearingParams(SCENE_QUERY_STAT(SoundRayArrival), false, GetOwner());
		HearingParams.AddIgnoredActor(Listener->GetOwner());
		if (World->LineTraceSingleByChannel(OcclusionHit, ClosestPoint, ListenerLocation, ECC_Visibility, HearingParams)) { continue; }

		const float ReachedVolume = FMath::Lerp(StartVolume, EndVolume, Alpha);
		if (ReachedVolume <= 0.0f)
		{
			continue;
		}

		FSoundRayEvent Received = Event;
		Received.Strength = ReachedVolume;
		Received.HeardLocation = RayStart;
		Received.ArrivalLocation = ClosestPoint;
		Received.ReflectionCount = ReflectionIndex;
		Listener->HearSoundEvent(Received);
	}
}
