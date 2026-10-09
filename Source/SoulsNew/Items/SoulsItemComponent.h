#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulsItemComponent.generated.h"

class USoulsHealthComponent;
class USoulsItemDefinition;
class USoulsItemComponent;

/** Native results keep failure causes explicit; no inventory/compatibility policy implied. */
enum class ESoulsItemResult : uint8
{
	Success,
	Busy,
	Unavailable,
	InvalidOwner,
	InvalidDefinition,
	UnsupportedEffect,
	MissingHealth,
	AmbiguousHealth,
	HealthRejected,
	InvalidGrant
};

/** Read-only-by-copy diagnostic view; no pointer into the possession registry. */
struct SOULSNEW_API FSoulsItemGrantSnapshot
{
	FGuid GrantId;
	FName ItemId;
	TArray<FGuid> ModifierIds;
};

/** GC-tracked runtime record, never stored in a definition. No Blueprint exposure. */
USTRUCT()
struct FSoulsOwnedItem
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USoulsItemDefinition> Definition;

	UPROPERTY(Transient)
	FGuid GrantId;

	UPROPERTY(Transient)
	FName ItemId;

	UPROPERTY(Transient)
	TWeakObjectPtr<USoulsHealthComponent> Health;

	UPROPERTY(Transient)
	TArray<FGuid> ModifierIds;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsItemChangedNative, USoulsItemComponent*, FGuid, FName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoulsItemChanged, USoulsItemComponent*, ItemComponent, FGuid, GrantId, FName, ItemId);

/** Actor-local passive possession. No Character/controller casts, Tick, RNG loadouts or persistence. */
UCLASS(ClassGroup=(SoulsNew), meta=(BlueprintSpawnableComponent))
class SOULSNEW_API USoulsItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USoulsItemComponent();

	/** C++ authority. Each call creates an independent grant; output is invalid on failure. */
	ESoulsItemResult GrantItem(USoulsItemDefinition* Definition, FGuid& OutGrantId);
	ESoulsItemResult RemoveItem(const FGuid& GrantId);
	bool FindGrant(const FGuid& GrantId, FSoulsItemGrantSnapshot& OutGrant) const;
	/** Borrowed immutable definition; callers retaining it must use a GC-safe reference. */
	USoulsItemDefinition* GetGrantDefinition(const FGuid& GrantId) const;

	UFUNCTION(BlueprintPure, Category="SoulsNew|Items")
	int32 GetGrantCount() const { return OwnedItems.Num(); }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Items")
	bool HasGrant(FGuid GrantId) const;

	UFUNCTION(BlueprintPure, Category="SoulsNew|Items")
	bool HasItem(FName ItemId) const;

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Items")
	FSoulsItemChanged OnItemGranted;

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Items")
	FSoulsItemChanged OnItemRemoved;

	FSoulsItemChangedNative& ItemGranted() { return ItemGrantedEvent; }
	FSoulsItemChangedNative& ItemRemoved() { return ItemRemovedEvent; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UPROPERTY(Transient)
	TArray<FSoulsOwnedItem> OwnedItems;

	bool bMutating = false;
	bool bTearingDown = false;
	FSoulsItemChangedNative ItemGrantedEvent;
	FSoulsItemChangedNative ItemRemovedEvent;
	void ReleaseItemsForTeardown();
};
