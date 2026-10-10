#include "Combat/SoulsMeleeCombatComponent.h"

#include "Attributes/SoulsHealthComponent.h"
#include "Components/PrimitiveComponent.h"
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
	if (bExecutingAttack || !IsValid(Owner) || !World || !Owner->HasActorBegunPlay()
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
	// Reserve gating before queries or synchronous callbacks can request another attack.
	TGuardValue<bool> ExecutionGuard(bExecutingAttack, true);
	NextLightAttackTime = static_cast<double>(World->GetTimeSeconds()) + LightAttackCooldown;
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
	Context.AttackInstanceId = FGuid::NewGuid();
	TSet<TWeakObjectPtr<AActor>> Visited;
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSoulsNew, Verbose, TEXT("[SoulsCombat] LightAttack started: %s"), *Owner->GetName());
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
		if (!IsValid(Owner) || !Owner->HasActorBegunPlay() || (OwnerHealth && (!IsValid(OwnerHealth) || OwnerHealth->IsDead()))) { break; }
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Owner || Visited.Contains(Target)) { continue; }
		Visited.Add(Target);
		USoulsHealthComponent* Health = UniqueHealth(Target, bAmbiguous);
		if (!Health || bAmbiguous) { continue; }
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
