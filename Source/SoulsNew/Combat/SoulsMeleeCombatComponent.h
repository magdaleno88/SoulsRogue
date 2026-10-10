#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulsMeleeCombatComponent.generated.h"

class UAnimMontage;
class UAnimInstance;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class USoulsHealthComponent;
class UPrimitiveComponent;
class USceneComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class ESoulsMeleeState : uint8
{
	Idle,
	Windup,
	Active,
	Recovery
};

/** Montage-driven prototype attack. The notify supplies timing; this component owns damage/state. */
UCLASS(ClassGroup=(SoulsNew), meta=(BlueprintSpawnableComponent))
class SOULSNEW_API USoulsMeleeCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USoulsMeleeCombatComponent();

	/** Starts a montage in Windup; never applies damage at input time. */
	UFUNCTION(BlueprintCallable, Category="SoulsNew|Combat")
	bool TryLightAttack();

	UFUNCTION(BlueprintPure, Category="SoulsNew|Combat")
	ESoulsMeleeState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Combat")
	bool IsAttackWindowOpen() const { return CombatState == ESoulsMeleeState::Active; }

	UFUNCTION(BlueprintCallable, Category="SoulsNew|Combat")
	void CancelCurrentAttack();

	/** Configuration can only change while Idle. Null explicitly disables the attack. */
	bool SetLightAttackMontage(UAnimMontage* Montage);
	UAnimMontage* GetLightAttackMontage() const { return LightAttackMontage; }
	FGuid GetActiveAttackId() const { return ActiveAttackId; }
	int32 GetAttackMontageInstanceId() const { return AttackMontageInstanceId; }

	/** Notify requests carry the exact mesh, asset and montage execution ID; stale requests are rejected. */
	bool BeginAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId);
	bool ProcessAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId);
	/** Natural native End can recover one missed evaluation; interruption never does. */
	bool EndAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId, bool bReachedEnd = false);

	float GetLightAttackDamage() const { return LightAttackDamage; }
	float GetLightAttackRange() const { return LightAttackRange; }
	float GetTraceRadius() const { return TraceRadius; }
	float GetLightAttackCooldown() const { return LightAttackCooldown; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SoulsNew|Combat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> LightAttackMontage;

	UPROPERTY(Transient, BlueprintReadOnly, Category="SoulsNew|Combat", meta=(AllowPrivateAccess="true"))
	ESoulsMeleeState CombatState = ESoulsMeleeState::Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

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
	bool bStartingAttack = false;
	bool bProcessingWindow = false;
	bool bWindowOpened = false;
	bool bWindowEvaluated = false;
	float WindowBeginMontagePosition = 0.0f;
	bool bChangedRootMotionMode = false;
	uint8 PreviousRootMotionMode = 0;
	int32 AttackMontageInstanceId = INDEX_NONE;
	FGuid ActiveAttackId;
	TWeakObjectPtr<USkeletalMeshComponent> ActiveMesh;
	TWeakObjectPtr<UAnimInstance> ActiveAnimInstance;
	TWeakObjectPtr<USoulsHealthComponent> ObservedHealth;
	FDelegateHandle DeathHandle;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	// Eligibility samples only, never hits: no spatial history is fabricated for a skipped window.
	struct FSoulsWindowSpatialSample
	{
		FTransform Transform;
		FVector BoundsExtent;
		TWeakObjectPtr<USoulsHealthComponent> Health;
		bool bMoved = false;
		FDelegateHandle TransformHandle;
	};
	FTransform SampledOwnerTransform = FTransform::Identity;
	TWeakObjectPtr<USceneComponent> SampledOwnerRoot;
	FDelegateHandle OwnerTransformHandle;
	bool bOwnerMoved = false;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FSoulsWindowSpatialSample> InitialWindowCandidates;

	bool MatchesExecution(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId) const;
	void GatherAttackHits(TArray<FHitResult>& Hits) const;
	void CaptureWindowEligibility();
	void ClearWindowEligibility();
	bool EvaluateAttackWindow(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 InstanceId, bool bConservativeCatchUp);
	void FinishCurrentAttack(const FGuid& ExpectedAttackId);
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, FGuid ExpectedAttackId);
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, FGuid ExpectedAttackId);
	void OnOwnerDeath(USoulsHealthComponent* Health);
};
