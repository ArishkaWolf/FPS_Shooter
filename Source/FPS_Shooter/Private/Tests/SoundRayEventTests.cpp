#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NPCHearingComponent.h"
#include "NoiseMakerPickup.h"
#include "SoundHearingNPC.h"
#include "SoundRayEmitterComponent.h"
#include "SoundRayInteractionComponent.h"
#include "StealthAIController.h"
#include "StealthCharacter.h"
#include "StealthDoor.h"
#include "StealthGameMode.h"
#include "StealthObjective.h"
#include "StealthPlayerController.h"

namespace
{
	constexpr auto SoundFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FSoundTestWorld
	{
		UWorld* World;
		AStealthCharacter* Player;
		AStealthPlayerController* PC;
		ASoundHearingNPC* Guard;
		AStealthGameMode* Mission;
		FSoundTestWorld()
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
			Player = Spawn<AStealthCharacter>(FVector(-3000, 0, 200));
			PC->Possess(Player);
			Mission = World->GetAuthGameMode<AStealthGameMode>();
			World->BeginPlay();
			Player->GetCharacterMovement()->DisableMovement();
			Mission->Tick(0.01f);
			Player->SetActorLocation(FVector(-2000, 0, 200));
			Mission->Tick(0.01f);
			Guard = Spawn<ASoundHearingNPC>(FVector(300, 0, 200));
			Guard->GetCharacterMovement()->DisableMovement();
			Guard->GetController()->SetActorTickEnabled(false);
			Guard->HearingComponent->HearingRadius = 30.0f;
		}
		~FSoundTestWorld()
		{
			World->BeginTearingDown();
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		template<typename T> T* Spawn(FVector Location)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), Location, FRotator::ZeroRotator, Params);
		}
		AActor* Source(FVector Location)
		{
			AActor* Actor = Spawn<AActor>(Location);
			USceneComponent* Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root); Actor->AddInstanceComponent(Root); Root->RegisterComponent();
			Actor->SetActorLocation(Location);
			return Actor;
		}
		USoundRayEmitterComponent* Emitter(AActor* Source)
		{
			USoundRayEmitterComponent* Component = NewObject<USoundRayEmitterComponent>(Source);
			Source->AddInstanceComponent(Component);
			Component->bAutoEmitFootsteps = false; Component->bDirectionalSound = true;
			Component->RayPitchDegrees = 0; Component->MaxDistance = 1000;
			Component->AttenuationPerMeter = 0.04f; Component->MaxReflections = 0;
			Component->RegisterComponent();
			return Component;
		}
		void Wall(FVector Location, FVector Extent)
		{
			AActor* Actor = Source(Location);
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box); Box->SetupAttachment(Actor->GetRootComponent());
			Box->SetBoxExtent(Extent); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Box->RegisterComponent();
		}
		FSoundRayEvent Event(ESoundEventType Type, float Strength, FVector Position, float Memory = 12.0f)
		{
			FSoundRayEvent Event;
			Event.EventId = FGuid::NewGuid(); Event.Type = Type;
			Event.Strength = Event.EmittedStrength = Strength;
			Event.SourceLocation = Event.HeardLocation = Event.ArrivalLocation = Position;
			Event.TimeSeconds = World->GetTimeSeconds(); Event.MemoryDuration = Memory;
			Event.SourceActor = Event.SourceOwner = Player;
			return Event;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundMetadataTest, "FPS_Shooter.SoundEvents.RayMetadataAndLegacyAPI", SoundFlags)
