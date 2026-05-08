#include "SoundRayDemoSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "FallingSoundSphere.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SoundHearingNPC.h"

void USoundRayDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsNetMode(NM_DedicatedServer) || (InWorld.WorldType != EWorldType::PIE && InWorld.WorldType != EWorldType::Game))
	{
		return;
	}

	SpawnDemo(InWorld);
}

void USoundRayDemoSubsystem::SpawnDemo(UWorld& InWorld) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(&InWorld, 0);
	const FVector BaseLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const FVector Forward = PlayerPawn ? PlayerPawn->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	const FVector Right = PlayerPawn ? PlayerPawn->GetActorRightVector().GetSafeNormal2D() : FVector::RightVector;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SoundDemoSpawnTrace), false, PlayerPawn);
	const auto SnapToGround = [&InWorld, &QueryParams](const FVector& DesiredLocation, float HalfHeight)
	{
		FHitResult Hit;
		const FVector TraceStart = DesiredLocation + FVector(0.0f, 0.0f, 1000.0f);
		const FVector TraceEnd = DesiredLocation - FVector(0.0f, 0.0f, 3000.0f);
		if (InWorld.LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			return Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight);
		}

		return DesiredLocation;
	};

	const FVector SourceGroundLocation = SnapToGround(BaseLocation + Forward * 520.0f, 0.0f);
	const FVector SourceLocation = SourceGroundLocation + FVector(0.0f, 0.0f, 520.0f);
	InWorld.SpawnActor<AFallingSoundSphere>(AFallingSoundSphere::StaticClass(), SourceLocation, FRotator::ZeroRotator);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	const auto SpawnWall = [&InWorld, &SnapToGround, CubeMesh](const FVector& GroundCenter, float YawDegrees, const FVector& Dimensions)
	{
		if (!CubeMesh)
		{
			return;
		}

		const FVector Location = SnapToGround(GroundCenter, Dimensions.Z * 0.5f);
		AStaticMeshActor* Wall = InWorld.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, FRotator(0.0f, YawDegrees, 0.0f));
		if (!Wall || !Wall->GetStaticMeshComponent())
		{
			return;
		}

		Wall->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
		Wall->GetStaticMeshComponent()->SetWorldScale3D(Dimensions / 100.0f);
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->GetStaticMeshComponent()->SetGenerateOverlapEvents(false);
	};

	SpawnWall(SourceGroundLocation + Forward * 600.0f, Forward.Rotation().Yaw + 90.0f, FVector(900.0f, 80.0f, 320.0f));
	SpawnWall(SourceGroundLocation - Forward * 520.0f - Right * 380.0f, Forward.Rotation().Yaw + 35.0f, FVector(850.0f, 80.0f, 280.0f));
	SpawnWall(SourceGroundLocation + Forward * 820.0f + Right * 740.0f, Forward.Rotation().Yaw - 25.0f, FVector(700.0f, 80.0f, 300.0f));
	SpawnWall(SourceGroundLocation - Forward * 840.0f + Right * 780.0f, Forward.Rotation().Yaw + 110.0f, FVector(780.0f, 80.0f, 300.0f));

	struct FDemoNPCSpawn
	{
		FVector Offset;
		const TCHAR* Label;
	};

	const FDemoNPCSpawn NPCSpawns[5] =
	{
		{ Forward * 1150.0f - Right * 850.0f, TEXT("Open corner") },
		{ Forward * 1150.0f + Right * 850.0f, TEXT("Behind wall") },
		{ -Forward * 1050.0f - Right * 850.0f, TEXT("Rear left") },
		{ Forward * 1700.0f + Right * 1350.0f, TEXT("Far double block") },
		{ -Forward * 1450.0f + Right * 1300.0f, TEXT("Rear obstacle") }
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NPCSpawns); ++Index)
	{
		const FVector Location = SnapToGround(SourceGroundLocation + NPCSpawns[Index].Offset, 96.0f);
		const FRotator Rotation = (-NPCSpawns[Index].Offset).Rotation();
		ASoundHearingNPC* NPC = InWorld.SpawnActor<ASoundHearingNPC>(ASoundHearingNPC::StaticClass(), Location, FRotator(0.0f, Rotation.Yaw, 0.0f));
		if (NPC)
		{
			NPC->NPCLabel = FText::FromString(NPCSpawns[Index].Label);
		}
	}
}
