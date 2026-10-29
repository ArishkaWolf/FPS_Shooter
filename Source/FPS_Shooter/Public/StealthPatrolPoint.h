#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "StealthPatrolPoint.generated.h"

UCLASS()
class FPS_SHOOTER_API AStealthPatrolPoint : public ATargetPoint
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol")
	FName RouteName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol")
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol", meta=(ClampMin="0.0"))
	float WaitTime = 1.5f;
};
