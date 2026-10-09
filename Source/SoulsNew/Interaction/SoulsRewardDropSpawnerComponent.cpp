#include "Interaction/SoulsRewardDropSpawnerComponent.h"

#include "Engine/World.h"
#include "Interaction/SoulsItemPickup.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

USoulsRewardDropSpawnerComponent::USoulsRewardDropSpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PickupClass = ASoulsItemPickup::StaticClass();
}

bool USoulsRewardDropSpawnerComponent::RefreshRewardBinding()
{
	check(IsInGameThread());
	if (bSpawning || bEnded || IsBeingDestroyed()) { return false; }
	AActor* Owner = GetOwner();
	TArray<USoulsRewardComponent*> Sources;
	if (IsValid(Owner) && !Owner->IsActorBeingDestroyed()) { Owner->GetComponents(Sources); }
	Sources.RemoveAll([](USoulsRewardComponent* Source) { return !IsValid(Source) || Source->IsBeingDestroyed(); });
	if (Sources.Num() != 1) { UnbindReward(); return false; }
	if (RewardSource.Get() != Sources[0] || !GeneratedHandle.IsValid())
	{
		UnbindReward();
		RewardSource = Sources[0];
		GeneratedHandle = Sources[0]->RewardGenerated().AddUObject(this, &USoulsRewardDropSpawnerComponent::HandleRewardGenerated);
	}
	// Late composition may discover a reward already generated; the same guarded operation applies.
	if (USoulsItemReward* Reward = Sources[0]->GetReward()) { TrySpawnRewardPickup(Reward); }
	return true;
}

ESoulsRewardDropState USoulsRewardDropSpawnerComponent::GetDropState() const
{
	if (!RetainedReward) { return ESoulsRewardDropState::Idle; }
	if (RetainedReward->GetState() == ESoulsItemRewardState::Claimed) { return ESoulsRewardDropState::Completed; }
	ASoulsItemPickup* Pickup = ActivePickup.Get();
	return IsValid(Pickup) && !Pickup->IsActorBeingDestroyed()
		? ESoulsRewardDropState::PickupActive : ESoulsRewardDropState::PendingSpawn;
}

USoulsItemReward* USoulsRewardDropSpawnerComponent::GetPendingReward() const
{
	return GetDropState() == ESoulsRewardDropState::PendingSpawn ? RetainedReward.Get() : nullptr;
}

ASoulsItemPickup* USoulsRewardDropSpawnerComponent::GetActivePickup() const
{
	return GetDropState() == ESoulsRewardDropState::PickupActive ? ActivePickup.Get() : nullptr;
}

bool USoulsRewardDropSpawnerComponent::SetPickupClass(TSubclassOf<ASoulsItemPickup> InClass)
{
	check(IsInGameThread());
	if (bSpawning || bEnded || IsBeingDestroyed()) { return false; }
	PickupClass = InClass;
	return true;
}

bool USoulsRewardDropSpawnerComponent::SetSpawnOffset(const FVector& InOffset)
{
	check(IsInGameThread());
	if (bSpawning || bEnded || IsBeingDestroyed() || InOffset.ContainsNaN()) { return false; }
	SpawnOffset = InOffset;
	return true;
}

ESoulsRewardDropResult USoulsRewardDropSpawnerComponent::TrySpawnPendingReward()
{
	check(IsInGameThread());
	if (bSpawning) { return ESoulsRewardDropResult::Busy; }
	if (bEnded || IsBeingDestroyed()) { return ESoulsRewardDropResult::Unavailable; }
	if (!RetainedReward) { return ESoulsRewardDropResult::NoPendingReward; }
	return TrySpawnRewardPickup(RetainedReward);
}

