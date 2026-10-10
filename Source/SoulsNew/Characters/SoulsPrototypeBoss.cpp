#include "Characters/SoulsPrototypeBoss.h"

#include "Attributes/SoulsHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/SoulsItemPickup.h"
#include "Interaction/SoulsRewardDropSpawnerComponent.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "SoulsNew.h"

ASoulsPrototypeBoss::ASoulsPrototypeBoss()
{
	PrimaryActorTick.bCanEverTick = false;
	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PrototypeCollision"));
	SetRootComponent(Collision);
	Collision->InitCapsuleSize(50.0f, 100.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeVisual"));
	VisualMesh->SetupAttachment(Collision);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetRelativeScale3D(FVector(1, 1, 2));
	VisualMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Health = CreateDefaultSubobject<USoulsHealthComponent>(TEXT("SoulsHealth"));
	Items = CreateDefaultSubobject<USoulsItemComponent>(TEXT("SoulsItems"));
	Rewards = CreateDefaultSubobject<USoulsRewardComponent>(TEXT("SoulsRewards"));
	DropSpawner = CreateDefaultSubobject<USoulsRewardDropSpawnerComponent>(TEXT("SoulsDropSpawner"));
}

void ASoulsPrototypeBoss::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Placed actors can be constructed on the async loading thread in a cooked game.
	// Configure spawned/editor instances here; BeginPlay also configures loaded actors
	// on the game thread before granting their item or marking the prototype ready.
	if (!HasActorBegunPlay())
	{
		Health->SetBaseMaxHealth(500.0f);
		DropSpawner->SetSpawnOffset(FVector(160, 0, -55));
	}
	if (!VisualMesh->GetStaticMesh() && !VisualMeshAsset.IsNull())
	{
		if (UStaticMesh* Mesh = VisualMeshAsset.LoadSynchronous()) { VisualMesh->SetStaticMesh(Mesh); }
		else { UE_LOG(LogSoulsNew, Warning, TEXT("[SoulsPrototype] %s: prototype mesh unavailable."), *GetName()); }
	}
}

void ASoulsPrototypeBoss::BeginPlay()
{
	Super::BeginPlay();
	// Explicitly complete composition before configuring damage/reward flow; component BeginPlay order is irrelevant.
	// Default cube/capsule half-height is 100: pickup center is 45 above the floor and beside the body.
	if (!DropSpawner->SetSpawnOffset(FVector(160, 0, -55))
		|| !DropSpawner->RefreshRewardBinding() || !Health->SetBaseMaxHealth(500.0f))
	{
		UE_LOG(LogSoulsNew, Error, TEXT("[SoulsPrototype] %s: invalid prototype component setup."), *GetName());
		return;
	}
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Boss base health: %g; MaxHealth before item: %g"), Health->GetBaseMaxHealth(), Health->GetMaxHealth());
#endif
	if (!PrototypeItemDefinition)
	{
		UE_LOG(LogSoulsNew, Warning, TEXT("[SoulsPrototype] %s: PrototypeItemDefinition is missing. Assign an ItemDefinition in Details before Play; base health retained and no source grant configured."), *GetName());
		return;
	}
	if (Items->GrantItem(PrototypeItemDefinition, PrototypeGrantId) != ESoulsItemResult::Success)
	{
		UE_LOG(LogSoulsNew, Error, TEXT("[SoulsPrototype] %s: prototype item grant failed."), *GetName());
		return;
	}
	if (Rewards->ConfigureSourceGrant(PrototypeGrantId) != ESoulsRewardSourceResult::Success)
	{
		Items->RemoveItem(PrototypeGrantId);
		PrototypeGrantId.Invalidate();
		UE_LOG(LogSoulsNew, Error, TEXT("[SoulsPrototype] %s: source grant configuration failed."), *GetName());
		return;
	}
	// Reward processing is already subscribed. Presentation never destroys this source.
	DeathVisualHandle = Health->Death().AddUObject(this, &ASoulsPrototypeBoss::HandleDeathVisual);
	bPrototypeReady = true;
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Item %s granted to boss: MaxHealth=%g Base=%g Grant=%s Definition=%s"),
		*PrototypeItemDefinition->GetItemId().ToString(), Health->GetMaxHealth(), Health->GetBaseMaxHealth(), *PrototypeGrantId.ToString(), *PrototypeItemDefinition->GetPathName());
	RewardLogHandle = Rewards->RewardGenerated().AddUObject(this, &ASoulsPrototypeBoss::LogRewardGenerated);
	SpawnLogHandle = DropSpawner->PickupSpawned().AddUObject(this, &ASoulsPrototypeBoss::LogPickupSpawned);
	SpawnFailureLogHandle = DropSpawner->PickupSpawnFailed().AddWeakLambda(this,
		[this](USoulsRewardDropSpawnerComponent*, USoulsItemReward* Reward, ESoulsRewardDropResult Reason)
		{
			UE_LOG(LogSoulsNew, Warning, TEXT("[SoulsPrototype] Pickup spawn failed: Reason=%d Reward=%s retained for explicit retry."),
				static_cast<int32>(Reason), *Reward->GetRewardId().ToString());
		});
#endif
}

bool ASoulsPrototypeBoss::SetPrototypeItemDefinition(USoulsItemDefinition* Definition)
{
	check(IsInGameThread());
	if (HasActorBegunPlay() || IsActorBeginningPlay() || IsActorBeingDestroyed()) { return false; }
	PrototypeItemDefinition = Definition;
	return true;
}

void ASoulsPrototypeBoss::HandleDeathVisual(USoulsHealthComponent* Sender)
{
	if (Sender != Health || !Health->IsDead()) { return; }
	SetActorEnableCollision(false);
	VisualMesh->SetVisibility(false, true);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Boss died: %s; source retained, mesh hidden and collision disabled."), *GetName());
#endif
}

#if !UE_BUILD_SHIPPING
void ASoulsPrototypeBoss::LogRewardGenerated(USoulsRewardComponent* Source, USoulsItemReward* Reward)
{
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Reward generated: %s Reward=%s Definition=%s"),
		*Reward->GetItemDefinition()->GetItemId().ToString(), *Reward->GetRewardId().ToString(), *Reward->GetItemDefinition()->GetPathName());
}

void ASoulsPrototypeBoss::LogPickupSpawned(USoulsRewardDropSpawnerComponent* Spawner, ASoulsItemPickup* Pickup, USoulsItemReward* Reward)
{
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Pickup spawned: %s Location=%s Reward=%s State=%s"),
		*Pickup->GetName(), *Pickup->GetActorLocation().ToString(), *Reward->GetRewardId().ToString(),
		Reward->GetState() == ESoulsItemRewardState::Available ? TEXT("Available") : TEXT("Claimed"));
}
#endif

void ASoulsPrototypeBoss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bPrototypeReady = false;
	Health->Death().Remove(DeathVisualHandle);
#if !UE_BUILD_SHIPPING
	Rewards->RewardGenerated().Remove(RewardLogHandle);
	DropSpawner->PickupSpawned().Remove(SpawnLogHandle);
	DropSpawner->PickupSpawnFailed().Remove(SpawnFailureLogHandle);
#endif
	Super::EndPlay(EndPlayReason);
}
