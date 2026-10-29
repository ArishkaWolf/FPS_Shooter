#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NPCHearingComponent.h"
#include "SoundHearingNPC.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthNPCAnimInstance.h"
#include "StealthPlayerController.h"

namespace
{
	constexpr auto NPCFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FNPCTestWorld
	{
		UWorld* World;
		AStealthGameMode* Mission;
		AStealthPlayerController* PC;
		AStealthCharacter* Player;
		ASoundHearingNPC* Guard;
		AStealthAIController* AI;
		FNPCTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(true).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/FPS_Shooter.StealthGameMode"), TRAVEL_Absolute);
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			PC = World->SpawnActor<AStealthPlayerController>();
			Player = Spawn<AStealthCharacter>(FVector(0, 0, 200));
			PC->Possess(Player);
			Mission = World->GetAuthGameMode<AStealthGameMode>();
			World->BeginPlay();
			Player->GetCharacterMovement()->DisableMovement();
			Mission->Tick(0.01f);
			Player->SetActorLocation(FVector(-3000, 0, 200));
			Mission->Tick(0.01f);
			Guard = Spawn<ASoundHearingNPC>(FVector(0, 0, 200));
			Guard->GetCharacterMovement()->DisableMovement();
			AI = CastChecked<AStealthAIController>(Guard->GetController());
		}
		~FNPCTestWorld()
		{
			World->BeginTearingDown();
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		template<typename T> T* Spawn(const FVector& Location)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}
		void Advance(float Seconds)
		{
			for (float Elapsed = 0; Elapsed < Seconds - KINDA_SMALL_NUMBER; )
			{
				const float Step = FMath::Min(0.02f, Seconds - Elapsed);
				// Deterministic actor/component time for isolated worlds without an engine tick loop.
				if (!Guard->IsDead())
				{
					static_cast<UActorComponent*>(Guard->HearingComponent)->TickComponent(Step, LEVELTICK_All, nullptr);
					AI->Tick(Step);
				}
				static_cast<AActor*>(Guard)->Tick(Step);
				Elapsed += Step;
			}
		}
		AActor* Cover(const FVector& Location, const FVector& Extent)
		{
			AActor* Actor = Spawn<AActor>(Location);
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Actor->AddInstanceComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Box->RegisterComponent();
			Box->SetWorldLocation(Location);
			return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCWeakSoundTest, "FPS_Shooter.NPC.WeakSoundSuspicionCycle", NPCFlags)
bool FNPCWeakSoundTest::RunTest(const FString& Parameters)
{
	FNPCTestWorld Scene;
	TestEqual(TEXT("Initial Patrol"), Scene.AI->GetAIState(), EStealthAIState::Patrol);
	Scene.Guard->HearingComponent->HearSoundAtLocation(FVector(500, 200, 200), 0.08f);
	Scene.Advance(0.02f);
	TestEqual(TEXT("Patrol -> Suspicious on weak sound"), Scene.AI->GetAIState(), EStealthAIState::Suspicious);
	TestFalse(TEXT("Unconfirmed sound does not block extraction"), Scene.Mission->IsPlayerPursued());
	TestTrue(TEXT("Suspicion stops and turns toward sound"), Scene.Guard->GetVelocity().IsNearlyZero()
		&& Scene.Guard->GetCharacterMovement()->bUseControllerDesiredRotation);
	Scene.Advance(1.7f);
	TestEqual(TEXT("Suspicious -> Investigate after listening"), Scene.AI->GetAIState(), EStealthAIState::Investigate);
	Scene.Advance(7.1f);
	TestEqual(TEXT("Investigate -> Search on unreachable destination timeout"), Scene.AI->GetAIState(), EStealthAIState::Search);
	Scene.Advance(1.5f);
	TestEqual(TEXT("Search -> Patrol"), Scene.AI->GetAIState(), EStealthAIState::Patrol);
	Scene.Advance(2.0f);
	TestEqual(TEXT("Old sound cannot restart completed search"), Scene.AI->GetAIState(), EStealthAIState::Patrol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCPartialSightTest, "FPS_Shooter.NPC.PartialSightSuspicionCycle", NPCFlags)
bool FNPCPartialSightTest::RunTest(const FString& Parameters)
{
	FNPCTestWorld Scene;
	Scene.Player->SetActorLocation(FVector(600, 0, 200));
	Scene.Cover(FVector(300, 0, 150), FVector(30, 150, 90));
	FHitResult Hit;
	FCollisionQueryParams SightParams(SCENE_QUERY_STAT(NPCTestSight), true, Scene.Guard);
	SightParams.AddIgnoredActor(Scene.Player);
	TestTrue(TEXT("Low body sample is occluded"), Scene.World->LineTraceSingleByChannel(Hit,
		FVector(0, 0, 270), FVector(600, 0, 175), ECC_Visibility, SightParams));
	TestFalse(TEXT("Head sample is exposed"), Scene.World->LineTraceSingleByChannel(Hit,
		FVector(0, 0, 270), FVector(600, 0, 260), ECC_Visibility, SightParams));
	Scene.Advance(0.6f);
	TestEqual(TEXT("Patrol -> Suspicious from partial visual exposure"), Scene.AI->GetAIState(), EStealthAIState::Suspicious);
	TestTrue(TEXT("Partial detection remains below confirmation"), Scene.AI->GetDetectionLevel() > 0.12f && Scene.AI->GetDetectionLevel() < 1.0f);
	const FVector Observed = Scene.AI->GetLastKnownPlayerLocation();
	Scene.Player->SetActorLocation(FVector(-3000, 0, 200));
	Scene.Advance(1.7f);
	TestEqual(TEXT("Lost glimpse -> Investigate"), Scene.AI->GetAIState(), EStealthAIState::Investigate);
	TestTrue(TEXT("Visual investigation uses observation, not hidden player"), Scene.AI->GetInvestigationLocation().Equals(Observed));
	Scene.Advance(7.1f);
	TestEqual(TEXT("Visual Investigate -> Search"), Scene.AI->GetAIState(), EStealthAIState::Search);
	Scene.Advance(1.5f);
	TestEqual(TEXT("Visual Search -> Patrol"), Scene.AI->GetAIState(), EStealthAIState::Patrol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSoundPriorityTest, "FPS_Shooter.NPC.SoundPriorityAndStrongSoundCycle", NPCFlags)
bool FNPCSoundPriorityTest::RunTest(const FString& Parameters)
{
	FNPCTestWorld Scene;
	UNPCHearingComponent* Hearing = Scene.Guard->HearingComponent;
	Hearing->HearSoundAtLocation(FVector(1200, 0, 200), 0.9f);
	Hearing->HearSoundAtLocation(FVector(200, 200, 200), 0.3f);
	FNPCHearingStimulus Best;
	TestTrue(TEXT("Evidence retained"), Hearing->GetBestSound(Best));
	TestEqual(TEXT("Strength outweighs proximity for a much louder sound"), Best.Strength, 0.9f);
	Scene.Advance(0.02f);
	TestEqual(TEXT("Patrol -> Investigate for strong sound"), Scene.AI->GetAIState(), EStealthAIState::Investigate);
	Hearing->HearSoundAtLocation(FVector(400, 200, 200), 0.8f);
	Scene.Advance(0.02f);
	TestTrue(TEXT("Closer, slightly quieter sound wins by priority"), Scene.AI->GetInvestigationLocation().Equals(FVector(400, 200, 200)));
	const int32 BeforeReflections = Hearing->HearingEventId;
	for (int32 Index = 0; Index < 30; ++Index) { Hearing->HearSoundAtLocation(FVector(410, 200, 200), 0.4f); }
	TestEqual(TEXT("Nearby weaker reflected rays are coalesced"), Hearing->HearingEventId, BeforeReflections);
	Hearing->HearSoundAtLocation(FVector(-600, 0, 200), 0.05f);
	Scene.Advance(0.02f);
	TestTrue(TEXT("Weak distraction cannot steal a strong investigation"), Scene.AI->GetInvestigationLocation().Equals(FVector(400, 200, 200)));
	Scene.Advance(6.0f);
	Hearing->HearSoundAtLocation(FVector(200, -200, 200), 0.5f);
	Scene.Advance(0.02f);
	TestTrue(TEXT("Recent moderate sound outranks old loud evidence"), Scene.AI->GetInvestigationLocation().Equals(FVector(200, -200, 200)));
	Scene.Advance(7.1f);
	TestEqual(TEXT("Strong sound Investigate -> Search"), Scene.AI->GetAIState(), EStealthAIState::Search);
	Scene.Advance(1.5f);
	TestEqual(TEXT("Strong sound Search -> Patrol"), Scene.AI->GetAIState(), EStealthAIState::Patrol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCBurstChaseTest, "FPS_Shooter.NPC.ChaseBurstsAndEscapeCycle", NPCFlags)
bool FNPCBurstChaseTest::RunTest(const FString& Parameters)
{
	FNPCTestWorld Scene;
	Scene.Guard->FireDamage = 0.0f;
	Scene.Player->SetActorLocation(FVector(600, 0, 200));
	Scene.AI->Tick(3.0f);
	TestEqual(TEXT("Patrol -> Chase after confirmed sight"), Scene.AI->GetAIState(), EStealthAIState::Chase);
	TestEqual(TEXT("No immediate fire at detection"), Scene.Guard->GetShotsFired(), 0);
	Scene.Guard->SetActorRotation(FRotator(0, 20, 0));
	Scene.AI->Tick(0.79f);
	TestEqual(TEXT("Reaction delay maintained"), Scene.Guard->GetShotsFired(), 0);
	Scene.AI->Tick(0.02f);
	TestEqual(TEXT("Guard must turn toward target before shooting"), Scene.Guard->GetShotsFired(), 0);
	Scene.Guard->SetActorRotation(FRotator::ZeroRotator);
	Scene.AI->Tick(0.01f);
	Scene.AI->Tick(0.17f);
	Scene.AI->Tick(0.17f);
	TestEqual(TEXT("Controlled burst contains three shots"), Scene.Guard->GetShotsFired(), 3);
	Scene.AI->Tick(0.7f);
	TestEqual(TEXT("Pause between bursts"), Scene.Guard->GetShotsFired(), 3);
	Scene.AI->Tick(0.71f);
	TestEqual(TEXT("Next burst starts after pause"), Scene.Guard->GetShotsFired(), 4);
	Scene.Cover(FVector(300, 0, 250), FVector(30, 200, 250));
	Scene.AI->Tick(0.2f);
	TestEqual(TEXT("Losing visibility cancels the remaining burst"), Scene.Guard->GetShotsFired(), 4);
	const FVector Observed = Scene.AI->GetLastKnownPlayerLocation();
	Scene.Player->SetActorLocation(FVector(5000, 0, 200));
	Scene.AI->Tick(2.3f);
	TestEqual(TEXT("Chase -> Search after lost sight delay"), Scene.AI->GetAIState(), EStealthAIState::Search);
	TestTrue(TEXT("Search origin is last observation"), Scene.AI->GetInvestigationLocation().Equals(Observed));
	TestTrue(TEXT("Confirmed Search blocks extraction"), Scene.Mission->IsPlayerPursued());
	Scene.AI->Tick(1.5f);
	TestEqual(TEXT("Chase search -> Patrol"), Scene.AI->GetAIState(), EStealthAIState::Patrol);
	TestFalse(TEXT("Pursuit clears after search"), Scene.Mission->IsPlayerPursued());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCCombatTest, "FPS_Shooter.NPC.SpreadDamageAnimationAndDeath", NPCFlags)
bool FNPCCombatTest::RunTest(const FString& Parameters)
{
	FNPCTestWorld Scene;
	Scene.Player->SetActorLocation(FVector(600, 0, 200));
	Scene.Player->GetMesh()->TickAnimation(0.0f, false);
	Scene.Player->GetMesh()->RefreshBoneTransforms();
	Scene.Guard->FireDamage = 0.0f;
	TestNotNull(TEXT("Native NPC animation graph active"), Cast<UStealthNPCAnimInstance>(Scene.Guard->GetMesh()->GetAnimInstance()));
	for (UAnimSequence* Clip : { Scene.Guard->GetIdleAnimation(), Scene.Guard->GetWalkAnimation(), Scene.Guard->GetRunAnimation(),
		Scene.Guard->GetFireAnimation(), Scene.Guard->GetHitAnimation(), Scene.Guard->GetDeathAnimation() })
	{
		if (TestNotNull(TEXT("Existing NPC animation available"), Clip))
		{
			TestEqual(TEXT("Animation uses Manny skeleton"), Clip->GetSkeleton(), Scene.Guard->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton());
		}
	}
	FVector FirstDirection;
	bool bVaried = false;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		TestTrue(TEXT("Alive guard fires"), Scene.Guard->FireWeaponAt(Scene.Player));
		const FVector Direction = Scene.Guard->GetLastShotDirection();
		if (Index == 0) { FirstDirection = Direction; }
		else { bVaried |= !Direction.Equals(FirstDirection, 0.00001f); }
		TestTrue(TEXT("Spread stays inside configured cone"), FVector::DotProduct(Direction, Scene.Guard->GetLastShotAimDirection())
			>= FMath::Cos(FMath::DegreesToRadians(Scene.Guard->FireSpreadDegrees)) - 0.00001f);
	}
	TestTrue(TEXT("Spread varies between shots"), bVaried);
	TestEqual(TEXT("Firing triggers recoil clip"), Scene.Guard->GetFireAnimationAge(), 0.0f);
	Scene.Guard->FireDamage = 14.0f;
	Scene.Guard->FireSpreadDegrees = 0.0f;
	Scene.Guard->FireWeaponAt(Scene.Player);
	TestTrue(TEXT("Unobstructed gunfire damages health"), Scene.Player->GetHealth() < 100.0f && Scene.Player->GetHealth() > 0.0f);
	AActor* CoverActor = Scene.Cover(FVector(300, 0, 250), FVector(30, 200, 250));
	const float HealthBehindCover = Scene.Player->GetHealth();
	Scene.Guard->FireWeaponAt(Scene.Player);
	TestEqual(TEXT("Muzzle trace respects cover"), Scene.Player->GetHealth(), HealthBehindCover);
	UGameplayStatics::ApplyDamage(Scene.Guard, -10, Scene.PC, Scene.Player, UDamageType::StaticClass());
	TestEqual(TEXT("Negative damage cannot heal"), Scene.Guard->GetHealth(), 100.0f);
	UGameplayStatics::ApplyDamage(Scene.Guard, 30, Scene.PC, Scene.Player, UDamageType::StaticClass());
	TestEqual(TEXT("Nonlethal damage consumes guard health"), Scene.Guard->GetHealth(), 70.0f);
	TestEqual(TEXT("Damage triggers hit clip"), Scene.Guard->GetHitAnimationAge(), 0.0f);
	TestEqual(TEXT("Unseen attacker triggers investigation"), Scene.AI->GetAIState(), EStealthAIState::Investigate);
	CoverActor->SetActorEnableCollision(false);
	Scene.AI->Tick(3.0f);
	TestTrue(TEXT("Visible attacker confirms pursuit before lethal hit"), Scene.AI->HasActivePursuit());
	UGameplayStatics::ApplyDamage(Scene.Guard, 1000, Scene.PC, Scene.Player, UDamageType::StaticClass());
	TestTrue(TEXT("Lethal damage enters death state"), Scene.Guard->IsDead());
	TestEqual(TEXT("Health clamps at zero"), Scene.Guard->GetHealth(), 0.0f);
	TestFalse(TEXT("Death leaves visible actor for animation"), Scene.Guard->IsActorBeingDestroyed());
	TestFalse(TEXT("Corpse does not block movement"), Scene.Guard->GetActorEnableCollision());
	TestNull(TEXT("Dead guard is unpossessed"), Scene.Guard->GetController());
	TestFalse(TEXT("Dead guard cannot retain pursuit"), Scene.AI->HasActivePursuit());
	TestFalse(TEXT("Dead guard cannot fire"), Scene.Guard->FireWeaponAt(Scene.Player));
	const int32 HeardBefore = Scene.Guard->HearingComponent->HearingEventId;
	Scene.Guard->HearingComponent->HearSoundAtLocation(Scene.Player->GetActorLocation(), 1.0f);
	TestEqual(TEXT("Dead guard ignores sound rays"), Scene.Guard->HearingComponent->HearingEventId, HeardBefore);
	Scene.Advance(0.3f);
	TestTrue(TEXT("Death clip advances while AI stopped"), Scene.Guard->GetDeathAnimationAge() >= 0.29f);
	const FTransform SpawnTransform(FVector(1500, 0, 200));
	ASoundHearingNPC* ToughGuard = Scene.World->SpawnActorDeferred<ASoundHearingNPC>(ASoundHearingNPC::StaticClass(),
		SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	ToughGuard->MaxHealth = 200.0f;
	ToughGuard->FinishSpawning(SpawnTransform);
	TestEqual(TEXT("Placed instance MaxHealth initializes at BeginPlay"), ToughGuard->GetHealth(), 200.0f);
	return true;
}

#endif
