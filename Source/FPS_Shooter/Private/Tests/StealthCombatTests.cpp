#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Slate/SceneViewport.h"
#include "SoundHearingNPC.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthHUD.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

namespace
{
bool CanTraceRegion(USkeletalMeshComponent& Mesh, const TArray<FName>& Bones, float Multiplier)
{
	const UPhysicsAsset* PhysicsAsset = Mesh.GetPhysicsAsset();
	if (!PhysicsAsset || PhysicsAsset->GetFName() != TEXT("PA_Mannequin")) { return false; }
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatHitRegions), true);
	for (FName Bone : Bones)
	{
		if (PhysicsAsset->FindBodyIndex(Bone) == INDEX_NONE) { continue; }
		for (const FVector& Side : { FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector })
		{
			for (float Height : { 0.0f, -8.0f, 8.0f })
			{
				const FVector Center = Mesh.GetSocketLocation(Bone) + FVector(0, 0, Height);
				FHitResult Hit;
				if (Mesh.LineTraceComponent(Hit, Center + Side * 175.0f, Center - Side * 6.0f, Query)
					&& Bones.Contains(Hit.BoneName) && StealthBallistics::GetDamageMultiplier(Hit.BoneName) == Multiplier)
				{
					return true;
				}
			}
		}
	}
	return false;
}

