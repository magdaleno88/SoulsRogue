#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPtr.h"
#include "SoulsPrototypeBoss.generated.h"

class UCapsuleComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USoulsHealthComponent;
class USoulsItemComponent;
class USoulsItemDefinition;
class USoulsItemReward;
class USoulsRewardComponent;
class USoulsRewardDropSpawnerComponent;
class ASoulsItemPickup;

/** Inert vertical-slice fixture for manual placement. Not the production boss architecture. */
UCLASS()
class SOULSNEW_API ASoulsPrototypeBoss : public AActor
{
	GENERATED_BODY()
public:
	ASoulsPrototypeBoss();
	USoulsHealthComponent* GetSoulsHealth() const { return Health; }
	USoulsItemComponent* GetSoulsItems() const { return Items; }
	USoulsRewardComponent* GetRewards() const { return Rewards; }
	USoulsRewardDropSpawnerComponent* GetDropSpawner() const { return DropSpawner; }
	USoulsItemDefinition* GetPrototypeDefinition() const { return PrototypeItemDefinition; }
	/** Configure before BeginPlay (including deferred spawn). No runtime loadout replacement. */
	bool SetPrototypeItemDefinition(USoulsItemDefinition* Definition);
	FGuid GetPrototypeGrantId() const { return PrototypeGrantId; }
	bool IsPrototypeReady() const { return bPrototypeReady; }
	UStaticMeshComponent* GetPrototypeMesh() const { return VisualMesh; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category="Prototype")
	TObjectPtr<UCapsuleComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category="Prototype")
	TObjectPtr<UStaticMeshComponent> VisualMesh;
	UPROPERTY(VisibleAnywhere, Category="Prototype")
	TObjectPtr<USoulsHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category="Prototype")
	TObjectPtr<USoulsItemComponent> Items;
	UPROPERTY(VisibleAnywhere, Category="Prototype")
	TObjectPtr<USoulsRewardComponent> Rewards;
	UPROPERTY(VisibleAnywhere, Category="Prototype")
	TObjectPtr<USoulsRewardDropSpawnerComponent> DropSpawner;
	UPROPERTY(EditDefaultsOnly, Category="Prototype")
	TSoftObjectPtr<UStaticMesh> VisualMeshAsset;
	/** Strong authored reference: serialized actors retain and cook their configured definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype|Items", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoulsItemDefinition> PrototypeItemDefinition;
	UPROPERTY(Transient)
	FGuid PrototypeGrantId;
	bool bPrototypeReady = false;
	FDelegateHandle DeathVisualHandle;
#if !UE_BUILD_SHIPPING
	FDelegateHandle RewardLogHandle;
	FDelegateHandle SpawnLogHandle;
	FDelegateHandle SpawnFailureLogHandle;
	void LogRewardGenerated(USoulsRewardComponent* Source, USoulsItemReward* Reward);
	void LogPickupSpawned(USoulsRewardDropSpawnerComponent* Spawner, ASoulsItemPickup* Pickup, USoulsItemReward* Reward);
#endif
	void HandleDeathVisual(USoulsHealthComponent* Sender);
};
