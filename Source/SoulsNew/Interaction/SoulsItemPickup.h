#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPtr.h"
#include "SoulsItemPickup.generated.h"

class USoulsItemReward;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ASoulsItemPickup;

enum class ESoulsPickupClaimResult : uint8
{
	Success,
	Busy,
	Unavailable,
	InvalidReward,
	AlreadyClaimed,
	InvalidTarget,
	MissingItemComponent,
	AmbiguousItemComponents,
	ClaimRejected
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsPickupClaimedNative, ASoulsItemPickup*, AActor*, FGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoulsPickupClaimed, ASoulsItemPickup*, Pickup, AActor*, Target, FGuid, GrantId);

/** Physical presentation of an existing logical reward. All effects remain behind Reward::Claim. */
UCLASS()
class SOULSNEW_API ASoulsItemPickup : public AActor
{
	GENERATED_BODY()

public:
	ASoulsItemPickup();

	/** One assignment per pickup. Invalid/repeated assignments never replace a retained reward. */
	bool InitializeReward(USoulsItemReward* InReward);
	/** Game thread only. Output is valid only on success. Overlap uses this same operation. */
	ESoulsPickupClaimResult TryClaimForActor(AActor* Candidate, FGuid& OutGrantId);

	UFUNCTION(BlueprintPure, Category="SoulsNew|Pickups")
	USoulsItemReward* GetReward() const { return Reward; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Pickups")
	bool IsConsumed() const { return bConsumed; }

	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }
	UStaticMeshComponent* GetVisualMesh() const { return VisualMesh; }

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Pickups")
	FSoulsPickupClaimed OnPickupClaimed;
	FSoulsPickupClaimedNative& PickupClaimed() { return PickupClaimedEvent; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category="SoulsNew|Pickups")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, Category="SoulsNew|Pickups")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Included engine shape; soft reference loads once per instance during construction. */
	UPROPERTY(EditDefaultsOnly, Category="SoulsNew|Pickups")
	TSoftObjectPtr<UStaticMesh> VisualMeshAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoulsItemReward> Reward;

	UPROPERTY(Transient)
	bool bConsumed = false;

	bool bClaiming = false;
	bool bEnded = false;
	FSoulsPickupClaimedNative PickupClaimedEvent;
	void DisableInteraction();

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
