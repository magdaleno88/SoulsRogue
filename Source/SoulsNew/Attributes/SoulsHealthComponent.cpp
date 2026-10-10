#include "Attributes/SoulsHealthComponent.h"

#include "Templates/UnrealTemplate.h"
#include "SoulsNew.h"

namespace
{
	// Never aggregate in hash/insertion order: identical ID sets have identical arithmetic order.
	bool ModifierIdLess(const FSoulsAttributeModifier& Left, const FSoulsAttributeModifier& Right)
	{
		if (Left.Id.A != Right.Id.A) { return Left.Id.A < Right.Id.A; }
		if (Left.Id.B != Right.Id.B) { return Left.Id.B < Right.Id.B; }
		if (Left.Id.C != Right.Id.C) { return Left.Id.C < Right.Id.C; }
		return Left.Id.D < Right.Id.D;
	}
}

USoulsHealthComponent::USoulsHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void USoulsHealthComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Reconcile serialized/editor base configuration with runtime values, without resurrecting.
	if (!SetBaseMaxHealth(BaseMaxHealth))
	{
		UE_LOG(LogSoulsNew, Warning, TEXT("%s: invalid initial health configuration; falling back to base health 100."), *GetPathName());
		SetBaseMaxHealth(DefaultBaseMaxHealth);
	}
}

bool USoulsHealthComponent::IsValidModifier(const FSoulsAttributeModifier& Modifier)
{
	if (!Modifier.Id.IsValid() || !FMath::IsFinite(Modifier.Magnitude))
	{
		return false;
	}

	switch (Modifier.Operation)
	{
	case ESoulsAttributeModifierOperation::Flat:
	case ESoulsAttributeModifierOperation::AdditivePercent:
		return true;
	case ESoulsAttributeModifierOperation::Multiplicative:
		return Modifier.Magnitude > 0.0f;
	default:
		return false;
	}
}

bool USoulsHealthComponent::TryCalculateMaxHealth(float Base, TConstArrayView<FSoulsAttributeModifier> Modifiers, float& OutMaxHealth)
{
	if (!FMath::IsFinite(Base) || Base < MinimumMaxHealth)
	{
		return false;
	}

	double Flat = 0.0;
	double AdditivePercent = 0.0;
	double Multiplicative = 1.0;
	for (const FSoulsAttributeModifier& Modifier : Modifiers)
	{
		switch (Modifier.Operation)
		{
		case ESoulsAttributeModifierOperation::Flat:
			Flat += Modifier.Magnitude;
			break;
		case ESoulsAttributeModifierOperation::AdditivePercent:
			AdditivePercent += Modifier.Magnitude;
			break;
		case ESoulsAttributeModifierOperation::Multiplicative:
			Multiplicative *= Modifier.Magnitude;
			break;
		default:
			return false;
		}

		if (!FMath::IsFinite(Flat) || !FMath::IsFinite(AdditivePercent) || !FMath::IsFinite(Multiplicative))
		{
			return false;
		}
	}

	const double Result = (static_cast<double>(Base) + Flat) * (1.0 + AdditivePercent) * Multiplicative;
	if (!FMath::IsFinite(Result) || Result > static_cast<double>(TNumericLimits<float>::Max()))
	{
		return false;
	}

	OutMaxHealth = static_cast<float>(FMath::Max(static_cast<double>(MinimumMaxHealth), Result));
	return true;
}

bool USoulsHealthComponent::SetBaseMaxHealth(float NewBaseMaxHealth)
{
	check(IsInGameThread());
	float NewMaxHealth;
	if (bBroadcastingChanges || !TryCalculateMaxHealth(NewBaseMaxHealth, MaxHealthModifiers, NewMaxHealth))
	{
		return false;
	}

	BaseMaxHealth = NewBaseMaxHealth;
	ApplyMaxHealthPreservingPercentage(NewMaxHealth);
	return true;
}

bool USoulsHealthComponent::AddMaxHealthModifier(const FSoulsAttributeModifier& Modifier)
{
	return ApplyMaxHealthModifierChanges(MakeArrayView(&Modifier, 1), {});
}

bool USoulsHealthComponent::FindMaxHealthModifier(const FGuid& Id, FSoulsAttributeModifier& OutModifier) const
{
	check(IsInGameThread());
	const FSoulsAttributeModifier* Found = MaxHealthModifiers.FindByPredicate([&Id](const FSoulsAttributeModifier& Modifier) { return Modifier.Id == Id; });
	if (!Found)
	{
		return false;
	}

	OutModifier = *Found; // A copy; callers cannot retain a pointer into mutable storage.
	return true;
}

bool USoulsHealthComponent::RemoveMaxHealthModifier(const FGuid& Id)
{
	return ApplyMaxHealthModifierChanges({}, MakeArrayView(&Id, 1));
}

