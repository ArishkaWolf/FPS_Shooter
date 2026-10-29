#include "StealthArmsAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "StealthCharacter.h"
#include "TwoBoneIK.h"

namespace
{
	class FStealthArmsProxy : public FAnimInstanceProxy
	{
	public:
		explicit FStealthArmsProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			// Read actors only on the game thread; pose evaluation can run on a worker thread.
			if (const AStealthCharacter* Player = Cast<AStealthCharacter>(Instance->TryGetPawnOwner()))
			{
				Idle = Player->GetArmsIdleAnimation();
				Run = Player->GetArmsRunAnimation();
				Fire = Player->GetArmsFireAnimation();
				Aim = Player->GetAimAlpha();
				ShotAge = Player->GetShotAnimationAge();
				bReloading = Player->IsReloading();
				if (bReloading) { Reload = Player->GetReloadProgress(); }
				ReloadWeight = FMath::FInterpTo(ReloadWeight, bReloading ? 1.0f : 0.0f, DeltaSeconds, 14.0f);
				WeaponToHand = Player->GetWeaponToHandTransform();
				MagazineGrip = Player->GetMagazineGripLocation();
				Foregrip = Player->GetForegripLocation();
				const float Speed = Player->GetVelocity().Size2D();
				const float MoveWeight = FMath::Clamp(Speed / 180.0f, 0.0f, 1.0f);
				const float TargetRunWeight = MoveWeight * (Player->bIsCrouched ? 0.25f : 1.0f)
					* FMath::Lerp(1.0f, 0.08f, Aim) * (bReloading ? 0.0f : 1.0f);
				RunWeight = FMath::FInterpTo(RunWeight, TargetRunWeight, DeltaSeconds, 9.0f);
				IdleTime += DeltaSeconds * FMath::Lerp(1.0f, 0.45f, Aim);
				RunTime += DeltaSeconds * FMath::Clamp(Speed / 350.0f, 0.35f, 1.6f);
			}
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			Output.ResetToRefPose();
			if (!Idle)
			{
				return true;
			}
			FAnimationPoseData OutputData(Output);
			Idle->GetAnimationPose(OutputData, FAnimExtractContext(static_cast<double>(FMath::Fmod(IdleTime, Idle->GetPlayLength())), false));
			if (Run && RunWeight > KINDA_SMALL_NUMBER)
			{
				FPoseContext RunPose(Output);
				FAnimationPoseData RunData(RunPose);
				Run->GetAnimationPose(RunData, FAnimExtractContext(static_cast<double>(FMath::Fmod(RunTime, Run->GetPlayLength())), false));
				FAnimationRuntime::BlendTwoPosesTogether(OutputData, RunData, 1.0f - RunWeight, OutputData);
			}
			if (Fire && !bReloading && ShotAge < Fire->GetPlayLength())
			{
				FPoseContext FirePose(Output);
				FAnimationPoseData FireData(FirePose);
				Fire->GetAnimationPose(FireData, FAnimExtractContext(static_cast<double>(ShotAge), false));
				const float FireWeight = FMath::Clamp(ShotAge / 0.025f, 0.0f, 1.0f)
					* FMath::Clamp((Fire->GetPlayLength() - ShotAge) / 0.09f, 0.0f, 1.0f);
				FAnimationRuntime::BlendTwoPosesTogether(OutputData, FireData, 1.0f - FireWeight, OutputData);
			}
			AnimateSupportHand(Output);
			Output.Pose.NormalizeRotations();
			return true;
		}

	private:
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Run = nullptr;
		UAnimSequence* Fire = nullptr;
		float IdleTime = 0.0f;
		float RunTime = 0.0f;
		float RunWeight = 0.0f;
		float Aim = 0.0f;
		float ShotAge = BIG_NUMBER;
		float Reload = 0.0f;
		float ReloadWeight = 0.0f;
		bool bReloading = false;
		FTransform WeaponToHand;
		FVector MagazineGrip;
		FVector Foregrip;

