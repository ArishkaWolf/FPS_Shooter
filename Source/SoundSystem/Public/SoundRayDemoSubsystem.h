#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SoundRayDemoSubsystem.generated.h"

UCLASS()
class SOUNDSYSTEM_API USoundRayDemoSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void SpawnDemo(UWorld& InWorld) const;
};
