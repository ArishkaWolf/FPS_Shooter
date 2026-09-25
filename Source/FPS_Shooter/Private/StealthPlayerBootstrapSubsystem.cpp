#include "StealthPlayerBootstrapSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "StealthCharacter.h"

void UStealthPlayerBootstrapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsNetMode(NM_DedicatedServer) || (InWorld.WorldType != EWorldType::PIE && InWorld.WorldType != EWorldType::Game))
	{
		return;
	}

	APlayerController* PlayerController = InWorld.GetFirstPlayerController();
	APawn* ExistingPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!PlayerController || !ExistingPawn || ExistingPawn->IsA<AStealthCharacter>())
	{
		return;
	}

	const FTransform SpawnTransform = ExistingPawn->GetActorTransform();
	PlayerController->UnPossess();
	ExistingPawn->Destroy();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerController;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AStealthCharacter* Character = InWorld.SpawnActor<AStealthCharacter>(AStealthCharacter::StaticClass(), SpawnTransform, SpawnParams))
	{
		PlayerController->Possess(Character);
	}
}
