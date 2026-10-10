#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "SoulsAnimNotifyState_MeleeWindow.generated.h"

/** Stateless timing adapter. Never retains an owner, calculates damage or stores per-attack state. */
UCLASS(meta=(DisplayName="Souls Melee Window"))
class SOULSNEW_API USoulsAnimNotifyState_MeleeWindow : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	USoulsAnimNotifyState_MeleeWindow();
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Duration, const FAnimNotifyEventReference& Reference) override;
	virtual void NotifyTick(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float Delta, const FAnimNotifyEventReference& Reference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload) override;
	virtual void BranchingPointNotifyTick(FBranchingPointNotifyPayload& Payload, float Delta) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload) override;
};
