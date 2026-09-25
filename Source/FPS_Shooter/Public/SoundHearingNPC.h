#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SoundHearingNPC.generated.h"

class UNPCHearingComponent;
class UTextRenderComponent;

UCLASS()
class FPS_SHOOTER_API ASoundHearingNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ASoundHearingNPC();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNPCHearingComponent> HearingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTextRenderComponent> SoundLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing")
	FText NPCLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol")
	FName PatrolRouteName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float PatrolSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float InvestigateSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float ChaseSpeed = 430.0f;

protected:
	virtual void Tick(float DeltaSeconds) override;
};
