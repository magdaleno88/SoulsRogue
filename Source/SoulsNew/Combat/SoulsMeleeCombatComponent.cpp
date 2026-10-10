#include "Combat/SoulsMeleeCombatComponent.h"

#include "Attributes/SoulsHealthComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "SoulsNew.h"
#include "Templates/UnrealTemplate.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

namespace
{
	// Ambiguity is never resolved by component registration order.
	USoulsHealthComponent* UniqueHealth(AActor* Actor, bool& bAmbiguous)
	{
		TArray<USoulsHealthComponent*> Components;
		Actor->GetComponents(Components);
		Components.RemoveAll([](USoulsHealthComponent* Component) { return !IsValid(Component); });
		bAmbiguous = Components.Num() > 1;
		return Components.Num() == 1 ? Components[0] : nullptr;
	}
}

USoulsMeleeCombatComponent::USoulsMeleeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USoulsMeleeCombatComponent::TryLightAttack()
{
	check(IsInGameThread());
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (bStartingAttack || bProcessingWindow || CombatState != ESoulsMeleeState::Idle
		|| !IsValid(Owner) || !World || !Owner->HasActorBegunPlay() || !IsValid(LightAttackMontage)
		|| !FMath::IsFinite(LightAttackDamage) || LightAttackDamage <= 0.0f
		|| !FMath::IsFinite(TraceRadius) || TraceRadius <= 0.0f
		|| !FMath::IsFinite(LightAttackRange) || LightAttackRange <= TraceRadius + 1.0f
		|| !FMath::IsFinite(LightAttackCooldown) || LightAttackCooldown < 0.0f)
	{
		return false;
	}
	bool bAmbiguous = false;
	const USoulsHealthComponent* OwnerHealth = UniqueHealth(Owner, bAmbiguous);
	if (bAmbiguous || (OwnerHealth && OwnerHealth->IsDead()) || World->GetTimeSeconds() < NextLightAttackTime)
	{
		return false;
	}
	// Require one initialized animation source; additional decorative skeletal meshes are ambiguous.
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);
	Meshes.RemoveAll([](USkeletalMeshComponent* Mesh) { return !IsValid(Mesh); });
	if (Meshes.Num() != 1) { return false; }
	USkeletalMeshComponent* Mesh = Meshes[0];
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	if (!IsValid(Anim) || !Mesh->GetSkeletalMeshAsset()
		|| !LightAttackMontage->GetSkeleton() || LightAttackMontage->GetSkeleton() != Mesh->GetSkeletalMeshAsset()->GetSkeleton()) { return false; }
	TGuardValue<bool> StartGuard(bStartingAttack, true);
	CombatState = ESoulsMeleeState::Windup;
	ActiveAttackId = FGuid::NewGuid();
	const FGuid ThisAttack = ActiveAttackId;
	ActiveMontage = LightAttackMontage;
	ActiveMesh = Mesh;
	ActiveAnimInstance = Anim;
	HitActors.Reset();
	bWindowOpened = false;
	// The source contains root motion. Extract and discard it during this attack, without changing the asset.
	PreviousRootMotionMode = static_cast<uint8>(Anim->RootMotionMode.GetValue());
	bChangedRootMotionMode = true;
	Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	const float Length = Anim->Montage_Play(ActiveMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
	FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(LightAttackMontage);
	if (Length <= 0.0f || !Instance || ActiveAttackId != ThisAttack || !IsValid(Owner)
		|| (OwnerHealth && OwnerHealth->IsDead()))
	{
		FinishCurrentAttack(ThisAttack);
		if (Length > 0.0f) { Anim->Montage_Stop(0.0f, LightAttackMontage); }
		return false;
	}
	AttackMontageInstanceId = Instance->GetInstanceID();
	Instance->OnMontageBlendingOutStarted.BindUObject(this, &USoulsMeleeCombatComponent::OnMontageBlendingOut, ThisAttack);
	Instance->OnMontageEnded.BindUObject(this, &USoulsMeleeCombatComponent::OnMontageEnded, ThisAttack);
	NextLightAttackTime = static_cast<double>(World->GetTimeSeconds()) + LightAttackCooldown;
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSoulsNew, Verbose, TEXT("[SoulsCombat] LightAttack Windup: %s Instance=%d"), *Owner->GetName(), AttackMontageInstanceId);
#endif
	return true;
}