		void AnimateSupportHand(FPoseContext& Output) const
		{
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			const auto BoneIndex = [&Bones](FName Name)
			{
				return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name)));
			};
			const FCompactPoseBoneIndex UpperIndex = BoneIndex(TEXT("upperarm_l"));
			const FCompactPoseBoneIndex LowerIndex = BoneIndex(TEXT("lowerarm_l"));
			const FCompactPoseBoneIndex HandIndex = BoneIndex(TEXT("hand_l"));
			const FCompactPoseBoneIndex RightIndex = BoneIndex(TEXT("hand_r"));
			if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE || RightIndex == INDEX_NONE)
			{
				return;
			}
			FCSPose<FCompactPose> ComponentPose;
			ComponentPose.InitPose(Output.Pose);
			// Raise the hands into ADS while keeping the shoulders below the camera near plane.
			const FTransform RightHandTarget = ComponentPose.GetComponentSpaceTransform(RightIndex);
			const FCompactPoseBoneIndex SpineIndex = BoneIndex(TEXT("spine_05"));
			const FCompactPoseBoneIndex RightUpperIndex = BoneIndex(TEXT("upperarm_r"));
			const FCompactPoseBoneIndex RightLowerIndex = BoneIndex(TEXT("lowerarm_r"));
			if (Aim > KINDA_SMALL_NUMBER && SpineIndex != INDEX_NONE && RightUpperIndex != INDEX_NONE && RightLowerIndex != INDEX_NONE)
			{
				FTransform Spine = ComponentPose.GetComponentSpaceTransform(SpineIndex);
				Spine.AddToTranslation(FVector(0, 0, -12.0f * Aim));
				TArray<FBoneTransform> SpineTransform;
				SpineTransform.Emplace(SpineIndex, Spine);
				ComponentPose.LocalBlendCSBoneTransforms(SpineTransform, 1.0f);
				FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(ComponentPose, Output.Pose);
				ComponentPose.InitPose(Output.Pose);
				FTransform RightUpper = ComponentPose.GetComponentSpaceTransform(RightUpperIndex);
				FTransform RightLower = ComponentPose.GetComponentSpaceTransform(RightLowerIndex);
				FTransform RightHand = ComponentPose.GetComponentSpaceTransform(RightIndex);
				AnimationCore::SolveTwoBoneIK(RightUpper, RightLower, RightHand,
					RightUpper.GetLocation() + FVector(-15, 55, -25), RightHandTarget.GetLocation(), false, 1.0, 1.0);
				RightHand.SetRotation(RightHandTarget.GetRotation());
				TArray<FBoneTransform> RightTransforms;
				RightTransforms.Emplace(RightUpperIndex, RightUpper);
				RightTransforms.Emplace(RightLowerIndex, RightLower);
				RightTransforms.Emplace(RightIndex, RightHand);
				ComponentPose.LocalBlendCSBoneTransforms(RightTransforms, 1.0f);
			}
			FTransform Upper = ComponentPose.GetComponentSpaceTransform(UpperIndex);
			FTransform Lower = ComponentPose.GetComponentSpaceTransform(LowerIndex);
			FTransform Hand = ComponentPose.GetComponentSpaceTransform(HandIndex);
			const FTransform Weapon = WeaponToHand * ComponentPose.GetComponentSpaceTransform(RightIndex);
			const FVector Support = Weapon.TransformPosition(Foregrip);
			const FVector Magazine = Weapon.TransformPosition(MagazineGrip);
			const FVector Down = Weapon.TransformVectorNoScale(FVector(0, 0, -1));
			const FVector ChargingHandle = Weapon.TransformPosition(FVector(0, -12, 7));
			const float Times[] = { 0.0f, 0.16f, 0.34f, 0.49f, 0.67f, 0.79f, 0.89f, 1.0f };
			const FVector Points[] = { Support, Magazine, Magazine + Down * 14.0f,
				Magazine + Down * 24.0f, Magazine, ChargingHandle, ChargingHandle + Down * 2.0f, Support };
			int32 Segment = 0;
			while (Segment < UE_ARRAY_COUNT(Times) - 2 && Reload > Times[Segment + 1]) { ++Segment; }
			const float Alpha = FMath::SmoothStep(Times[Segment], Times[Segment + 1], Reload);
			const FVector Target = FMath::Lerp(Support, FMath::Lerp(Points[Segment], Points[Segment + 1], Alpha), ReloadWeight);
			const FVector ElbowTarget = Upper.GetLocation() + FVector(-15, -55, -25);
			AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, ElbowTarget, Target, false, 1.0, 1.0);
			TArray<FBoneTransform> Transforms;
			Transforms.Emplace(UpperIndex, Upper);
			Transforms.Emplace(LowerIndex, Lower);
			Transforms.Emplace(HandIndex, Hand);
			ComponentPose.LocalBlendCSBoneTransforms(Transforms, 1.0f);
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(ComponentPose, Output.Pose);
		}
	};
}

FAnimInstanceProxy* UStealthArmsAnimInstance::CreateAnimInstanceProxy()
{
	return new FStealthArmsProxy(this);
}

void UStealthArmsAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
