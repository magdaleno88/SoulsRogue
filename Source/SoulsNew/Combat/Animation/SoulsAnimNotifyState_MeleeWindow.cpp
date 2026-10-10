#include "Combat/Animation/SoulsAnimNotifyState_MeleeWindow.h"

#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/SoulsMeleeCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	USoulsMeleeCombatComponent* Combat(USkeletalMeshComponent* Mesh)
	{
		if (!IsValid(Mesh) || !IsValid(Mesh->GetOwner())) { return nullptr; }
		TArray<USoulsMeleeCombatComponent*> Components;
		Mesh->GetOwner()->GetComponents(Components);
		Components.RemoveAll([](USoulsMeleeCombatComponent* Component) { return !IsValid(Component); });
		return Components.Num() == 1 ? Components[0] : nullptr;
	}
	int32 ExecutionId(const FAnimNotifyEventReference& Reference)
	{
		const auto* Context = Reference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
		return Context ? Context->MontageInstanceID : INDEX_NONE;
	}
}

USoulsAnimNotifyState_MeleeWindow::USoulsAnimNotifyState_MeleeWindow()
{
	bIsNativeBranchingPoint = true; // Precise montage timing and direct per-execution provenance.
	NotifyStateBehaviorFlags = static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
	NotifyColor = FColor::Orange;
#endif
}

FString USoulsAnimNotifyState_MeleeWindow::GetNotifyName_Implementation() const { return TEXT("Souls Melee Window"); }

void USoulsAnimNotifyState_MeleeWindow::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference& Reference)
{
	if (auto* C = Combat(Mesh)) { C->BeginAttackWindow(Mesh, Animation, ExecutionId(Reference)); }
}
void USoulsAnimNotifyState_MeleeWindow::NotifyTick(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference& Reference)
{
	if (auto* C = Combat(Mesh)) { C->ProcessAttackWindow(Mesh, Animation, ExecutionId(Reference)); }
}
void USoulsAnimNotifyState_MeleeWindow::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	if (auto* C = Combat(Mesh)) { C->EndAttackWindow(Mesh, Animation, ExecutionId(Reference)); }
}

void USoulsAnimNotifyState_MeleeWindow::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload)
{
	if (auto* C = Combat(Payload.SkelMeshComponent)) { C->BeginAttackWindow(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.MontageInstanceID); }
}
void USoulsAnimNotifyState_MeleeWindow::BranchingPointNotifyTick(FBranchingPointNotifyPayload& Payload, float)
{
	if (auto* C = Combat(Payload.SkelMeshComponent)) { C->ProcessAttackWindow(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.MontageInstanceID); }
}
void USoulsAnimNotifyState_MeleeWindow::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload)
{
	if (auto* C = Combat(Payload.SkelMeshComponent)) { C->EndAttackWindow(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.MontageInstanceID); }
}
