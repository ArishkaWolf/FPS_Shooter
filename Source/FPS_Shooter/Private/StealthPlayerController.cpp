#include "StealthPlayerController.h"

#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "StealthGameMode.h"

void AStealthPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void AStealthPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	for (const FKey Key : { EKeys::Enter, EKeys::Gamepad_Special_Right })
	{
		FInputKeyBinding& Binding = InputComponent->BindKey(Key, IE_Pressed, this, &AStealthPlayerController::RestartMission);
		Binding.bExecuteWhenPaused = true;
	}
}

void AStealthPlayerController::ShowMissionResult()
{
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetPause(true);
}

void AStealthPlayerController::RestartMission()
{
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->RestartMission();
	}
}
