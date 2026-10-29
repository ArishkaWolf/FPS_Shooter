#include "SoundRayDemoSubsystem.h"

#include "Animation/AnimationAsset.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "FallingSoundSphere.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "SoundHearingNPC.h"

namespace
{
TAutoConsoleVariable<int32> CVarSoundRayDemoEnabled(
	TEXT("SoundRayDemo.Enabled"),
	0,
	TEXT("Spawns the standalone sound-ray demonstration scene when set to 1."));

bool ContainsDiplomaHiddenWeaponToken(const FString& Text)
{
	static const TCHAR* WeaponTokens[] =
	{
		TEXT("Weapon"),
		TEXT("Rifle"),
		TEXT("Pistol"),
		TEXT("Shotgun"),
		TEXT("Sniper"),
		TEXT("Launcher"),
		TEXT("Grenade"),
		TEXT("Knife"),
		TEXT("Ammo"),
		TEXT("Gun")
	};

	for (const TCHAR* Token : WeaponTokens)
	{
		if (Text.Contains(Token, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool IsDiplomaHiddenWeaponComponent(const UActorComponent* Component)
{
	if (!Component)
	{
		return false;
	}

	if (ContainsDiplomaHiddenWeaponToken(Component->GetName()))
	{
		return true;
	}

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
		const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		return StaticMesh && ContainsDiplomaHiddenWeaponToken(StaticMesh->GetPathName());
	}

	if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component))
	{
		const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
		return SkeletalMesh && ContainsDiplomaHiddenWeaponToken(SkeletalMesh->GetPathName());
	}

	return false;
}

void HideDiplomaWeaponVisuals(AActor& Actor)
{
	if (ContainsDiplomaHiddenWeaponToken(Actor.GetName()))
	{
		Actor.SetActorHiddenInGame(true);
		Actor.SetActorEnableCollision(false);
	}

	TArray<UActorComponent*> Components;
	Actor.GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (!IsDiplomaHiddenWeaponComponent(Component))
		{
			continue;
		}

		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			SceneComponent->SetHiddenInGame(true, true);
			SceneComponent->SetVisibility(false, true);
		}

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void SetDiplomaNeutralPose(ACharacter& Character)
{
	USkeletalMeshComponent* Mesh = Character.GetMesh();
	if (!Mesh)
	{
		return;
	}

	const USkeletalMesh* SkeletalMesh = Mesh->GetSkeletalMeshAsset();
	const FString MeshName = SkeletalMesh ? SkeletalMesh->GetName() : FString();
	const TCHAR* IdleAnimationPath = MeshName.Contains(TEXT("Quinn"), ESearchCase::IgnoreCase)
		? TEXT("/Game/Characters/Mannequins/Animations/Quinn/MF_Idle.MF_Idle")
		: TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle");

	if (UAnimationAsset* IdleAnimation = LoadObject<UAnimationAsset>(nullptr, IdleAnimationPath))
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->SetAnimation(IdleAnimation);
		Mesh->SetPosition(0.0f, false);
		Mesh->bPauseAnims = true;
	}
	else
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->SetAnimation(nullptr);
		Mesh->bPauseAnims = true;
	}
}

void PrepareWorldForDiplomaScreenshots(UWorld& World)
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		HideDiplomaWeaponVisuals(*Actor);

		if (ACharacter* Character = Cast<ACharacter>(Actor))
		{
			SetDiplomaNeutralPose(*Character);
		}
	}
}
}

void USoundRayDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsNetMode(NM_DedicatedServer) || (InWorld.WorldType != EWorldType::PIE && InWorld.WorldType != EWorldType::Game))
	{
		return;
	}

	if (CVarSoundRayDemoEnabled.GetValueOnGameThread() == 0)
	{
		return;
	}

	SpawnDemo(InWorld);
}

