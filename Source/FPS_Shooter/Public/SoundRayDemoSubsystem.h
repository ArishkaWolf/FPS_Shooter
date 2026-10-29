#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SoundRayDemoSubsystem.generated.h"

UCLASS()
class FPS_SHOOTER_API USoundRayDemoSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void SpawnDemo(UWorld& InWorld) const;
};
