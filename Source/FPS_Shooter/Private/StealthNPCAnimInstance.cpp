#include "StealthNPCAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "SoundHearingNPC.h"

namespace
{
	class FStealthNPCProxy : public FAnimInstanceProxy
	{
	public:
		explicit FStealthNPCProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			if (const ASoundHearingNPC* NPC = Cast<ASoundHearingNPC>(Instance->TryGetPawnOwner()))
			{
				Idle = NPC->GetIdleAnimation(); Walk = NPC->GetWalkAnimation(); Run = NPC->GetRunAnimation();
				Fire = NPC->GetFireAnimation(); Hit = NPC->GetHitAnimation(); Death = NPC->GetDeathAnimation();
				FireAge = NPC->GetFireAnimationAge(); HitAge = NPC->GetHitAnimationAge();
				DeathAge = NPC->GetDeathAnimationAge(); bDead = NPC->IsDead();
				const float Speed = NPC->GetVelocity().Size2D();
				MoveWeight = FMath::FInterpTo(MoveWeight, FMath::Clamp(Speed / 100.0f, 0.0f, 1.0f), DeltaSeconds, 8.0f);
				RunWeight = FMath::FInterpTo(RunWeight, FMath::Clamp((Speed - 230.0f) / 140.0f, 0.0f, 1.0f), DeltaSeconds, 8.0f);
				IdleTime += DeltaSeconds;
				MoveTime += DeltaSeconds * FMath::Clamp(Speed / 210.0f, 0.5f, 1.8f);
			}
		}
		virtual bool Evaluate(FPoseContext& Output) override
		{
			Output.ResetToRefPose();
			if (!Idle) { return true; }
			Sample(Idle, IdleTime, true, Output);
			if (Walk && MoveWeight > KINDA_SMALL_NUMBER)
			{
				FPoseContext Moving(Output);
				Sample(Walk, MoveTime, true, Moving);
				if (Run && RunWeight > KINDA_SMALL_NUMBER)
				{
					FPoseContext Running(Output);
					Sample(Run, MoveTime, true, Running);
					FAnimationPoseData MovingData(Moving), RunningData(Running);
					FAnimationRuntime::BlendTwoPosesTogether(MovingData, RunningData, 1.0f - RunWeight, MovingData);
				}
				FAnimationPoseData OutputData(Output), MovingData(Moving);
				FAnimationRuntime::BlendTwoPosesTogether(OutputData, MovingData, 1.0f - MoveWeight, OutputData);
			}
			UpperBodyOverlay(Fire, FireAge, 0.025f, 0.08f, 1.0f, Output);
			UpperBodyOverlay(Hit, HitAge, 0.04f, 0.18f, 0.7f, Output);
			if (bDead && Death)
			{
				FPoseContext Dying(Output);
				Sample(Death, DeathAge, false, Dying);
				FAnimationPoseData OutputData(Output), DeathData(Dying);
				FAnimationRuntime::BlendTwoPosesTogether(OutputData, DeathData,
					1.0f - FMath::Clamp(DeathAge / 0.12f, 0.0f, 1.0f), OutputData);
			}
			Output.Pose.NormalizeRotations();
			return true;
		}
	private:
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Walk = nullptr;
		UAnimSequence* Run = nullptr;
		UAnimSequence* Fire = nullptr;
		UAnimSequence* Hit = nullptr;
		UAnimSequence* Death = nullptr;
		float IdleTime = 0, MoveTime = 0, MoveWeight = 0, RunWeight = 0;
		float FireAge = BIG_NUMBER, HitAge = BIG_NUMBER, DeathAge = 0;
		bool bDead = false;
		static void Sample(UAnimSequence* Sequence, float Time, bool bLoop, FPoseContext& Pose)
		{
			const float Length = FMath::Max(KINDA_SMALL_NUMBER, Sequence->GetPlayLength());
			const double Position = bLoop ? FMath::Fmod(Time, Length) : FMath::Clamp(Time, 0.0f, Length);
			FAnimationPoseData Data(Pose);
			Sequence->GetAnimationPose(Data, FAnimExtractContext(Position, false));
		}
		void UpperBodyOverlay(UAnimSequence* Clip, float Age, float FadeIn, float FadeOut, float Weight, FPoseContext& Output) const
		{
			if (!Clip || bDead || Age >= Clip->GetPlayLength()) { return; }
			FPoseContext Overlay(Output);
			Sample(Clip, Age, false, Overlay);
			const float Alpha = Weight * FMath::Clamp(Age / FadeIn, 0.0f, 1.0f)
				* FMath::Clamp((Clip->GetPlayLength() - Age) / FadeOut, 0.0f, 1.0f);
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			const FCompactPoseBoneIndex Spine = Bones.MakeCompactPoseIndex(
				FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("spine_01"))));
			if (Spine == INDEX_NONE) { return; }
			for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				FCompactPoseBoneIndex Ancestor = Bone;
				while (Ancestor != INDEX_NONE && Ancestor != Spine) { Ancestor = Bones.GetParentBoneIndex(Ancestor); }
				if (Ancestor == Spine) { Output.Pose[Bone].BlendWith(Overlay.Pose[Bone], Alpha); }
			}
		}
	};
}

FAnimInstanceProxy* UStealthNPCAnimInstance::CreateAnimInstanceProxy() { return new FStealthNPCProxy(this); }
void UStealthNPCAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