ESoulsRewardDropResult USoulsRewardDropSpawnerComponent::TrySpawnRewardPickup(USoulsItemReward* InReward)
{
	check(IsInGameThread());
	if (bSpawning) { return ESoulsRewardDropResult::Busy; }
	if (bEnded || IsBeingDestroyed()) { return ESoulsRewardDropResult::Unavailable; }
	if (!IsValid(InReward) || !InReward->GetRewardId().IsValid() || !IsValid(InReward->GetItemDefinition()))
	{
		return ESoulsRewardDropResult::InvalidReward;
	}
	if (InReward->GetState() == ESoulsItemRewardState::Claimed) { return ESoulsRewardDropResult::AlreadyClaimed; }
	if (RetainedReward == InReward && GetActivePickup()) { return ESoulsRewardDropResult::AlreadyActive; }
	if (RetainedReward && RetainedReward != InReward && RetainedReward->GetState() == ESoulsItemRewardState::Available)
	{
		return ESoulsRewardDropResult::RewardPending;
	}

	TStrongObjectPtr<USoulsRewardDropSpawnerComponent> KeepSpawnerAlive(this);
	TStrongObjectPtr<USoulsItemReward> KeepRewardAlive(InReward);
	TGuardValue<bool> SpawnGuard(bSpawning, true);
	RetainedReward = InReward; // Commit retention before every fallible spawn step.
	ActivePickup.Reset();
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed() || !IsValid(World) || World->bIsTearingDown)
	{
		return FailSpawn(ESoulsRewardDropResult::InvalidWorld);
	}
	UClass* Class = PickupClass.Get();
	if (!IsValid(Class) || !Class->IsChildOf(ASoulsItemPickup::StaticClass())
		|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return FailSpawn(ESoulsRewardDropResult::InvalidPickupClass);
	}
	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FTransform SpawnTransform(OwnerTransform.GetRotation(), OwnerTransform.TransformPosition(SpawnOffset), FVector::OneVector);
	if (OwnerTransform.ContainsNaN() || SpawnOffset.ContainsNaN() || SpawnTransform.ContainsNaN())
	{
		return FailSpawn(ESoulsRewardDropResult::InvalidTransform);
	}
	// No Owner/Instigator: the physical reward has no lifetime dependency on the source.
	TStrongObjectPtr<ASoulsItemPickup> Pickup(World->SpawnActorDeferred<ASoulsItemPickup>(
		Class, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	if (!IsValid(Pickup.Get()) || Pickup->IsActorBeingDestroyed()) { return FailSpawn(ESoulsRewardDropResult::SpawnFailed); }
	// InitializeReward enables the sphere; hold actor interaction off until construction finishes.
	Pickup->SetActorEnableCollision(false);
	if (!Pickup->InitializeReward(InReward))
	{
		Pickup->Destroy();
		return FailSpawn(ESoulsRewardDropResult::InitializationFailed);
	}
	Pickup->FinishSpawning(SpawnTransform);
	if (!IsValid(Pickup.Get()) || Pickup->IsActorBeingDestroyed() || Pickup->GetReward() != InReward)
	{
		if (IsValid(Pickup.Get()) && !Pickup->IsActorBeingDestroyed()) { Pickup->Destroy(); }
		return FailSpawn(ESoulsRewardDropResult::SpawnFailed);
	}
	ActivePickup = Pickup.Get();
	LastSpawnResult = ESoulsRewardDropResult::Success;
	// Activating may synchronously claim/destroy this now fully constructed pickup.
	Pickup->SetActorEnableCollision(true);
	if (!bEnded && !IsBeingDestroyed())
	{
		PickupSpawnedEvent.Broadcast(this, Pickup.Get(), InReward);
		if (!bEnded && !IsBeingDestroyed()) { OnPickupSpawned.Broadcast(this, Pickup.Get(), InReward); }
	}
	return ESoulsRewardDropResult::Success;
}

ESoulsRewardDropResult USoulsRewardDropSpawnerComponent::FailSpawn(ESoulsRewardDropResult Reason)
{
	LastSpawnResult = Reason;
	if (!bEnded && !IsBeingDestroyed()) { PickupSpawnFailedEvent.Broadcast(this, RetainedReward, Reason); }
	return Reason;
}

void USoulsRewardDropSpawnerComponent::HandleRewardGenerated(USoulsRewardComponent* Source, USoulsItemReward* Reward)
{
	if (Source == RewardSource.Get()) { TrySpawnRewardPickup(Reward); }
}

void USoulsRewardDropSpawnerComponent::UnbindReward()
{
	if (USoulsRewardComponent* Source = RewardSource.Get()) { Source->RewardGenerated().Remove(GeneratedHandle); }
	GeneratedHandle.Reset();
	RewardSource.Reset();
}

void USoulsRewardDropSpawnerComponent::OnRegister()
{
	Super::OnRegister();
	if (GetWorld() && GetWorld()->IsGameWorld()) { RefreshRewardBinding(); }
}

void USoulsRewardDropSpawnerComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshRewardBinding();
}

void USoulsRewardDropSpawnerComponent::OnUnregister()
{
	UnbindReward();
	Super::OnUnregister();
}

void USoulsRewardDropSpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnded = true;
	UnbindReward();
	Super::EndPlay(EndPlayReason);
}

void USoulsRewardDropSpawnerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	bEnded = true;
	UnbindReward();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