bool FSoundMetadataTest::RunTest(const FString& Parameters)
{
	FSoundTestWorld Scene;
	// Isolate empty-space attenuation from the listener mesh acting as a reflecting obstacle.
	Scene.Guard->GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	AActor* Source = Scene.Source(FVector(0, 0, 200));
	Source->SetOwner(Scene.Player);
	USoundRayEmitterComponent* Emitter = Scene.Emitter(Source);
	for (ESoundEventType Type : { ESoundEventType::Footstep, ESoundEventType::Landing, ESoundEventType::Gunshot,
		ESoundEventType::ThrownObject, ESoundEventType::Door, ESoundEventType::Interaction })
	{
		Scene.Guard->HearingComponent->ClearSoundMemory();
		Emitter->EmitSoundEvent(Type);
		FNPCHearingStimulus Heard;
		if (!TestTrue(TEXT("Typed event reaches NPC through the existing ray"), Scene.Guard->HearingComponent->GetBestSound(Heard))) { continue; }
		TestEqual(TEXT("Event type arrives"), Heard.Event.Type, Type);
		TestTrue(TEXT("Identity shared with emitter"), Heard.Event.EventId == Emitter->GetLastEmittedEvent().EventId);
		TestTrue(TEXT("Source actor and responsible owner preserved"), Heard.Event.SourceActor == Source && Heard.Event.SourceOwner == Scene.Player);
		TestTrue(TEXT("Emission position preserved"), Heard.Event.SourceLocation.Equals(FVector(0, 0, 200)));
		TestTrue(TEXT("Direct sound localizes to source"), Heard.Location.Equals(Heard.Event.SourceLocation));
		TestTrue(TEXT("Arrival point remains on ray at listener"), Heard.Event.ArrivalLocation.Equals(Scene.Guard->GetActorLocation(), 0.1f));
		TestEqual(TEXT("Emission timestamp preserved"), Heard.Event.TimeSeconds, double(Scene.World->GetTimeSeconds()));
		TestTrue(TEXT("Distance attenuation preserved"), FMath::IsNearlyEqual(Heard.Strength, 0.88f, 0.001f));
	}
	Emitter->EmitSoundEvent(ESoundEventType::Gunshot, 1.6f, 3000);
	TestEqual(TEXT("Overrides do not change footstep volume"), Emitter->Volume, 1.0f);
	TestEqual(TEXT("Overrides do not change propagation distance"), Emitter->MaxDistance, 1000.0f);
	Scene.Guard->HearingComponent->ClearSoundMemory();
	Emitter->EmitFootstepSound();
	TestEqual(TEXT("Legacy emission remains a footstep"), Scene.Guard->HearingComponent->LastHeardEvent.Type, ESoundEventType::Footstep);
	Scene.Guard->HearingComponent->ClearSoundMemory();
	Scene.Guard->HearingComponent->HearSoundAtLocation(FVector(500, 0, 200), 0.5f);
	TestTrue(TEXT("Legacy hearing API remains functional"), Scene.Guard->HearingComponent->HasHeardSound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundReflectionsTest, "FPS_Shooter.SoundEvents.ReflectionsAbsorptionAndOcclusion", SoundFlags)
bool FSoundReflectionsTest::RunTest(const FString& Parameters)
{
	FSoundTestWorld Scene;
	Scene.Guard->SetActorLocation(FVector(200, 800, 200));
	AActor* Source = Scene.Source(FVector(0, 0, 200)); Source->SetActorRotation(FRotator(0, 45, 0));
	USoundRayEmitterComponent* Emitter = Scene.Emitter(Source);
	Emitter->Volume = 1.2f; Emitter->MaxDistance = 1600; Emitter->WallAbsorption = 0.5f;
	Scene.Wall(FVector(510, 500, 200), FVector(10, 1500, 200));
	Emitter->EmitSoundEvent(ESoundEventType::ThrownObject);
	TestFalse(TEXT("Off-axis listener cannot hear without reflection"), Scene.Guard->HearingComponent->HasHeardSound());
	Emitter->MaxReflections = 1;
	Emitter->EmitSoundEvent(ESoundEventType::ThrownObject);
	FNPCHearingStimulus Heard;
	if (TestTrue(TEXT("Reflected ray reaches listener"), Scene.Guard->HearingComponent->GetBestSound(Heard)))
	{
		TestEqual(TEXT("Reflection count delivered"), Heard.Event.ReflectionCount, 1);
		TestTrue(TEXT("Original source position survives reflection"), Heard.Event.SourceLocation.Equals(Source->GetActorLocation()));
		TestTrue(TEXT("Reflected evidence localizes near reflecting wall"), Heard.Location.X > 490 && Heard.Location.Y > 490);
		TestTrue(TEXT("Wall absorption and distance attenuation preserved"), FMath::IsNearlyEqual(Heard.Strength, 0.2906f, 0.01f));
	}
	Scene.Guard->HearingComponent->ClearSoundMemory();
	Emitter->WallAbsorption = 1.0f;
	Emitter->EmitSoundEvent(ESoundEventType::ThrownObject);
	TestFalse(TEXT("Full absorption stops reflected sound"), Scene.Guard->HearingComponent->HasHeardSound());
	Emitter->WallAbsorption = 0.0f;
	Emitter->EmitSoundEvent(ESoundEventType::ThrownObject);
	TestTrue(TEXT("Less absorption yields stronger reflection"), Scene.Guard->HearingComponent->CurrentSoundStrength > 0.7f);
	Scene.Guard->HearingComponent->ClearSoundMemory();
	Scene.Guard->SetActorLocation(FVector(600, 500, 200)); Scene.Guard->HearingComponent->HearingRadius = 220;
	Emitter->EmitSoundEvent(ESoundEventType::Gunshot);
	TestFalse(TEXT("Hearing radius cannot leak through solid wall"), Scene.Guard->HearingComponent->HasHeardSound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundTypedPriorityTest, "FPS_Shooter.SoundEvents.TypeStrengthAgeDistanceAndMemory", SoundFlags)
bool FSoundTypedPriorityTest::RunTest(const FString& Parameters)
{
	FSoundTestWorld Scene;
	UNPCHearingComponent* Hearing = Scene.Guard->HearingComponent;
	const FVector Position(500, 0, 200);
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Footstep, 0.8f, Position));
	FSoundRayEvent Gunshot = Scene.Event(ESoundEventType::Gunshot, 0.4f, Position);
	Hearing->HearSoundEvent(Gunshot);
	FNPCHearingStimulus Best;
	Hearing->GetBestSound(Best);
	TestEqual(TEXT("Quieter gunshot can outrank a louder footstep"), Best.Event.Type, ESoundEventType::Gunshot);
	TestEqual(TEXT("Different event types at same position are retained"), Hearing->GetRememberedSoundCount(), 2);
	Gunshot.Strength = 0.2f; Hearing->HearSoundEvent(Gunshot);
	TestEqual(TEXT("Weaker reflection does not add another event"), Hearing->GetRememberedSoundCount(), 2);
	Gunshot.Strength = 0.6f; Hearing->HearSoundEvent(Gunshot); Hearing->GetBestSound(Best);
	TestEqual(TEXT("Strongest arrival of one event wins"), Best.Strength, 0.6f);
	Hearing->ClearSoundMemory();
	FSoundRayEvent Far = Scene.Event(ESoundEventType::Gunshot, 0.6f, FVector(4000, 0, 200));
	Far.HeardLocation = Scene.Guard->GetActorLocation();
	Hearing->HearSoundEvent(Far);
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Gunshot, 0.5f, Position));
	Hearing->GetBestSound(Best);
	TestTrue(TEXT("Distance uses original source, not the nearby arrival segment"), Best.Event.SourceLocation.Equals(Position));
	Scene.Player->SetActorLocation(FVector(8000, 0, 200));
	TestTrue(TEXT("Evidence position does not follow a moving owner"), Best.Event.SourceLocation.Equals(Position));
	Hearing->ClearSoundMemory();
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Gunshot, 0.9f, Position, 6.0f));
	static_cast<UActorComponent*>(Hearing)->TickComponent(5.0f, LEVELTICK_All, nullptr);
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Footstep, 0.5f, Position));
	Hearing->GetBestSound(Best);
	TestEqual(TEXT("Fresh sound outranks aged gunshot"), Best.Event.Type, ESoundEventType::Footstep);
	Hearing->ClearSoundMemory();
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Door, 0.9f, Position, 1.0f));
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Interaction, 0.3f, Position, 60.0f));
	static_cast<UActorComponent*>(Hearing)->TickComponent(1.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Each source keeps its own memory duration"), Hearing->GetRememberedSoundCount(), 1);
	Hearing->GetBestSound(Best);
	TestEqual(TEXT("Long-lived event survives short-lived source"), Best.Event.Type, ESoundEventType::Interaction);
	Hearing->ClearSoundMemory();
	AStealthAIController* AI = CastChecked<AStealthAIController>(Scene.Guard->GetController());
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Footstep, 0.5f, Position));
	AI->Tick(0.02f);
	const FVector ShotPosition(700, 700, 200);
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Gunshot, 0.4f, ShotPosition));
	AI->Tick(0.02f);
	TestTrue(TEXT("NPC redirects toward a more salient typed event"), AI->GetInvestigationLocation().Equals(ShotPosition));
	Hearing->HearSoundEvent(Scene.Event(ESoundEventType::Footstep, 0.75f, Position));
	AI->Tick(0.02f);
	TestTrue(TEXT("Current gunshot retains type weight during hysteresis comparison"), AI->GetInvestigationLocation().Equals(ShotPosition));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundGameplaySourcesTest, "FPS_Shooter.SoundEvents.LandingDoorsAndInteractions", SoundFlags)
