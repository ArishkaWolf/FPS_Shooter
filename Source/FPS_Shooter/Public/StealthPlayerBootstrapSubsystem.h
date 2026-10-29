#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StealthPlayerBootstrapSubsystem.generated.h"

UCLASS()
class FPS_SHOOTER_API UStealthPlayerBootstrapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