class FCombatPIECheck final : public IAutomationLatentCommand
{
public:
	explicit FCombatPIECheck(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 150.0)
		{
			Test.AddError(FString::Printf(TEXT("Combat PIE timed out in stage %d"), Stage));
			return true;
		}
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || !World->HasBegunPlay()) { return false; }
		FSceneViewport* Viewport = nullptr;
		if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
		{
			UGameViewportClient* Client = World->GetGameViewport();
			Viewport = Client ? Client->GetGameViewport() : nullptr;
			if (!Viewport) { return false; }
			if (Viewport->GetSizeXY() != FIntPoint(1280, 720))
			{
				Viewport->SetFixedViewportSize(1280, 720);
				return false;
			}
			if (!PendingScreenshot.IsEmpty())
			{
				if (GFrameCounter <= ScreenshotFrame + 1) { return false; }
				SaveScreenshot(*Viewport);
			}
		}
		if (World->GetTimeSeconds() < ReadyAt) { return false; }
		APlayerController* PC = World->GetFirstPlayerController();
		AStealthCharacter* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		UCameraComponent* Camera = Player ? Player->FindComponentByClass<UCameraComponent>() : nullptr;
		AStealthGameMode* Mission = World->GetAuthGameMode<AStealthGameMode>();
		if (!Player || !Camera || !Mission || !Cast<AStealthHUD>(PC->GetHUD()))
		{
			Test.AddError(TEXT("Missing first-person combat or HUD in PIE"));
			return true;
		}

		switch (Stage)
		{
		case 0:
			if (!Prepare(*World, *Player, *Mission)) { return true; }
			SetKey(*PC, EKeys::RightMouseButton, true);
			break;
		case 1:
			for (USkeletalMeshComponent* Mesh : { Player->GetMesh(), Guard->GetMesh() })
			{
				Test.TestTrue(TEXT("Physics Asset traces hit the head"), CanTraceRegion(*Mesh, {TEXT("head")}, 2.5f));
				Test.TestTrue(TEXT("Physics Asset traces hit the torso"), CanTraceRegion(*Mesh,
					{TEXT("pelvis"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05")}, 1.0f));
				Test.TestTrue(TEXT("Physics Asset traces hit the left arm"), CanTraceRegion(*Mesh,
					{TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l")}, 0.75f));
				Test.TestTrue(TEXT("Physics Asset traces hit the right arm"), CanTraceRegion(*Mesh,
					{TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")}, 0.75f));
				Test.TestTrue(TEXT("Physics Asset traces hit the left leg"), CanTraceRegion(*Mesh,
					{TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l")}, 0.65f));
				Test.TestTrue(TEXT("Physics Asset traces hit the right leg"), CanTraceRegion(*Mesh,
					{TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")}, 0.65f));
			}
			Test.TestTrue(TEXT("ADS is active before shooting"), Player->GetAimAlpha() > 0.98f && Camera->FieldOfView < 66.0f);
			AimAt(*PC, *Camera, Guard->GetMesh()->GetSocketLocation(TEXT("head")) + FVector(0, 0, 7));
			break;
		case 2:
		{
			Fire(*PC);
			Test.TestEqual(TEXT("Player rifle spends a round on cover"), Player->GetAmmoInMagazine(), 29);
			Test.TestEqual(TEXT("Wall blocks player damage"), Guard->GetHealth(), 100.0f);
			Test.TestTrue(TEXT("Guard fires toward the wall"), Guard->FireWeaponAt(Player));
			Test.TestEqual(TEXT("Wall blocks guard damage"), Player->GetHealth(), 100.0f);
			bool bBulletMark = false;
			for (USceneComponent* Child : Wall->GetStaticMeshComponent()->GetAttachChildren())
			{
				if (const UDecalComponent* Decal = Cast<UDecalComponent>(Child))
				{
					bBulletMark |= Decal->GetDecalMaterial() != nullptr && Decal->GetComponentScale().Equals(FVector::OneVector);
				}
			}
			Test.TestTrue(TEXT("Rifle impact places a correctly sized bullet mark on cover"), bBulletMark);
			Capture(TEXT("CoverImpact"));
			break;
		}
		case 3:
			Wall->Destroy();
			Fire(*PC);
			Test.TestEqual(TEXT("ADS rifle head hit uses 2.5x damage"), Guard->GetHealth(), 15.0f);
			Test.TestTrue(TEXT("Surviving guard starts hit reaction"), Guard->GetHitAnimationAge() < 0.05f);
			AimAt(*PC, *Camera, Guard->GetMesh()->GetSocketLocation(TEXT("spine_04")));
			break;
		case 4:
			Fire(*PC);
			Test.TestTrue(TEXT("Player rifle can kill a guard"), Guard->IsDead() && Guard->GetHealth() == 0.0f);
			Test.TestTrue(TEXT("Guard death animation starts"), Guard->GetDeathAnimation() && Guard->GetDeathAnimationAge() < 0.05f);
			Test.TestEqual(TEXT("HUD ammo source counts all three shots"), Player->GetAmmoInMagazine(), 27);
			SetKey(*PC, EKeys::RightMouseButton, false);
			AimAt(*PC, *Camera, Guard->GetActorLocation() - FVector(0, 0, 58));
			return Next(0.9f, *World);
		case 5:
			Capture(TEXT("GuardDown"));
			return Next(0.0f, *World);
		case 6:
			if (!Enemy.IsValid())
			{
				Enemy = SpawnGuard(*World, Clearing + FVector(450, 0, 0));
				if (!Test.TestNotNull(TEXT("Second guard can engage"), Enemy.Get())) { return true; }
			}
			if (Player->GetHealth() >= 100.0f) { return false; }
			Test.TestTrue(TEXT("First guard burst costs health without instant defeat"), Player->GetHealth() > 0.0f);
			Test.TestTrue(TEXT("Guard damage triggers first-person hit feedback"), Player->GetHitFeedback() > 0.0f);
			Capture(TEXT("GuardBurst"));
			return Next(0.0f, *World);
		case 7:
			if (Mission->GetMissionState() == EStealthMissionState::Playing) { return false; }
			Test.TestEqual(TEXT("Guard bursts can defeat the player"), Mission->GetMissionState(), EStealthMissionState::Lost);
			Test.TestEqual(TEXT("Defeat requires zero health"), Player->GetHealth(), 0.0f);
			Test.TestTrue(TEXT("Guard needed several shots"), Enemy->GetShotsFired() > 3);
			Test.TestFalse(TEXT("First-person death precedes the result"), Mission->IsMissionResultReady());
			return Next(0.0f, *World);
		case 8:
			if (!Mission->IsMissionResultReady()) { return false; }
			Test.TestTrue(TEXT("Death lowers and rolls the camera"), Camera->GetComponentRotation().Roll > 12.0f);
			Test.TestTrue(TEXT("Defeat result pauses after the animation"), UGameplayStatics::IsGamePaused(World));
			Capture(TEXT("PlayerDown"));
			return Next(0.0f, *World);
		default:
			if (Viewport) { Viewport->SetFixedViewportSize(0, 0); }
			return true;
		}
		return Next(0.45f, *World);
	}

private:
	bool Next(float Delay, const UWorld& World)
	{
		++Stage;
		ReadyAt = World.GetTimeSeconds() + Delay;
		return false;
	}

	bool Prepare(UWorld& World, AStealthCharacter& Player, AStealthGameMode& Mission)
	{
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("Greybox.QA.Movement"))) { Clearing = It->GetActorLocation(); break; }
		}
		if (Clearing.IsNearlyZero()) { Test.AddError(TEXT("Missing combat clearing")); return false; }
		for (TActorIterator<ASoundHearingNPC> It(&World); It; ++It)
		{
			if (AController* AI = It->GetController()) { AI->SetActorTickEnabled(false); }
		}
		Player.SetActorLocation(Clearing);
		Mission.Tick(0.01f);
		Guard = SpawnGuard(World, Clearing + FVector(525, 0, 0));
		if (!Test.TestNotNull(TEXT("Target guard spawned"), Guard.Get())) { return false; }
		Guard->GetController()->SetActorTickEnabled(false);
		Guard->GetCharacterMovement()->DisableMovement();
		Wall = World.SpawnActor<AStaticMeshActor>(Clearing + FVector(265, 0, 20), FRotator::ZeroRotator);
		if (!Test.TestNotNull(TEXT("Combat cover spawned"), Wall.Get())) { return false; }
		Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorScale3D(FVector(0.25, 2.5, 2.25));
		return true;
	}

	static ASoundHearingNPC* SpawnGuard(UWorld& World, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World.SpawnActor<ASoundHearingNPC>(Location, FRotator(0, 180, 0), Params);
	}

	static void SetKey(APlayerController& PC, FKey Key, bool bDown)
	{
		PC.InputKey(FInputKeyParams(Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0 : 0.0));
		PC.PlayerTick(0.016f);
	}

	static void Fire(APlayerController& PC)
	{
		SetKey(PC, EKeys::LeftMouseButton, true);
		SetKey(PC, EKeys::LeftMouseButton, false);
	}

	static void AimAt(APlayerController& PC, const UCameraComponent& Camera, const FVector& Target)
	{
		PC.SetControlRotation((Target - Camera.GetComponentLocation()).Rotation());
	}

	void Capture(const TCHAR* Name)
	{
		PendingScreenshot = Name;
		ScreenshotFrame = GFrameCounter;
	}

	void SaveScreenshot(FSceneViewport& Viewport)
	{
		TArray<FColor> Pixels;
		const FIntPoint Size = Viewport.GetSizeXY();
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("CombatWork/Screenshots");
		IFileManager::Get().MakeDirectory(*Directory, true);
		if (Test.TestTrue(TEXT("Read the rendered PIE viewport"), GetViewportScreenShot(&Viewport, Pixels)
			&& Pixels.Num() == Size.X * Size.Y))
		{
			for (FColor& Pixel : Pixels) { Pixel.A = 255; }
			TArray64<uint8> PNG;
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
			Test.TestTrue(TEXT("Save combat screenshot"), FFileHelper::SaveArrayToFile(PNG,
				*(Directory / (PendingScreenshot + TEXT(".png")))));
		}
		PendingScreenshot.Reset();
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<ASoundHearingNPC> Guard;
	TWeakObjectPtr<ASoundHearingNPC> Enemy;
	TWeakObjectPtr<AStaticMeshActor> Wall;
	FVector Clearing = FVector::ZeroVector;
	FString PendingScreenshot;
	uint64 ScreenshotFrame = 0;
	int32 Stage = 0;
	float ReadyAt = 2.0f;
	double Started = FPlatformTime::Seconds();
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthCombatTest, "FPS_Shooter.Combat.BallisticsAndLethalityPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStealthCombatTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true)) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FCombatPIECheck(*this));
	return true;
}

#endif
