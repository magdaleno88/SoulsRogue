#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SoulsItemReward.generated.h"

class USoulsItemComponent;
class USoulsItemDefinition;
class USoulsItemReward;
class USoulsRewardComponent;

UENUM(BlueprintType)
enum class ESoulsItemRewardState : uint8
{
	Available,
	Claimed
};

/** Native failures are explicit; Claim never computes effects or changes Health itself. */
enum class ESoulsRewardClaimResult : uint8
{
	Success,
	Busy,
	AlreadyClaimed,
	InvalidReward,
	InvalidTarget,
	GrantRejected,
	GrantLost
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsRewardClaimedNative, USoulsItemReward*, USoulsItemComponent*, FGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoulsRewardClaimed, USoulsItemReward*, Reward, USoulsItemComponent*, Target, FGuid, GrantId);

/** Standalone transient logical reward. No references to its source Actor, grant or modifiers. */
UCLASS(BlueprintType)
class SOULSNEW_API USoulsItemReward : public UObject
{
	GENERATED_BODY()

public:
	/** Game thread only. Failed/reentrant claims leave State Available and output invalid. */
	ESoulsRewardClaimResult Claim(USoulsItemComponent* Target, FGuid& OutGrantId);

	UFUNCTION(BlueprintPure, Category="SoulsNew|Rewards")
	FGuid GetRewardId() const { return RewardId; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Rewards")
	USoulsItemDefinition* GetItemDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Rewards")
	ESoulsItemRewardState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Rewards")
	FGuid GetClaimedGrantId() const { return ClaimedGrantId; }

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Rewards")
	FSoulsRewardClaimed OnRewardClaimed;

	FSoulsRewardClaimedNative& RewardClaimed() { return RewardClaimedEvent; }

private:
	friend class USoulsRewardComponent;
	static USoulsItemReward* Create(USoulsItemDefinition* InDefinition);

	UPROPERTY(Transient)
	FGuid RewardId;

	UPROPERTY(Transient)
	TObjectPtr<USoulsItemDefinition> Definition;

	UPROPERTY(Transient)
	ESoulsItemRewardState State = ESoulsItemRewardState::Available;

	UPROPERTY(Transient)
	FGuid ClaimedGrantId;

	bool bClaiming = false;
	FSoulsRewardClaimedNative RewardClaimedEvent;
};
