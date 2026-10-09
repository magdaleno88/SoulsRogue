#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulsRewardComponent.generated.h"

class USoulsHealthComponent;
class USoulsItemComponent;
class USoulsItemReward;
class USoulsRewardComponent;

enum class ESoulsRewardSourceResult : uint8
{
	NotGenerated,
	Success,
	Busy,
	Unavailable,
	InvalidOwner,
	IncompatibleOwner,
	InvalidGrant,
	OwnerDead,
	PendingReward
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSoulsRewardGeneratedNative, USoulsRewardComponent*, USoulsItemReward*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSoulsRewardGenerated, USoulsRewardComponent*, RewardComponent, USoulsItemReward*, Reward);

/** Actor-local generation from a selected grant; death subscription is configured explicitly. */
UCLASS(ClassGroup=(SoulsNew), meta=(BlueprintSpawnableComponent))
class SOULSNEW_API USoulsRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USoulsRewardComponent();
	ESoulsRewardSourceResult ConfigureSourceGrant(const FGuid& GrantId);
	/** Health must be alive and any previous reward claimed. Resetting Health does not auto-rearm. */
	ESoulsRewardSourceResult Rearm();
	FGuid GetSourceGrantId() const { return SourceGrantId; }
	bool HasHandledDeath() const { return bDeathHandled; }
	ESoulsRewardSourceResult GetLastGenerationResult() const { return LastGenerationResult; }

	/** Borrowed object. External holders must retain a reflected/strong reference beyond this owner. */
	UFUNCTION(BlueprintPure, Category="SoulsNew|Rewards")
	USoulsItemReward* GetReward() const { return CurrentReward; }

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Rewards")
	FSoulsRewardGenerated OnRewardGenerated;

	FSoulsRewardGeneratedNative& RewardGenerated() { return RewardGeneratedEvent; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UPROPERTY(Transient)
	FGuid SourceGrantId;

	UPROPERTY(Transient)
	TWeakObjectPtr<USoulsHealthComponent> SourceHealth;

	UPROPERTY(Transient)
	TWeakObjectPtr<USoulsItemComponent> SourceItems;

	UPROPERTY(Transient)
	TObjectPtr<USoulsItemReward> CurrentReward;

	FDelegateHandle DeathHandle;
	bool bDeathHandled = false;
	bool bHandlingDeath = false;
	bool bEnded = false;
	ESoulsRewardSourceResult LastGenerationResult = ESoulsRewardSourceResult::NotGenerated;
	FSoulsRewardGeneratedNative RewardGeneratedEvent;
	ESoulsRewardSourceResult ResolveComponents(USoulsHealthComponent*& OutHealth, USoulsItemComponent*& OutItems) const;
	void HandleDeath(USoulsHealthComponent* Health);
	void UnbindDeath();
};