bool FSoundGameplaySourcesTest::RunTest(const FString& Parameters)
{
	FSoundTestWorld Scene;
	Scene.Player->GetSoundEmitter()->EmitFootstepSound();
	TestTrue(TEXT("Possessed pawn footsteps identify the pawn rather than its controller"),
		Scene.Player->GetSoundEmitter()->GetLastEmittedEvent().SourceOwner == Scene.Player);
	const float FootstepVolume = Scene.Player->GetSoundEmitter()->Volume;
	const float FootstepDistance = Scene.Player->GetSoundEmitter()->MaxDistance;
	Scene.Player->GetCharacterMovement()->Velocity = FVector(0, 0, -350);
	Scene.Player->Landed(FHitResult());
	const FSoundRayEvent Soft = Scene.Player->GetSoundEmitter()->GetLastEmittedEvent();
	Scene.Player->GetCharacterMovement()->Velocity = FVector(0, 0, -950);
	Scene.Player->Landed(FHitResult());
	const FSoundRayEvent Hard = Scene.Player->GetSoundEmitter()->GetLastEmittedEvent();
	TestEqual(TEXT("Landing uses its event type"), Hard.Type, ESoundEventType::Landing);
	TestTrue(TEXT("Faster impact increases strength and range"), Hard.Strength > Soft.Strength && Hard.RayDistance > Soft.RayDistance);
	TestEqual(TEXT("Landing preserves footstep volume"), Scene.Player->GetSoundEmitter()->Volume, FootstepVolume);
	TestEqual(TEXT("Landing preserves footstep distance"), Scene.Player->GetSoundEmitter()->MaxDistance, FootstepDistance);
	TestNotNull(TEXT("Landing audio asset retained"), Scene.Player->GetLandingSound());
	Scene.Player->SetActorLocation(FVector(1000, 0, 200));
	Scene.Guard->FireDamage = 0.0f;
	TestTrue(TEXT("Guard can still fire"), Scene.Guard->FireWeaponAt(Scene.Player));
	TestEqual(TEXT("NPC gun uses Gunshot event"), Scene.Guard->SoundEmitter->GetLastEmittedEvent().Type, ESoundEventType::Gunshot);
	TestTrue(TEXT("NPC gunshot identifies its owner"), Scene.Guard->SoundEmitter->GetLastEmittedEvent().SourceOwner == Scene.Guard);
	AStealthDoor* Door = Scene.Spawn<AStealthDoor>(FVector(1500, 0, 0));
	FHitResult DoorHit;
	TestTrue(TEXT("Closed door blocks a visibility trace"), Scene.World->LineTraceSingleByChannel(DoorHit,
		FVector(1400, 50, 100), FVector(1600, 50, 100), ECC_Visibility) && DoorHit.GetActor() == Door);
	TestTrue(TEXT("Opening door changes state"), Door->SetOpen(true, Scene.Player));
	TestEqual(TEXT("Door movement emits Door event"), Door->SoundEmitter->GetLastEmittedEvent().Type, ESoundEventType::Door);
	TestTrue(TEXT("Door event identifies responsible actor"), Door->SoundEmitter->GetLastEmittedEvent().SourceOwner == Scene.Player);
	const FGuid OpeningId = Door->SoundEmitter->GetLastEmittedEvent().EventId;
	TestFalse(TEXT("Repeated open is not a second action"), Door->SetOpen(true, Scene.Player));
	TestTrue(TEXT("Repeated state does not duplicate sound"), Door->SoundEmitter->GetLastEmittedEvent().EventId == OpeningId);
	Door->Tick(0.7f);
	TestTrue(TEXT("Door leaf animates to open angle"), FMath::IsNearlyEqual(Door->DoorMesh->GetRelativeRotation().Yaw, Door->OpenAngle));
	TestFalse(TEXT("Open door clears the passage trace"), Scene.World->LineTraceSingleByChannel(DoorHit,
		FVector(1400, 50, 100), FVector(1600, 50, 100), ECC_Visibility));
	IStealthInteractable::Execute_Interact(Door, Scene.Player);
	TestFalse(TEXT("Existing interaction interface closes door"), Door->IsOpen());
	TestTrue(TEXT("Closing emits a fresh event"), Door->SoundEmitter->GetLastEmittedEvent().EventId != OpeningId);
	Door->Tick(0.7f);
	TestTrue(TEXT("Closing restores door collision across passage"), Scene.World->LineTraceSingleByChannel(DoorHit,
		FVector(1400, 50, 100), FVector(1600, 50, 100), ECC_Visibility) && DoorHit.GetActor() == Door);
	TestNotNull(TEXT("Door audio available and replaceable"), Door->SoundEmitter->DoorCloseAudio.Get());
	ANoiseMakerPickup* Pickup = Scene.Spawn<ANoiseMakerPickup>(FVector(100, 0, 200));
	USoundRayInteractionComponent* PickupSound = Pickup->FindComponentByClass<USoundRayInteractionComponent>();
	const int32 NoiseMakers = Scene.Player->GetNoiseMakerCount();
	IStealthInteractable::Execute_Interact(Pickup, Scene.Player);
	TestTrue(TEXT("Pickup still grants inventory"), Scene.Player->GetNoiseMakerCount() > NoiseMakers);
	TestEqual(TEXT("Successful pickup emits Interaction"), PickupSound->GetLastEmittedEvent().Type, ESoundEventType::Interaction);
	AStealthObjective* Objective = Scene.Spawn<AStealthObjective>(FVector(100, 0, 200));
	USoundRayInteractionComponent* ObjectiveSound = Objective->FindComponentByClass<USoundRayInteractionComponent>();
	IStealthInteractable::Execute_Interact(Objective, Scene.Player);
	TestTrue(TEXT("Intel interaction still succeeds"), Scene.Mission->HasObjective());
	TestEqual(TEXT("Successful intel interaction emits Interaction"), ObjectiveSound->GetLastEmittedEvent().Type, ESoundEventType::Interaction);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundDebugTest, "FPS_Shooter.SoundEvents.DebugOnlyByConsole", SoundFlags)