bool USoulsMeleeCombatComponent::SetLightAttackMontage(UAnimMontage* Montage)
{
	check(IsInGameThread());
	if (CombatState != ESoulsMeleeState::Idle || bStartingAttack || bProcessingWindow) { return false; }
	LightAttackMontage = Montage;
	return true;
}

void USoulsMeleeCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	bool bAmbiguous = false;
	ObservedHealth = UniqueHealth(GetOwner(), bAmbiguous);
	if (ObservedHealth.IsValid()) { DeathHandle = ObservedHealth->Death().AddUObject(this, &USoulsMeleeCombatComponent::OnOwnerDeath); }
}

void USoulsMeleeCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCurrentAttack();
	if (ObservedHealth.IsValid()) { ObservedHealth->Death().Remove(DeathHandle); }
	ObservedHealth.Reset();
	Super::EndPlay(EndPlayReason);
}

void USoulsMeleeCombatComponent::OnUnregister()
{
	CancelCurrentAttack();
	Super::OnUnregister();
}

void USoulsMeleeCombatComponent::OnOwnerDeath(USoulsHealthComponent*)
{
	CancelCurrentAttack();
}

bool USoulsMeleeCombatComponent::MatchesExecution(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId) const
{
	return CombatState != ESoulsMeleeState::Idle && ActiveAttackId.IsValid()
		&& IsValid(Mesh) && ActiveMesh == Mesh && ActiveMontage == Animation
		&& InstanceId != INDEX_NONE && InstanceId == AttackMontageInstanceId
		&& ActiveAnimInstance.IsValid() && Mesh->GetAnimInstance() == ActiveAnimInstance.Get();
}

bool USoulsMeleeCombatComponent::BeginAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId)
{
	check(IsInGameThread());
	if (!MatchesExecution(Mesh, Animation, InstanceId) || CombatState != ESoulsMeleeState::Windup || bWindowOpened) { return false; }
	bWindowOpened = true;
	HitActors.Reset();
	CombatState = ESoulsMeleeState::Active;
	return true;
}

bool USoulsMeleeCombatComponent::EndAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId)
{
	check(IsInGameThread());
	if (!MatchesExecution(Mesh, Animation, InstanceId) || CombatState != ESoulsMeleeState::Active) { return false; }
	CombatState = ESoulsMeleeState::Recovery;
	return true;
}

void USoulsMeleeCombatComponent::FinishCurrentAttack(const FGuid& ExpectedAttackId)
{
	if (!ExpectedAttackId.IsValid() || ExpectedAttackId != ActiveAttackId) { return; }
	if (UAnimInstance* Anim = ActiveAnimInstance.Get())
	{
		if (FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(AttackMontageInstanceId))
		{
			if (Instance->OnMontageBlendingOutStarted.IsBoundToObject(this)) { Instance->OnMontageBlendingOutStarted.Unbind(); }
			if (Instance->OnMontageEnded.IsBoundToObject(this)) { Instance->OnMontageEnded.Unbind(); }
		}
		if (bChangedRootMotionMode && Anim->RootMotionMode == ERootMotionMode::IgnoreRootMotion)
		{
			Anim->SetRootMotionMode(static_cast<ERootMotionMode::Type>(PreviousRootMotionMode));
		}
	}
	CombatState = ESoulsMeleeState::Idle;
	bWindowOpened = false;
	bChangedRootMotionMode = false;
	ActiveAttackId.Invalidate();
	AttackMontageInstanceId = INDEX_NONE;
	ActiveMontage = nullptr;
	ActiveMesh.Reset();
	ActiveAnimInstance.Reset();
	HitActors.Reset();
}

void USoulsMeleeCombatComponent::CancelCurrentAttack()
{
	check(IsInGameThread());
	UAnimInstance* Anim = ActiveAnimInstance.Get();
	const int32 InstanceId = AttackMontageInstanceId;
	FinishCurrentAttack(ActiveAttackId);
	// Stop only the instance this component owned, after invalidating callbacks/notify provenance.
	if (Anim && InstanceId != INDEX_NONE)
	{
		if (FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(InstanceId))
		{
			Instance->Stop(FAlphaBlend(0.0f), true);
		}
	}
}

void USoulsMeleeCombatComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, FGuid ExpectedAttackId)
{
	if (ActiveAttackId != ExpectedAttackId || ActiveMontage != Montage) { return; }
	if (bInterrupted) { FinishCurrentAttack(ExpectedAttackId); }
	else { CombatState = ESoulsMeleeState::Recovery; } // Close damage defensively; wait for End before Idle.
}

void USoulsMeleeCombatComponent::OnMontageEnded(UAnimMontage* Montage, bool, FGuid ExpectedAttackId)
{
	if (ActiveMontage == Montage) { FinishCurrentAttack(ExpectedAttackId); }
}

bool USoulsMeleeCombatComponent::ProcessAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId)
{
	check(IsInGameThread());
	if (bProcessingWindow || !MatchesExecution(Mesh, Animation, InstanceId) || CombatState != ESoulsMeleeState::Active) { return false; }
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	bool bAmbiguous = false;
	USoulsHealthComponent* OwnerHealth = IsValid(Owner) ? UniqueHealth(Owner, bAmbiguous) : nullptr;
	if (!IsValid(Owner) || !World || !Owner->HasActorBegunPlay() || bAmbiguous || (OwnerHealth && OwnerHealth->IsDead()))
	{
		CancelCurrentAttack(); return false;
	}
	TGuardValue<bool> ProcessingGuard(bProcessingWindow, true);
	const FGuid ThisAttack = ActiveAttackId;
	const FVector Forward = Owner->GetActorForwardVector();
	const FVector Start = Owner->GetActorLocation() + Forward * (TraceRadius + 1.0f);
	const FVector End = Owner->GetActorLocation() + Forward * LightAttackRange;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SoulsLightAttack), false, Owner);
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(TraceRadius), Query);
	// Stable target order and component choice, independent of physics broadphase order.
	Hits.Sort([](const FHitResult& Left, const FHitResult& Right)
	{
		const FString LeftActor = GetPathNameSafe(Left.GetActor());
		const FString RightActor = GetPathNameSafe(Right.GetActor());
		if (LeftActor != RightActor) { return LeftActor.Compare(RightActor, ESearchCase::CaseSensitive) < 0; }
		return GetPathNameSafe(Left.GetComponent()).Compare(GetPathNameSafe(Right.GetComponent()), ESearchCase::CaseSensitive) < 0;
	});
	FSoulsDamageContext Context;
	Context.DamageAmount = LightAttackDamage;
	Context.SourceActor = Owner;
	Context.InstigatorActor = Owner->GetInstigator() ? static_cast<AActor*>(Owner->GetInstigator()) : Owner;
	Context.AttackId = TEXT("Attack.Light");
	Context.AttackInstanceId = ThisAttack;
#if !UE_BUILD_SHIPPING
	if (bDrawDebugSweep)
	{
		DrawDebugLine(World, Start, End, FColor::Cyan, false, 0.55f);
		DrawDebugSphere(World, Start, TraceRadius, 12, FColor::Cyan, false, 0.55f);
		DrawDebugSphere(World, End, TraceRadius, 12, FColor::Cyan, false, 0.55f);
	}
#endif
	for (const FHitResult& Hit : Hits)
	{
		// A preceding target's observers may end play, kill or destroy either actor.
		if (ActiveAttackId != ThisAttack || CombatState != ESoulsMeleeState::Active || !IsValid(Owner)
			|| !Owner->HasActorBegunPlay() || (OwnerHealth && (!IsValid(OwnerHealth) || OwnerHealth->IsDead()))) { break; }
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Owner || HitActors.Contains(Target)) { continue; }
		USoulsHealthComponent* Health = UniqueHealth(Target, bAmbiguous);
		if (!Health || bAmbiguous) { continue; }
		HitActors.Add(Target); // Reserve before synchronous observers can trigger another sweep.
		Context.HitLocation = Hit.ImpactPoint;
		Context.HitNormal = Hit.ImpactNormal;
		const float Applied = Health->ApplyDamage(Context);
#if !UE_BUILD_SHIPPING
		if (Applied > 0.0f && IsValid(Health))
		{
			UE_LOG(LogSoulsNew, Display, TEXT("[SoulsCombat] Hit %s Damage=%g Health=%g/%g Attack=%s"),
				*GetNameSafe(Target), Applied, Health->GetCurrentHealth(), Health->GetMaxHealth(), *Context.AttackId.ToString());
		}
#endif
	}
	return true;
}
