#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Attributes/SoulsAttributeModifier.h"
#include "Combat/SoulsDamageContext.h"
#include "SoulsHealthComponent.generated.h"

class USoulsHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoulsHealthChanged, USoulsHealthComponent*, HealthComponent, float, OldHealth, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoulsMaxHealthChanged, USoulsHealthComponent*, HealthComponent, float, OldMaxHealth, float, NewMaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSoulsDeath, USoulsHealthComponent*, HealthComponent);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsHealthChangedNative, USoulsHealthComponent*, float, float);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSoulsMaxHealthChangedNative, USoulsHealthComponent*, float, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FSoulsDeathNative, USoulsHealthComponent*);
DECLARE_MULTICAST_DELEGATE_FourParams(FSoulsDamageReceivedNative, USoulsHealthComponent*, const FSoulsDamageContext&, float, float);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSoulsDamageReceived, USoulsHealthComponent*, HealthComponent, const FSoulsDamageContext&, Context, float, DamageApplied, float, ResultingHealth);

/**
 * Owner-agnostic health. No UI, items, Character casts, Tick or Unreal Damage System dependency.
 * MaxHealth = max(1, (Base + sum(Flat)) * (1 + sum(AdditivePercent)) * product(Multiplicative)).
 * Changes to MaxHealth preserve CurrentHealth / MaxHealth; dead entities remain at zero.
 * Mutation is game-thread only. Reentrant mutations during notifications are rejected.
 */
UCLASS(ClassGroup=(SoulsNew), meta=(BlueprintSpawnableComponent))
class SOULSNEW_API USoulsHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USoulsHealthComponent();

	UFUNCTION(BlueprintPure, Category="SoulsNew|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Health")
	float GetHealthPercentage() const { return CurrentHealth / MaxHealth; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Health")
	bool IsDead() const { return bIsDead; }

	/** Returns actual health lost. Negative/non-finite requests, dead state and reentry are no-ops. */
	UFUNCTION(BlueprintCallable, Category="SoulsNew|Health")
	float TakeDamage(float Amount);

	/** Same health authority as TakeDamage. Returns actual loss, with confirmed context notification. */
	UFUNCTION(BlueprintCallable, Category="SoulsNew|Health")
	float ApplyDamage(const FSoulsDamageContext& Context);

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Health")
	FSoulsDamageReceived OnDamageReceived;

	FSoulsDamageReceivedNative& DamageReceived() { return DamageReceivedEvent; }

	/** Returns actual health restored; does not resurrect a dead entity. */
	UFUNCTION(BlueprintCallable, Category="SoulsNew|Health")
	float Heal(float Amount);

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Health")
	FSoulsHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Health")
	FSoulsMaxHealthChanged OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="SoulsNew|Health")
	FSoulsDeath OnDeath;

	/** Native subscriptions for C++ consumers; identical payloads to the Blueprint notifications. */
	FSoulsHealthChangedNative& HealthChanged() { return HealthChangedEvent; }
	FSoulsMaxHealthChangedNative& MaxHealthChanged() { return MaxHealthChangedEvent; }
	FSoulsDeathNative& Death() { return DeathEvent; }

	float GetBaseMaxHealth() const { return BaseMaxHealth; }

	/** C++ authority: rejects non-finite values, values below 1, or an overflowing aggregate. */
	bool SetBaseMaxHealth(float NewBaseMaxHealth);

	/** IDs must be valid and unique per component. Failure leaves all state unchanged. */
	bool AddMaxHealthModifier(const FSoulsAttributeModifier& Modifier);
	bool FindMaxHealthModifier(const FGuid& Id, FSoulsAttributeModifier& OutModifier) const;
	bool RemoveMaxHealthModifier(const FGuid& Id);

	/** Validate a candidate registry, then commit once. Failure changes nothing and emits no events. */
	bool ApplyMaxHealthModifierChanges(TConstArrayView<FSoulsAttributeModifier> AddedModifiers,
		TConstArrayView<FGuid> RemovedModifierIds);

	/** Explicit new life: retains modifiers, fills health and rearms Death. C++ only. */
	bool ResetHealthForNewLife();

protected:
	virtual void InitializeComponent() override;

private:
	static constexpr float DefaultBaseMaxHealth = 100.0f;
	static constexpr float MinimumMaxHealth = 1.0f;

	/** Designer configuration only; runtime writes must go through SetBaseMaxHealth. */
	UPROPERTY(EditAnywhere, Category="SoulsNew|Health", meta=(ClampMin="1.0", UIMin="1.0"))
	float BaseMaxHealth = DefaultBaseMaxHealth;

	UPROPERTY(Transient)
	float MaxHealth = DefaultBaseMaxHealth;

	UPROPERTY(Transient)
	float CurrentHealth = DefaultBaseMaxHealth;

	UPROPERTY(Transient)
	bool bIsDead = false;

	TArray<FSoulsAttributeModifier> MaxHealthModifiers;
	bool bBroadcastingChanges = false;
	FSoulsHealthChangedNative HealthChangedEvent;
	FSoulsMaxHealthChangedNative MaxHealthChangedEvent;
	FSoulsDeathNative DeathEvent;
	FSoulsDamageReceivedNative DamageReceivedEvent;

	static bool IsValidModifier(const FSoulsAttributeModifier& Modifier);
	static bool TryCalculateMaxHealth(float Base, TConstArrayView<FSoulsAttributeModifier> Modifiers, float& OutMaxHealth);
	void ApplyMaxHealthPreservingPercentage(float NewMaxHealth);
	void CommitHealthValues(float NewMaxHealth, float NewCurrentHealth, bool bNewLife = false,
		const FSoulsDamageContext* DamageContext = nullptr);
};
