#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Attributes/SoulsHealthComponent.h"
#include "Characters/SoulsPrototypeBoss.h"
#include "Combat/SoulsMeleeCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Items/SoulsItemDefinition.h"
#include "Misc/AutomationTest.h"
#include "SoulsNewCharacter.h"

namespace SoulsMeleeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	inline UAnimMontage* Montage() { return LoadObject<UAnimMontage>(nullptr, TEXT("/Game/SoulsNew/Animations/AM_Souls_LightAttack.AM_Souls_LightAttack")); }
	struct FFixture
	{
		UWorld* World;
		FFixture()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
		}
		~FFixture() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		AActor* Actor()
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		USphereComponent* Collision(AActor* Actor, FName Name = TEXT("Body"), FVector Offset = FVector::ZeroVector)
		{
			auto* C = NewObject<USphereComponent>(Actor, Name); Actor->AddInstanceComponent(C);
			C->SetSphereRadius(20); C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			C->SetCollisionObjectType(ECC_WorldDynamic); C->SetCollisionResponseToAllChannels(ECR_Block);
			if (Actor->GetRootComponent()) { C->SetupAttachment(Actor->GetRootComponent()); C->SetRelativeLocation(Offset); }
			else { Actor->SetRootComponent(C); }
			C->RegisterComponent(); return C;
		}
		USoulsHealthComponent* Health(AActor* Actor, float Base = 300)
		{
			auto* H = NewObject<USoulsHealthComponent>(Actor); Actor->AddInstanceComponent(H);
			H->RegisterComponent(); H->SetBaseMaxHealth(Base); return H;
		}
		AActor* Target(FVector Location = FVector(150, 0, 0))
		{
			auto* A = Actor(); Collision(A); A->SetActorLocation(Location); Health(A); return A;
		}
		USkeletalMeshComponent* Animation(AActor* Actor, FName Name = TEXT("Animation"))
		{
			auto* Mesh = NewObject<USkeletalMeshComponent>(Actor, Name); Actor->AddInstanceComponent(Mesh);
			Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")));
			Mesh->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			if (Actor->GetRootComponent()) { Mesh->SetupAttachment(Actor->GetRootComponent()); }
			else { Actor->SetRootComponent(Mesh); }
			Mesh->RegisterComponent(); return Mesh;
		}
		USoulsMeleeCombatComponent* Combat(AActor* Actor, bool bWithAnimation = true)
		{
			if (bWithAnimation && !Actor->FindComponentByClass<USkeletalMeshComponent>()) { Animation(Actor); }
			auto* C = NewObject<USoulsMeleeCombatComponent>(Actor); Actor->AddInstanceComponent(C); C->RegisterComponent();
			C->SetLightAttackMontage(Montage()); return C;
		}
		ASoulsNewCharacter* Player()
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Class = LoadClass<ASoulsNewCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			return World->SpawnActor<ASoulsNewCharacter>(Class, FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		ASoulsPrototypeBoss* Boss(FVector Location = FVector(175, 0, 0))
		{
			FActorSpawnParameters P; P.bDeferConstruction = true; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* B = World->SpawnActor<ASoulsPrototypeBoss>(Location, FRotator::ZeroRotator, P);
			B->SetPrototypeItemDefinition(LoadObject<USoulsItemDefinition>(nullptr, TEXT("/Game/SoulsNew/Items/DA_Item_Vitality.DA_Item_Vitality")));
			B->FinishSpawning(FTransform(Location)); return B;
		}
		USkeletalMeshComponent* Mesh(USoulsMeleeCombatComponent* C) { return C->GetOwner()->FindComponentByClass<USkeletalMeshComponent>(); }
		bool Open(USoulsMeleeCombatComponent* C) { return C->BeginAttackWindow(Mesh(C), C->GetLightAttackMontage(), C->GetAttackMontageInstanceId()); }
		bool Process(USoulsMeleeCombatComponent* C) { return C->ProcessAttackWindow(Mesh(C), C->GetLightAttackMontage(), C->GetAttackMontageInstanceId()); }
		bool Close(USoulsMeleeCombatComponent* C) { return C->EndAttackWindow(Mesh(C), C->GetLightAttackMontage(), C->GetAttackMontageInstanceId()); }
		// Older collision/gating tests simulate notify requests. Production has no immediate-damage fallback.
		bool Attack(USoulsMeleeCombatComponent* C)
		{
			if (!C->TryLightAttack()) { return false; }
			const bool bProcessed = Open(C) && Process(C);
			Close(C); C->CancelCurrentAttack(); return bProcessed;
		}
		void Interrupt(USoulsMeleeCombatComponent* C)
		{
			auto* Anim = Mesh(C)->GetAnimInstance();
			Anim->Montage_Stop(0.0f, C->GetLightAttackMontage()); Anim->DispatchQueuedAnimEvents();
		}
		void Advance(float Seconds = 0.56f)
		{
			// UWorld clamps a large single delta. Substeps advance the requested world time faithfully.
			while (Seconds > UE_SMALL_NUMBER)
			{
				const float Step = FMath::Min(Seconds, 0.1f); World->Tick(LEVELTICK_All, Step); Seconds -= Step;
			}
		}
	};
}