void USoundRayDemoSubsystem::SpawnDemo(UWorld& InWorld) const
{
	static const FName DemoActorTag(TEXT("SoundRayDemo"));

	for (TActorIterator<ASoundHearingNPC> It(&InWorld); It; ++It)
	{
		It->Destroy();
	}

	for (TActorIterator<AFallingSoundSphere> It(&InWorld); It; ++It)
	{
		It->Destroy();
	}

	for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
	{
		if (It->Tags.Contains(DemoActorTag))
		{
			It->Destroy();
		}
	}

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
	AFallingSoundSphere* SoundSphere = InWorld.SpawnActor<AFallingSoundSphere>(AFallingSoundSphere::StaticClass(), SourceLocation, FRotator(0.0f, Forward.Rotation().Yaw, 0.0f));
	if (SoundSphere)
	{
		SoundSphere->Tags.AddUnique(DemoActorTag);
	}

	if (APlayerController* PlayerController = InWorld.GetFirstPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("stat fps"), true);
	}

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

		Wall->Tags.AddUnique(DemoActorTag);
		Wall->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
		Wall->GetStaticMeshComponent()->SetWorldScale3D(Dimensions / 100.0f);
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->GetStaticMeshComponent()->SetGenerateOverlapEvents(false);
	};

	SpawnWall(SourceGroundLocation + Forward * 700.0f, Forward.Rotation().Yaw + 90.0f, FVector(560.0f, 80.0f, 320.0f));
	SpawnWall(SourceGroundLocation + Forward * 520.0f + Right * 520.0f, Forward.Rotation().Yaw, FVector(900.0f, 80.0f, 320.0f));
	SpawnWall(SourceGroundLocation + Forward * 1160.0f + Right * 940.0f, Forward.Rotation().Yaw + 90.0f, FVector(680.0f, 80.0f, 300.0f));
	SpawnWall(SourceGroundLocation + Forward * 1320.0f + Right * 1320.0f, Forward.Rotation().Yaw, FVector(620.0f, 80.0f, 300.0f));
	SpawnWall(SourceGroundLocation - Forward * 520.0f + Right * 520.0f, Forward.Rotation().Yaw + 45.0f, FVector(760.0f, 80.0f, 300.0f));
	SpawnWall(SourceGroundLocation - Forward * 720.0f - Right * 250.0f, Forward.Rotation().Yaw - 20.0f, FVector(660.0f, 80.0f, 280.0f));

	struct FDemoNPCSpawn
	{
		FVector Offset;
		const TCHAR* Label;
	};

	const FDemoNPCSpawn NPCSpawns[8] =
	{
		{ Forward * 120.0f, TEXT("Near sphere") },
		{ Forward * 420.0f, TEXT("Open farther") },
		{ Forward * 980.0f, TEXT("Blocked wall") },
		{ Forward * 820.0f + Right * 220.0f, TEXT("Reflected wall") },
		{ -Forward * 700.0f - Right * 700.0f, TEXT("Rear left") },
		{ -Right * 1250.0f, TEXT("Side open") },
		{ Forward * 1400.0f + Right * 1200.0f, TEXT("Far double block") },
		{ -Forward * 920.0f + Right * 920.0f, TEXT("Rear obstacle") }
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NPCSpawns); ++Index)
	{
		const FVector Location = SnapToGround(SourceGroundLocation + NPCSpawns[Index].Offset, 96.0f);
		const FRotator Rotation = (-NPCSpawns[Index].Offset).Rotation();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASoundHearingNPC* NPC = InWorld.SpawnActor<ASoundHearingNPC>(ASoundHearingNPC::StaticClass(), Location, FRotator(0.0f, Rotation.Yaw, 0.0f), SpawnParameters);

		if (NPC)
		{
			NPC->Tags.AddUnique(DemoActorTag);
			NPC->NPCLabel = FText::FromString(NPCSpawns[Index].Label);
		}
	}

	PrepareWorldForDiplomaScreenshots(InWorld);
}

void USoundRayDemoSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DeltaTime <= KINDA_SMALL_NUMBER || !GEngine)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->IsNetMode(NM_DedicatedServer) || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
	{
		return;
	}

	if (CVarSoundRayDemoEnabled.GetValueOnGameThread() == 0)
	{
		return;
	}

	const float FPS = 1.0f / DeltaTime;
	GEngine->AddOnScreenDebugMessage(1729, 0.0f, FColor::Green, FString::Printf(TEXT("FPS: %.1f"), FPS));
}

TStatId USoundRayDemoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USoundRayDemoSubsystem, STATGROUP_Tickables);
}