bool FSoundDebugTest::RunTest(const FString& Parameters)
{
	FSoundTestWorld Scene;
	IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("SoundRays.Debug"));
	if (!TestNotNull(TEXT("Debug console command registered"), Debug)) { return false; }
	const int32 Previous = Debug->GetInt();
	Debug->Set(0, ECVF_SetByConsole);
	AActor* Source = Scene.Source(FVector(0, 0, 200));
	USoundRayEmitterComponent* Emitter = Scene.Emitter(Source); Emitter->bDrawDebug = true;
	const auto LineCount = [&Scene]()
	{
		int32 Count = 0;
		for (ULineBatchComponent* Batcher : { Scene.World->LineBatcher.Get(), Scene.World->PersistentLineBatcher.Get(), Scene.World->ForegroundLineBatcher.Get() })
		{
			if (Batcher) { Count += Batcher->BatchedLines.Num(); }
		}
		return Count;
	};
	Emitter->EmitFootstepSound();
	TestEqual(TEXT("Legacy serialized debug flag cannot draw by itself"), LineCount(), 0);
	static_cast<AActor*>(Scene.Guard)->Tick(0.01f);
	TestFalse(TEXT("NPC debug label hidden by default"), Scene.Guard->SoundLabel->IsVisible());
	Debug->Set(1, ECVF_SetByConsole); Emitter->EmitFootstepSound();
	TestTrue(TEXT("Console enables ray drawing"), LineCount() > 0);
	static_cast<AActor*>(Scene.Guard)->Tick(0.01f);
	TestTrue(TEXT("Console also enables NPC hearing label"), Scene.Guard->SoundLabel->IsVisible());
	Debug->Set(0, ECVF_SetByConsole); const int32 Before = LineCount(); Emitter->EmitFootstepSound();
	TestEqual(TEXT("Disabling command stops new debug geometry"), LineCount(), Before);
	static_cast<AActor*>(Scene.Guard)->Tick(0.01f);
	TestFalse(TEXT("Disabling command hides labels"), Scene.Guard->SoundLabel->IsVisible());
	Debug->Set(Previous, ECVF_SetByConsole);
	return true;
}

#endif
