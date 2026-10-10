#pragma once

#include "CoreMinimal.h"
#include "SoulsDamageContext.generated.h"

/** Request data, independent of the receiver. Actor references are observers, not ownership. */
USTRUCT(BlueprintType)
struct SOULSNEW_API FSoulsDamageContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SoulsNew|Damage")
	float DamageAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SoulsNew|Damage")
	TWeakObjectPtr<AActor> SourceActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SoulsNew|Damage")
	TWeakObjectPtr<AActor> InstigatorActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SoulsNew|Damage")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SoulsNew|Damage")
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SoulsNew|Damage")
	FName AttackId = NAME_None;

	/** One causal ID shared by all targets of the same attack execution. */
	UPROPERTY(BlueprintReadOnly, Category="SoulsNew|Damage")
	FGuid AttackInstanceId;
};
