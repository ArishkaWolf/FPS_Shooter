#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StealthInteractable.generated.h"

class AStealthCharacter;

UINTERFACE(BlueprintType)
class UStealthInteractable : public UInterface
{
	GENERATED_BODY()
};

class FPS_SHOOTER_API IStealthInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(AStealthCharacter* InteractingCharacter);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	FText GetInteractionPrompt() const;
	virtual FText GetInteractionPrompt_Implementation() const { return NSLOCTEXT("StealthUI", "Interact", "Interact"); }
};
