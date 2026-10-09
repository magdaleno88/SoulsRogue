#include "Items/SoulsItemComponent.h"

#include "Items/SoulsItemDefinition.h"
#include "Attributes/SoulsHealthComponent.h"
#include "Async/Async.h"
#include "GameFramework/Actor.h"
#include "Templates/UnrealTemplate.h"
#include "SoulsNew.h"

namespace
{
	// Missing IDs already have no effect. Never redirect removal to a replacement Health component.
	TArray<FGuid> ExistingModifierIds(USoulsHealthComponent* Health, TConstArrayView<FGuid> Ids)
	{
		TArray<FGuid> Existing;
		FSoulsAttributeModifier Modifier;
		for (const FGuid& Id : Ids)
		{
			if (Health->FindMaxHealthModifier(Id, Modifier)) { Existing.Add(Id); }
		}
		return Existing;
	}
}

USoulsItemComponent::USoulsItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ESoulsItemResult USoulsItemComponent::GrantItem(USoulsItemDefinition* Definition, FGuid& OutGrantId)
{
	check(IsInGameThread());
	OutGrantId.Invalidate();
	if (bMutating) { return ESoulsItemResult::Busy; }
	if (bTearingDown) { return ESoulsItemResult::Unavailable; }
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed()) { return ESoulsItemResult::InvalidOwner; }
	if (!IsValid(Definition)) { return ESoulsItemResult::InvalidDefinition; }
	const FSoulsItemDefinitionData& Data = Definition->GetData();
	if (Data.ItemId.IsNone() || Data.DisplayName.IsEmpty() || Data.Effects.IsEmpty())
	{
		return ESoulsItemResult::InvalidDefinition;
	}
	for (const FSoulsItemEffect& Effect : Data.Effects)
	{
		if (Effect.Attribute != ESoulsItemAttribute::MaxHealth) { return ESoulsItemResult::UnsupportedEffect; }
	}

	TArray<USoulsHealthComponent*> HealthComponents;
	Owner->GetComponents<USoulsHealthComponent>(HealthComponents);
	HealthComponents.RemoveAll([](USoulsHealthComponent* Health) { return !IsValid(Health); });
	if (HealthComponents.IsEmpty()) { return ESoulsItemResult::MissingHealth; }
	if (HealthComponents.Num() != 1) { return ESoulsItemResult::AmbiguousHealth; }
	USoulsHealthComponent* Health = HealthComponents[0];

	FSoulsOwnedItem Grant;
	Grant.Definition = Definition;
	Grant.ItemId = Data.ItemId;
	Grant.GrantId = FGuid::NewGuid();
	Grant.Health = Health;
	TArray<FSoulsAttributeModifier> Modifiers;
	for (const FSoulsItemEffect& Effect : Data.Effects)
	{
		const FGuid ModifierId = FGuid::NewGuid();
		Grant.ModifierIds.Add(ModifierId);
		Modifiers.Add({ ModifierId, Effect.Operation, Effect.Magnitude });
	}

	TGuardValue<bool> MutationGuard(bMutating, true);
	const int32 GrantIndex = OwnedItems.Add(Grant);
	// Health commits all effects before notifications; callbacks see the corresponding possession.
	if (!Health->ApplyMaxHealthModifierChanges(Modifiers, {}))
	{
		OwnedItems.RemoveAt(GrantIndex);
		return ESoulsItemResult::HealthRejected;
	}
	if (bTearingDown)
	{
		// The Health notification scope has ended: roll back this cancelled grant before returning.
		if (USoulsHealthComponent* Target = Grant.Health.Get())
		{
			const TArray<FGuid> Ids = ExistingModifierIds(Target, Grant.ModifierIds);
			if (!Target->ApplyMaxHealthModifierChanges({}, Ids))
			{
				UE_LOG(LogSoulsNew, Warning, TEXT("%s: cancelled item grant rollback failed; teardown cleanup is pending."), *Target->GetPathName());
			}
		}
		return ESoulsItemResult::Unavailable;
	}
	OutGrantId = Grant.GrantId;
	ItemGrantedEvent.Broadcast(this, Grant.GrantId, Grant.ItemId);
	OnItemGranted.Broadcast(this, Grant.GrantId, Grant.ItemId);
	return ESoulsItemResult::Success;
}

