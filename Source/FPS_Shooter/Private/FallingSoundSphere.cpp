#include "FallingSoundSphere.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "SoundRayEmitterComponent.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<int32> CVarFallingSphereRayCount(
	TEXT("SoundRayDemo.RayCount"),
	16,
	TEXT("Number of sound rays emitted by the falling demo sphere."));

AFallingSoundSphere::AFallingSoundSphere()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(50.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionComponent->SetSimulatePhysics(true);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	SetRootComponent(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
	}

	SoundEmitter = CreateDefaultSubobject<USoundRayEmitterComponent>(TEXT("SoundEmitter"));
	SoundEmitter->bAutoEmitFootsteps = false;
	SoundEmitter->bDirectionalSound = false;
	SoundEmitter->Volume = 1.15f;
	SoundEmitter->RayCount = 16;
	SoundEmitter->AttenuationPerMeter = 0.04f;
	SoundEmitter->WallAbsorption = 0.35f;
	SoundEmitter->RayPitchDegrees = 0.0f;
	SoundEmitter->ForgetSpeed = 1.0f / 60.0f;
	SoundEmitter->MaxDistance = 2200.0f;
	SoundEmitter->MaxReflections = 4;
	SoundEmitter->bDrawDebug = true;
	SoundEmitter->DebugDrawDuration = 60.0f;
}

void AFallingSoundSphere::BeginPlay()
{
	Super::BeginPlay();
	CollisionComponent->OnComponentHit.AddDynamic(this, &AFallingSoundSphere::OnSphereHit);
}

void AFallingSoundSphere::OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bEmittedSound || NormalImpulse.Size() < 50.0f)
	{
		return;
	}

	bEmittedSound = true;
	SoundEmitter->RayCount = FMath::Max(1, CVarFallingSphereRayCount.GetValueOnGameThread());
	SoundEmitter->EmitFootstepSound();
}
