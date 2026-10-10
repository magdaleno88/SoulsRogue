#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulsMeleeCombatComponent.generated.h"

/** Instant prototype sweep. No animation, stamina, equipment or owner-class assumptions. */
UCLASS(ClassGroup=(SoulsNew), meta=(BlueprintSpawnableComponent))
class SOULSNEW_API USoulsMeleeCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USoulsMeleeCombatComponent();

	/** True means an attack was admitted, including a miss. False means gating/configuration rejected it. */
	UFUNCTION(BlueprintCallable, Category="SoulsNew|Combat")
	bool TryLightAttack();

	float GetLightAttackDamage() const { return LightAttackDamage; }
	float GetLightAttackRange() const { return LightAttackRange; }
	float GetTraceRadius() const { return TraceRadius; }
	float GetLightAttackCooldown() const { return LightAttackCooldown; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SoulsNew|Combat", meta=(AllowPrivateAccess="true", ClampMin="0.001"))
	float LightAttackDamage = 100.0f;

	/** Endpoint distance from the owner's origin; the spherical volume extends by TraceRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SoulsNew|Combat", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float LightAttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SoulsNew|Combat", meta=(AllowPrivateAccess="true", ClampMin="0.001"))
	float TraceRadius = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SoulsNew|Combat", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float LightAttackCooldown = 0.55f;

	/** Gameplay never depends on debug rendering; ignored in Shipping. */
	UPROPERTY(EditAnywhere, Category="SoulsNew|Combat|Debug")
	bool bDrawDebugSweep = false;

	double NextLightAttackTime = 0.0;
	bool bExecutingAttack = false;
};
