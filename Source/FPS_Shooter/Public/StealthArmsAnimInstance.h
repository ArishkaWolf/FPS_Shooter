#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "StealthArmsAnimInstance.generated.h"

// Native first-person graph: compatible rifle clips, blended locomotion/recoil and reload hand IK.
UCLASS(Transient)
class FPS_SHOOTER_API UStealthArmsAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
