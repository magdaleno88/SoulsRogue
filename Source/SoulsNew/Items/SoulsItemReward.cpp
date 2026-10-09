#include "Items/SoulsItemReward.h"

#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

USoulsItemReward* USoulsItemReward::Create(USoulsItemDefinition* InDefinition)
{
	check(IsInGameThread());
	check(IsValid(InDefinition));
	USoulsItemReward* Reward = NewObject<USoulsItemReward>(GetTransientPackageAsObject(), NAME_None, RF_Transient);
	Reward->RewardId = FGuid::NewGuid();
	Reward->Definition = InDefinition;
	return Reward;
}

ESoulsRewardClaimResult USoulsItemReward::Claim(USoulsItemComponent* Target, FGuid& OutGrantId)
{
	check(IsInGameThread());
	OutGrantId.Invalidate();
	if (bClaiming) { return ESoulsRewardClaimResult::Busy; }
	if (State == ESoulsItemRewardState::Claimed) { return ESoulsRewardClaimResult::AlreadyClaimed; }
	if (!RewardId.IsValid() || !IsValid(Definition)) { return ESoulsRewardClaimResult::InvalidReward; }
	if (!IsValid(Target)) { return ESoulsRewardClaimResult::InvalidTarget; }

	// Callbacks may destroy the source/target or collect garbage; retain this operation's objects.
	TStrongObjectPtr<USoulsItemReward> KeepRewardAlive(this);
	TStrongObjectPtr<USoulsItemComponent> KeepTargetAlive(Target);
	TWeakObjectPtr<USoulsItemComponent> WeakTarget(Target);
	TGuardValue<bool> ClaimGuard(bClaiming, true);
	FGuid NewGrantId;
	if (Target->GrantItem(Definition, NewGrantId) != ESoulsItemResult::Success)
	{
		return ESoulsRewardClaimResult::GrantRejected;
	}
	// An ItemGranted callback may destroy the component and withdraw its grants before returning.
	if (!WeakTarget.IsValid() || !NewGrantId.IsValid()
		|| Target->GetGrantDefinition(NewGrantId) != Definition.Get())
	{
		return ESoulsRewardClaimResult::GrantLost;
	}

	ClaimedGrantId = NewGrantId;
	State = ESoulsItemRewardState::Claimed;
	OutGrantId = NewGrantId;
	RewardClaimedEvent.Broadcast(this, Target, NewGrantId);
	OnRewardClaimed.Broadcast(this, Target, NewGrantId);
	return ESoulsRewardClaimResult::Success;
}