bool USoulsHealthComponent::ApplyMaxHealthModifierChanges(TConstArrayView<FSoulsAttributeModifier> AddedModifiers,
	TConstArrayView<FGuid> RemovedModifierIds)
{
	check(IsInGameThread());
	if (bBroadcastingChanges)
	{
		return false;
	}

	TArray<FSoulsAttributeModifier> ProposedModifiers = MaxHealthModifiers;
	for (const FGuid& Id : RemovedModifierIds)
	{
		const int32 Index = ProposedModifiers.IndexOfByPredicate([&Id](const FSoulsAttributeModifier& Modifier) { return Modifier.Id == Id; });
		if (Index == INDEX_NONE) { return false; }
		ProposedModifiers.RemoveAt(Index);
	}
	for (const FSoulsAttributeModifier& Modifier : AddedModifiers)
	{
		if (!IsValidModifier(Modifier)
			|| ProposedModifiers.ContainsByPredicate([&Modifier](const FSoulsAttributeModifier& Existing) { return Existing.Id == Modifier.Id; }))
		{
			return false;
		}
		ProposedModifiers.Add(Modifier);
	}
	ProposedModifiers.Sort(ModifierIdLess);
	float NewMaxHealth;
	if (!TryCalculateMaxHealth(BaseMaxHealth, ProposedModifiers, NewMaxHealth))
	{
		return false;
	}

	MaxHealthModifiers = MoveTemp(ProposedModifiers);
	ApplyMaxHealthPreservingPercentage(NewMaxHealth);
	return true;
}

void USoulsHealthComponent::ApplyMaxHealthPreservingPercentage(float NewMaxHealth)
{
	const double Percentage = static_cast<double>(CurrentHealth) / MaxHealth;
	const float NewCurrentHealth = bIsDead ? 0.0f : static_cast<float>(Percentage * NewMaxHealth);
	CommitHealthValues(NewMaxHealth, NewCurrentHealth);
}

float USoulsHealthComponent::TakeDamage(float Amount)
{
	FSoulsDamageContext Context;
	Context.DamageAmount = Amount;
	return ApplyDamage(Context);
}

float USoulsHealthComponent::ApplyDamage(const FSoulsDamageContext& Context)
{
	check(IsInGameThread());
	const float Amount = Context.DamageAmount;
	if (bBroadcastingChanges || bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float NewCurrentHealth = static_cast<float>(FMath::Max(0.0, static_cast<double>(CurrentHealth) - Amount));
	const float ActualDamage = CurrentHealth - NewCurrentHealth;
	// Snapshot before any observer runs, including HealthChanged observers.
	const FSoulsDamageContext ConfirmedContext = Context;
	CommitHealthValues(MaxHealth, NewCurrentHealth, false, &ConfirmedContext);
	return ActualDamage;
}

float USoulsHealthComponent::Heal(float Amount)
{
	check(IsInGameThread());
	if (bBroadcastingChanges || bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float NewCurrentHealth = static_cast<float>(FMath::Min(static_cast<double>(MaxHealth), static_cast<double>(CurrentHealth) + Amount));
	const float ActualHealing = NewCurrentHealth - CurrentHealth;
	CommitHealthValues(MaxHealth, NewCurrentHealth);
	return ActualHealing;
}

bool USoulsHealthComponent::ResetHealthForNewLife()
{
	check(IsInGameThread());
	if (bBroadcastingChanges)
	{
		return false;
	}

	CommitHealthValues(MaxHealth, MaxHealth, true);
	return true;
}

void USoulsHealthComponent::CommitHealthValues(float NewMaxHealth, float NewCurrentHealth, bool bNewLife,
	const FSoulsDamageContext* DamageContext)
{
	const float OldMaxHealth = MaxHealth;
	const float OldCurrentHealth = CurrentHealth;
	MaxHealth = NewMaxHealth;
	CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.0f, MaxHealth);
	if (bNewLife)
	{
		bIsDead = false;
	}
	const bool bDiedNow = !bIsDead && CurrentHealth == 0.0f;
	bIsDead |= bDiedNow;

	// Commit the entire transition before observers run. No nested writes may invalidate payloads.
	TGuardValue<bool> BroadcastGuard(bBroadcastingChanges, true);
	if (OldMaxHealth != MaxHealth)
	{
		MaxHealthChangedEvent.Broadcast(this, OldMaxHealth, MaxHealth);
		OnMaxHealthChanged.Broadcast(this, OldMaxHealth, MaxHealth);
	}
	if (OldCurrentHealth != CurrentHealth)
	{
		HealthChangedEvent.Broadcast(this, OldCurrentHealth, CurrentHealth);
		OnHealthChanged.Broadcast(this, OldCurrentHealth, CurrentHealth);
	}
	if (DamageContext && OldCurrentHealth > CurrentHealth)
	{
		DamageReceivedEvent.Broadcast(this, *DamageContext, OldCurrentHealth - CurrentHealth, CurrentHealth);
		OnDamageReceived.Broadcast(this, *DamageContext, OldCurrentHealth - CurrentHealth, CurrentHealth);
	}
	if (bDiedNow)
	{
		DeathEvent.Broadcast(this);
		OnDeath.Broadcast(this);
	}
}