ESoulsItemResult USoulsItemComponent::RemoveItem(const FGuid& GrantId)
{
	check(IsInGameThread());
	if (bMutating) { return ESoulsItemResult::Busy; }
	if (bTearingDown) { return ESoulsItemResult::Unavailable; }
	const int32 Index = OwnedItems.IndexOfByPredicate([&GrantId](const FSoulsOwnedItem& Grant) { return Grant.GrantId == GrantId; });
	if (Index == INDEX_NONE) { return ESoulsItemResult::InvalidGrant; }

	TGuardValue<bool> MutationGuard(bMutating, true);
	const FSoulsOwnedItem Removed = OwnedItems[Index];
	OwnedItems.RemoveAt(Index);
	if (USoulsHealthComponent* Health = Removed.Health.Get())
	{
		const TArray<FGuid> Ids = ExistingModifierIds(Health, Removed.ModifierIds);
		if (!Health->ApplyMaxHealthModifierChanges({}, Ids))
		{
			OwnedItems.Insert(Removed, Index);
			return ESoulsItemResult::HealthRejected;
		}
	}
	ItemRemovedEvent.Broadcast(this, Removed.GrantId, Removed.ItemId);
	OnItemRemoved.Broadcast(this, Removed.GrantId, Removed.ItemId);
	return ESoulsItemResult::Success;
}

bool USoulsItemComponent::FindGrant(const FGuid& GrantId, FSoulsItemGrantSnapshot& OutGrant) const
{
	check(IsInGameThread());
	const FSoulsOwnedItem* Found = OwnedItems.FindByPredicate([&GrantId](const FSoulsOwnedItem& Grant) { return Grant.GrantId == GrantId; });
	if (!Found) { return false; }
	OutGrant = { Found->GrantId, Found->ItemId, Found->ModifierIds };
	return true;
}

bool USoulsItemComponent::HasGrant(FGuid GrantId) const
{
	return OwnedItems.ContainsByPredicate([&GrantId](const FSoulsOwnedItem& Grant) { return Grant.GrantId == GrantId; });
}

bool USoulsItemComponent::HasItem(FName ItemId) const
{
	return OwnedItems.ContainsByPredicate([ItemId](const FSoulsOwnedItem& Grant) { return Grant.ItemId == ItemId; });
}

void USoulsItemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseItemsForTeardown();
	Super::EndPlay(EndPlayReason);
}

void USoulsItemComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ReleaseItemsForTeardown();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USoulsItemComponent::ReleaseItemsForTeardown()
{
	if (bTearingDown) { return; }
	bTearingDown = true;
	// Teardown is not a gameplay Remove event. Retain only weak targets and exact IDs for cleanup.
	TMap<TWeakObjectPtr<USoulsHealthComponent>, TArray<FGuid>> ByHealth;
	for (const FSoulsOwnedItem& Grant : OwnedItems) { ByHealth.FindOrAdd(Grant.Health).Append(Grant.ModifierIds); }
	OwnedItems.Reset();
	for (const auto& Pair : ByHealth)
	{
		USoulsHealthComponent* Health = Pair.Key.Get();
		if (!Health) { continue; }
		const TArray<FGuid> Existing = ExistingModifierIds(Health, Pair.Value);
		if (Health->ApplyMaxHealthModifierChanges({}, Existing)) { continue; }
		// Destruction can be requested inside a Health callback: defer once beyond that call stack.
		AsyncTask(ENamedThreads::GameThread, [WeakHealth = Pair.Key, Ids = Pair.Value]()
		{
			if (USoulsHealthComponent* Target = WeakHealth.Get())
			{
				const TArray<FGuid> Remaining = ExistingModifierIds(Target, Ids);
				if (!Target->ApplyMaxHealthModifierChanges({}, Remaining))
				{
					UE_LOG(LogSoulsNew, Warning, TEXT("%s: item teardown could not remove owned health modifiers."), *Target->GetPathName());
				}
			}
		});
	}
}
