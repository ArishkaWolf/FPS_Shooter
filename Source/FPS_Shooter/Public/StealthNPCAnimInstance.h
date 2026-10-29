#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "StealthNPCAnimInstance.generated.h"

// Manny-compatible rifle locomotion, upper-body recoil/hit reaction and full-body death.
UCLASS(Transient)
class FPS_SHOOTER_API UStealthNPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
