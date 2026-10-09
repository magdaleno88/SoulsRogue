#include "Interaction/SoulsItemPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "SoulsNew.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"

ASoulsItemPickup::ASoulsItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	SetRootComponent(InteractionSphere);
	InteractionSphere->SetMobility(EComponentMobility::Movable);
	InteractionSphere->InitSphereRadius(75.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASoulsItemPickup::HandleBeginOverlap);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(InteractionSphere);
	VisualMesh->SetMobility(EComponentMobility::Movable);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetRelativeScale3D(FVector(0.4f));
	VisualMesh->SetCastShadow(false);
	VisualMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
}

void ASoulsItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!VisualMesh->GetStaticMesh() && !VisualMeshAsset.IsNull())
	{
		if (UStaticMesh* Mesh = VisualMeshAsset.LoadSynchronous()) { VisualMesh->SetStaticMesh(Mesh); }
		else { UE_LOG(LogSoulsNew, Warning, TEXT("%s: pickup visual mesh unavailable: %s"), *GetPathName(), *VisualMeshAsset.ToString()); }
	}
}

bool ASoulsItemPickup::InitializeReward(USoulsItemReward* InReward)
{
	check(IsInGameThread());
	if (bClaiming || bConsumed || bEnded || IsActorBeingDestroyed() || Reward) { return false; }
	if (!IsValid(InReward) || !InReward->GetRewardId().IsValid()
		|| InReward->GetState() != ESoulsItemRewardState::Available || !IsValid(InReward->GetItemDefinition())) { return false; }
	Reward = InReward;
	// Retain first: enabling collision can synchronously dispatch an overlap and complete a claim.
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	return true;
}

ESoulsPickupClaimResult ASoulsItemPickup::TryClaimForActor(AActor* Candidate, FGuid& OutGrantId)
{
	check(IsInGameThread());
	OutGrantId.Invalidate();
	if (bClaiming) { return ESoulsPickupClaimResult::Busy; }
	if (bConsumed || bEnded || IsActorBeingDestroyed()) { return ESoulsPickupClaimResult::Unavailable; }
	if (!IsValid(Reward) || !Reward->GetRewardId().IsValid() || !IsValid(Reward->GetItemDefinition())) { return ESoulsPickupClaimResult::InvalidReward; }
	if (Reward->GetState() != ESoulsItemRewardState::Available) { return ESoulsPickupClaimResult::AlreadyClaimed; }
	if (!IsValid(Candidate) || Candidate == this || Candidate->IsActorBeingDestroyed()) { return ESoulsPickupClaimResult::InvalidTarget; }
	TArray<USoulsItemComponent*> Items;
	Candidate->GetComponents<USoulsItemComponent>(Items);
	Items.RemoveAll([](USoulsItemComponent* Item) { return !IsValid(Item) || Item->IsBeingDestroyed(); });
	if (Items.IsEmpty()) { return ESoulsPickupClaimResult::MissingItemComponent; }
	if (Items.Num() != 1) { return ESoulsPickupClaimResult::AmbiguousItemComponents; }

	TStrongObjectPtr<ASoulsItemPickup> KeepPickupAlive(this);
	TStrongObjectPtr<USoulsItemReward> KeepRewardAlive(Reward.Get());
	TStrongObjectPtr<AActor> KeepTargetAlive(Candidate);
	TGuardValue<bool> ClaimGuard(bClaiming, true);
	FGuid GrantId;
	const ESoulsRewardClaimResult Result = KeepRewardAlive->Claim(Items[0], GrantId);
	if (Result != ESoulsRewardClaimResult::Success)
	{
		return Result == ESoulsRewardClaimResult::Busy ? ESoulsPickupClaimResult::Busy : ESoulsPickupClaimResult::ClaimRejected;
	}

	// Success is already confirmed by Reward, even if a callback destroyed this physical actor.
	bConsumed = true;
	DisableInteraction();
	OutGrantId = GrantId;
	PickupClaimedEvent.Broadcast(this, Candidate, GrantId);
	if (!bEnded) { OnPickupClaimed.Broadcast(this, Candidate, GrantId); }
	if (!IsActorBeingDestroyed() && !Destroy())
	{
		UE_LOG(LogSoulsNew, Warning, TEXT("%s: consumed pickup could not be destroyed; collision remains disabled."), *GetPathName());
	}
	return ESoulsPickupClaimResult::Success;
}

void ASoulsItemPickup::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	FGuid IgnoredGrantId;
	TryClaimForActor(OtherActor, IgnoredGrantId);
}

void ASoulsItemPickup::DisableInteraction()
{
	SetActorEnableCollision(false);
	if (IsValid(InteractionSphere))
	{
		InteractionSphere->SetGenerateOverlapEvents(false);
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASoulsItemPickup::Destroyed()
{
	bEnded = true;
	DisableInteraction();
	Super::Destroyed();
}

void ASoulsItemPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnded = true;
	DisableInteraction();
	Super::EndPlay(EndPlayReason);
}
