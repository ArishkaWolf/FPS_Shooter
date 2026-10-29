#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "SoundHearingNPC.h"
#include "StealthAIController.h"
#include "StealthArmsAnimInstance.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthObjective.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"

class FStealthCharacterPIECommand : public IAutomationLatentCommand
{
public:
	explicit FStealthCharacterPIECommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!World || World->WorldType != EWorldType::PIE || !World->HasBegunPlay() || World->GetTimeSeconds() < 2.0f)
		{
			return false;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		AStealthCharacter* Player = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
		if (!Player) { Test->AddError(TEXT("No first-person player in PIE")); return true; }
		const double StageTimeout = FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) ? 20.0 : 120.0;
		if (FPlatformTime::Seconds() - WallStageStarted > StageTimeout)
		{
			Test->AddError(FString::Printf(TEXT("Character PIE stage %d timed out"), Stage));
			return true;
		}
		if (World->GetTimeSeconds() - StageStarted < Delay) { return false; }
		AStealthGameMode* Mission = World->GetAuthGameMode<AStealthGameMode>();
		UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>();
		USkeletalMeshComponent* Arms = nullptr;
		USkeletalMeshComponent* Weapon = nullptr;
		TArray<USkeletalMeshComponent*> Meshes;
		Player->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == TEXT("FirstPersonArms")) { Arms = Mesh; }
			if (Mesh->GetFName() == TEXT("WeaponMesh")) { Weapon = Mesh; }
		}
		if (!Camera || !Arms || !Weapon || !Mission) { Test->AddError(TEXT("Missing first-person components")); return true; }
		const auto Key = [PC](FKey InKey, bool bDown)
		{
			PC->InputKey(FInputKeyParams(InKey, bDown ? IE_Pressed : IE_Released, bDown ? 1.0 : 0.0));
		};
		const auto Tap = [&Key, PC](FKey InKey)
		{
			Key(InKey, true); PC->PlayerTick(0.016f); Key(InKey, false); PC->PlayerTick(0.016f);
		};
		Delay = 0.35f;
		switch (Stage)
		{
		case 0:
			for (TActorIterator<ASoundHearingNPC> It(World); It; ++It)
			{
				It->GetCharacterMovement()->DisableMovement();
				if (AController* AI = It->GetController()) { AI->SetActorTickEnabled(false); }
			}
			Player->SetActorLocation(FVector(1400, 1110, 100));
			PC->SetControlRotation(FRotator::ZeroRotator);
			Mission->Tick(0.01f);
			Test->TestTrue(TEXT("First-person camera remains active"), Camera->IsActive() && Player->GetMesh()->bOwnerNoSee);
			Test->TestTrue(TEXT("Arms and rifle remain owner-only"), Arms->bOnlyOwnerSee && Weapon->bOnlyOwnerSee);
			Test->TestNotNull(TEXT("Native blended arms graph is active"), Cast<UStealthArmsAnimInstance>(Arms->GetAnimInstance()));
			Test->TestEqual(TEXT("Rifle follows the right hand"), Weapon->GetAttachSocketName(), FName(TEXT("hand_r")));
			Test->TestTrue(TEXT("Rifle bore faces the camera aim direction"), FVector::DotProduct(
				Weapon->GetComponentQuat().RotateVector(FVector::YAxisVector), Camera->GetForwardVector()) > 0.98f);
			Test->TestTrue(TEXT("Rifle grip is held near the right hand"), FVector::Dist(Weapon->GetSocketLocation(TEXT("Grip_Bone")), Arms->GetSocketLocation(TEXT("hand_r"))) < 20.0f);
			Capture(TEXT("Hip"));
			break;
		case 1:
			Test->TestEqual(TEXT("Stationary arms idle"), Player->GetArmsState(), EStealthArmsState::Idle);
			Test->TestTrue(TEXT("Left hand supports the rifle foregrip"), FVector::Dist(Arms->GetSocketLocation(TEXT("hand_l")),
				Weapon->GetComponentTransform().TransformPosition(Player->GetForegripLocation())) < 3.0f);
			Key(EKeys::RightMouseButton, true);
			break;
		case 2:
		{
			const FVector Sight = Camera->GetComponentTransform().InverseTransformPosition(
				Weapon->GetComponentTransform().TransformPosition(Player->GetIronSightLocation()));
			Test->AddInfo(FString::Printf(TEXT("ADS sight in camera space: %s, FOV %.2f"), *Sight.ToString(), Camera->FieldOfView));
			Test->TestTrue(TEXT("ADS smoothly approaches configured FOV"), Camera->FieldOfView < 66.0f && Camera->FieldOfView >= 65.0f);
			Test->TestTrue(TEXT("ADS aligns sights with screen centre"), FMath::Abs(Sight.Y) < 2.0f && FMath::Abs(Sight.Z) < 2.0f);
			Test->TestEqual(TEXT("ADS arms state"), Player->GetArmsState(), EStealthArmsState::Aim);
			Capture(TEXT("Aim"));
			Key(EKeys::LeftShift, true); Key(EKeys::W, true);
			break;
		}
		case 3:
			Test->TestTrue(TEXT("ADS blocks sprint and slows actual movement"), Player->GetVelocity().Size2D() <= 171.0f
				&& Player->GetCharacterMovement()->MaxWalkSpeed <= 170.0f);
			Key(EKeys::RightMouseButton, false); Key(EKeys::W, false); Key(EKeys::LeftShift, false);
			break;
		case 4:
			Test->TestTrue(TEXT("Releasing ADS restores hip FOV"), Camera->FieldOfView > 89.0f);
			Key(EKeys::W, true); Key(EKeys::LeftShift, true);
			break;
		case 5:
			Test->TestEqual(TEXT("Moving arms enter run"), Player->GetArmsState(), EStealthArmsState::Run);
			Test->TestEqual(TEXT("Sprint speed restored"), Player->GetCharacterMovement()->MaxWalkSpeed, 650.0f);
			Key(EKeys::W, false); Key(EKeys::LeftShift, false); Tap(EKeys::C);
			break;
		case 6:
			Test->TestTrue(TEXT("C toggles crouch"), Player->bIsCrouched);
			Test->TestEqual(TEXT("Crouch arms state"), Player->GetArmsState(), EStealthArmsState::Crouch);
			Capture(TEXT("Crouch"));
			Key(EKeys::RightMouseButton, true);
			break;
		case 7:
			Test->TestEqual(TEXT("Crouched ADS speed reduced"), Player->GetCharacterMovement()->MaxWalkSpeedCrouched, 125.0f);
			Key(EKeys::RightMouseButton, false); Tap(EKeys::C); Key(EKeys::LeftMouseButton, true);
			Delay = 0.20f;
			break;
		case 8:
			Test->TestTrue(TEXT("Held fire consumes rounds"), Player->GetAmmoInMagazine() < 30);
			Test->TestEqual(TEXT("Firing arms state"), Player->GetArmsState(), EStealthArmsState::Fire);
			Key(EKeys::LeftMouseButton, false);
			AmmoBeforeReload = Player->GetAmmoInMagazine();
			Tap(EKeys::R);
			Delay = 0.4f;
			break;
		case 9:
			Test->TestTrue(TEXT("Reload stays active over time"), Player->IsReloading());
			Test->TestEqual(TEXT("Reload arms state"), Player->GetArmsState(), EStealthArmsState::Reload);
			Test->TestEqual(TEXT("Reload does not transfer ammo early"), Player->GetAmmoInMagazine(), AmmoBeforeReload);
			Tap(EKeys::LeftMouseButton); Tap(EKeys::R);
			Test->TestEqual(TEXT("Cannot fire during reload"), Player->GetAmmoInMagazine(), AmmoBeforeReload);
			Capture(TEXT("Reload"));
			Key(EKeys::RightMouseButton, true);
			Delay = 2.0f;
			break;
		case 10:
			Test->TestFalse(TEXT("Reload completes"), Player->IsReloading());
			Test->TestEqual(TEXT("Reload fills magazine at completion"), Player->GetAmmoInMagazine(), 30);
			Test->TestEqual(TEXT("Reload transfers only missing rounds"), Player->GetReserveAmmo(), 90 - (30 - AmmoBeforeReload));
			Test->TestTrue(TEXT("Held ADS resumes after reload"), Player->GetAimAlpha() > 0.85f);
			Key(EKeys::RightMouseButton, false); Tap(EKeys::LeftMouseButton); Tap(EKeys::R);
			break;
		case 11:
			Tap(EKeys::Q);
			Test->TestFalse(TEXT("Throwing cancels reload without blocking distraction"), Player->IsReloading());
			Test->TestEqual(TEXT("Q still consumes a noise maker"), Player->GetNoiseMakerCount(), 2);
			Test->TestEqual(TEXT("Cancelled reload grants no rounds"), Player->GetAmmoInMagazine(), 29);
			for (TActorIterator<AStealthObjective> It(World); It; ++It)
			{
				Player->SetActorLocation(It->GetActorLocation() - FVector(180, 0, 64));
				PC->SetControlRotation((It->GetActorLocation() - Camera->GetComponentLocation()).Rotation());
				break;
			}
			break;
		case 12:
			Tap(EKeys::E);
			Test->TestTrue(TEXT("E still interacts using the first-person camera trace"), Mission->HasObjective());
			PC->SetControlRotation(FRotator::ZeroRotator);
			Player->SetActorLocation(FVector(1000, 1300, 220));
			Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Delay = 0.8f;
			break;
		case 13:
			if (Player->GetCharacterMovement()->IsFalling()) { return false; }
			SoftLanding = Player->GetLastLandingStrength();
			Test->TestTrue(TEXT("A fall generates a landing impact"), SoftLanding > 0.0f);
			Player->SetActorLocation(FVector(1000, 1300, 1200));
			Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Delay = 1.8f;
			break;
		case 14:
			if (Player->GetCharacterMovement()->IsFalling()) { return false; }
			Test->TestTrue(TEXT("Higher fall produces a stronger landing sound"), Player->GetLastLandingStrength() > SoftLanding);
			Test->TestFalse(TEXT("Jump is disabled on this level"), Player->CanJump());
			Test->TestEqual(TEXT("Full first-person control test preserves live mission"), Mission->GetMissionState(), EStealthMissionState::Playing);
			return true;
		}
		++Stage;
		StageStarted = World->GetTimeSeconds();
		WallStageStarted = FPlatformTime::Seconds();
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Stage = 0;
	int32 AmmoBeforeReload = 30;
	float SoftLanding = 0.0f;
	float StageStarted = 0.0f;
	float Delay = 0.0f;
	double WallStageStarted = FPlatformTime::Seconds();

	void Capture(const FString& Name) const
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
		{
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("CharacterPolish/Screenshots");
			IFileManager::Get().MakeDirectory(*Directory, true);
			FScreenshotRequest::RequestScreenshot(Directory / (Name + TEXT(".png")), false, false);
		}
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthCharacterPIETest, "FPS_Shooter.Character.FirstPersonControlsPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStealthCharacterPIETest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/ThirdPersonMap"), true))
	{
		AddError(TEXT("Cannot start character controls test in PIE"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FStealthCharacterPIECommand(this));
	return true;
}

#endif
