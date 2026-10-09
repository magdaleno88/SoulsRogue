#include "Items/SoulsRewardComponent.h"

#include "Attributes/SoulsHealthComponent.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "GameFramework/Actor.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

USoulsRewardComponent::USoulsRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ESoulsRewardSourceResult USoulsRewardComponent::ResolveComponents(USoulsHealthComponent*& OutHealth, USoulsItemComponent*& OutItems) const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed()) { return ESoulsRewardSourceResult::InvalidOwner; }
	TArray<USoulsHealthComponent*> HealthComponents;
	TArray<USoulsItemComponent*> ItemComponents;
	Owner->GetComponents<USoulsHealthComponent>(HealthComponents);
	Owner->GetComponents<USoulsItemComponent>(ItemComponents);
	HealthComponents.RemoveAll([](USoulsHealthComponent* Health) { return !IsValid(Health); });
	ItemComponents.RemoveAll([](USoulsItemComponent* Items) { return !IsValid(Items); });
	if (HealthComponents.Num() != 1 || ItemComponents.Num() != 1) { return ESoulsRewardSourceResult::IncompatibleOwner; }
	OutHealth = HealthComponents[0];
	OutItems = ItemComponents[0];
	return ESoulsRewardSourceResult::Success;
}

ESoulsRewardSourceResult USoulsRewardComponent::ConfigureSourceGrant(const FGuid& GrantId)
{
	check(IsInGameThread());
	if (bHandlingDeath) { return ESoulsRewardSourceResult::Busy; }
	if (bEnded) { return ESoulsRewardSourceResult::Unavailable; }
	USoulsHealthComponent* Health = nullptr;
	USoulsItemComponent* Items = nullptr;
	const ESoulsRewardSourceResult Resolved = ResolveComponents(Health, Items);
	if (Resolved != ESoulsRewardSourceResult::Success) { return Resolved; }
	if (Health->IsDead()) { return ESoulsRewardSourceResult::OwnerDead; }
	if (!IsValid(Items->GetGrantDefinition(GrantId))) { return ESoulsRewardSourceResult::InvalidGrant; }

	if (SourceHealth.Get() != Health) { UnbindDeath(); }
	SourceHealth = Health;
	SourceItems = Items;
	SourceGrantId = GrantId;
	if (!DeathHandle.IsValid()) { DeathHandle = Health->Death().AddUObject(this, &USoulsRewardComponent::HandleDeath); }
	return ESoulsRewardSourceResult::Success;
}

ESoulsRewardSourceResult USoulsRewardComponent::Rearm()
{
	check(IsInGameThread());
	if (bHandlingDeath) { return ESoulsRewardSourceResult::Busy; }
	if (bEnded) { return ESoulsRewardSourceResult::Unavailable; }
	USoulsHealthComponent* Health = nullptr;
	USoulsItemComponent* Items = nullptr;
	const ESoulsRewardSourceResult Resolved = ResolveComponents(Health, Items);
	if (Resolved != ESoulsRewardSourceResult::Success) { return Resolved; }
	if (Health != SourceHealth.Get() || Items != SourceItems.Get()) { return ESoulsRewardSourceResult::IncompatibleOwner; }
	if (Health->IsDead()) { return ESoulsRewardSourceResult::OwnerDead; }
	if (!IsValid(Items->GetGrantDefinition(SourceGrantId))) { return ESoulsRewardSourceResult::InvalidGrant; }
	if (CurrentReward && CurrentReward->GetState() != ESoulsItemRewardState::Claimed) { return ESoulsRewardSourceResult::PendingReward; }

	CurrentReward = nullptr;
	bDeathHandled = false;
	LastGenerationResult = ESoulsRewardSourceResult::NotGenerated;
	return ESoulsRewardSourceResult::Success;
}

void USoulsRewardComponent::HandleDeath(USoulsHealthComponent* Health)
{
	check(IsInGameThread());
	if (bEnded || bHandlingDeath || bDeathHandled || Health != SourceHealth.Get() || !Health->IsDead()) { return; }
	TStrongObjectPtr<USoulsRewardComponent> KeepComponentAlive(this);
	TGuardValue<bool> DeathGuard(bHandlingDeath, true);
	// Latch even an invalid selected grant: no repeated attempts/events in the same life.
	bDeathHandled = true;
	USoulsItemComponent* Items = SourceItems.Get();
	if (!Items) { LastGenerationResult = ESoulsRewardSourceResult::IncompatibleOwner; return; }
	USoulsItemDefinition* Definition = Items->GetGrantDefinition(SourceGrantId);
	if (!IsValid(Definition)) { LastGenerationResult = ESoulsRewardSourceResult::InvalidGrant; return; }

	TStrongObjectPtr<USoulsItemReward> Reward(USoulsItemReward::Create(Definition));
	CurrentReward = Reward.Get();
	LastGenerationResult = ESoulsRewardSourceResult::Success;
	RewardGeneratedEvent.Broadcast(this, Reward.Get());
	OnRewardGenerated.Broadcast(this, Reward.Get());
}

void USoulsRewardComponent::UnbindDeath()
{
	if (USoulsHealthComponent* Health = SourceHealth.Get()) { Health->Death().Remove(DeathHandle); }
	DeathHandle.Reset();
}

void USoulsRewardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnded = true;
	UnbindDeath();
	Super::EndPlay(EndPlayReason);
}

void USoulsRewardComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	bEnded = true;
	UnbindDeath();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
