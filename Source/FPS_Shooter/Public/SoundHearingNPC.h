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

protected:
	virtual void Tick(float DeltaSeconds) override;
};
