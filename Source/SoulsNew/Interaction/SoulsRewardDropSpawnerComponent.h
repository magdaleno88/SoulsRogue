#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulsRewardDropSpawnerComponent.generated.h"

class ASoulsItemPickup;
class USoulsItemReward;
class USoulsRewardComponent;
class USoulsRewardDropSpawnerComponent;

UENUM(BlueprintType)
enum class ESoulsRewardDropState : uint8
{
	Idle,
	PendingSpawn,
	PickupActive,
	Completed
};

enum class ESoulsRewardDropResult : uint8
{
	Success,
	Busy,
	Unavailable,
	InvalidReward,
	RewardPending,
	AlreadyActive,
	AlreadyClaimed,
	NoPendingReward,
	InvalidWorld,
	InvalidPickupClass,
	InvalidTransform,
	SpawnFailed,
	InitializationFailed
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsPickupSpawnedNative, USoulsRewardDropSpawnerComponent*, ASoulsItemPickup*, USoulsItemReward*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoulsPickupSpawned, USoulsRewardDropSpawnerComponent*, Spawner, ASoulsItemPickup*, Pickup, USoulsItemReward*, Reward);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsPickupSpawnFailedNative, USoulsRewardDropSpawnerComponent*, USoulsItemReward*, ESoulsRewardDropResult);

/** Actor-local logical-to-physical adapter. Owns no item effects or reward-generation rules. */
UCLASS(ClassGroup=(SoulsNew), meta=(BlueprintSpawnableComponent))
class SOULSNEW_API USoulsRewardDropSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USoulsRewardDropSpawnerComponent();
	/** Automatic at registration/BeginPlay; explicit refresh supports later component composition. */
	bool RefreshRewardBinding();
	/** Game thread only. Reject a different reward while an available reward is retained. */
	ESoulsRewardDropResult TrySpawnRewardPickup(USoulsItemReward* InReward);
	ESoulsRewardDropResult TrySpawnPendingReward();
	bool SetPickupClass(TSubclassOf<ASoulsItemPickup> InClass);
	bool SetSpawnOffset(const FVector& InOffset);

	UFUNCTION(BlueprintPure, Category="SoulsNew|Drops")
	ESoulsRewardDropState GetDropState() const;
	UFUNCTION(BlueprintPure, Category="SoulsNew|Drops")
	USoulsItemReward* GetPendingReward() const;
	UFUNCTION(BlueprintPure, Category="SoulsNew|Drops")
	ASoulsItemPickup* GetActivePickup() const;

	ESoulsRewardDropResult GetLastSpawnResult() const { return LastSpawnResult; }
	FSoulsPickupSpawnedNative& PickupSpawned() { return PickupSpawnedEvent; }
	FSoulsPickupSpawnFailedNative& PickupSpawnFailed() { return PickupSpawnFailedEvent; }
	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Drops")
	FSoulsPickupSpawned OnPickupSpawned;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void OnUnregister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UPROPERTY(EditAnywhere, Category="SoulsNew|Drops")
	TSubclassOf<ASoulsItemPickup> PickupClass;
	/** Owner-local offset, including owner rotation and scale; pickup scale is always one. */
	UPROPERTY(EditAnywhere, Category="SoulsNew|Drops")
	FVector SpawnOffset = FVector(0.0, 0.0, 100.0);
	/** Retained for pending retries and unclaimed pickup destruction; not a second reward. */
	UPROPERTY(Transient)
	TObjectPtr<USoulsItemReward> RetainedReward;
	UPROPERTY(Transient)
	TWeakObjectPtr<ASoulsItemPickup> ActivePickup;
	UPROPERTY(Transient)
	TWeakObjectPtr<USoulsRewardComponent> RewardSource;

	FDelegateHandle GeneratedHandle;
	bool bSpawning = false;
	bool bEnded = false;
	ESoulsRewardDropResult LastSpawnResult = ESoulsRewardDropResult::NoPendingReward;
	FSoulsPickupSpawnedNative PickupSpawnedEvent;
	FSoulsPickupSpawnFailedNative PickupSpawnFailedEvent;
	void HandleRewardGenerated(USoulsRewardComponent* Source, USoulsItemReward* Reward);
	void UnbindReward();
	ESoulsRewardDropResult FailSpawn(ESoulsRewardDropResult Reason);
};
