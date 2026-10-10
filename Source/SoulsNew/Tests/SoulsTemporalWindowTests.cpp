#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SoulsMeleeTestFixture.h"

namespace SoulsTemporalTests
{
	using namespace SoulsMeleeTests;
	// Advance the real animation directly: no UWorld delta clamp and no simulated notify calls.
	void Step(FFixture& F, USoulsMeleeCombatComponent* C, float Delta)
	{
		F.Mesh(C)->TickAnimation(Delta, false);
		F.Mesh(C)->GetAnimInstance()->DispatchQueuedAnimEvents();
	}
	void Finish(FFixture& F, USoulsMeleeCombatComponent* C)
	{
		for (int32 I = 0; I < 30 && C->GetCombatState() != ESoulsMeleeState::Idle; ++I) { Step(F, C, 0.1f); }
	}
}
using namespace SoulsTemporalTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalNormal, "SoulsNew.TemporalWindow.A.NormalWindowStillWorks", Flags)
bool FSoulsTemporalNormal::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); TestTrue(TEXT("Start"), C->TryLightAttack());
	Step(F, C, 0.15f); TestEqual(TEXT("Windup harmless"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	Step(F, C, 0.15f); TestTrue(TEXT("Active"), C->IsAttackWindowOpen());
	TestEqual(TEXT("Normal native Tick loses100"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	Finish(F, C); TestTrue(TEXT("Natural Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalSkip, "SoulsNew.TemporalWindow.B.FullWindowSkipped", Flags)
bool FSoulsTemporalSkip::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); int32 Hits = 0;
	B->GetSoulsHealth()->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext& Context, float Applied, float)
	{
		++Hits; TestTrue(TEXT("Damage only inside logical Active boundary"), C->IsAttackWindowOpen());
		TestEqual(TEXT("Same execution context"), Context.AttackInstanceId, C->GetActiveAttackId()); TestEqual(TEXT("Actual100"), Applied, 100.0f);
	});
	C->TryLightAttack(); Step(F, C, 0.6f);
	auto* Instance = F.Mesh(C)->GetAnimInstance()->GetMontageInstanceForID(C->GetAttackMontageInstanceId());
	if (TestNotNull(TEXT("Still in montage recovery"), Instance))
	{
		TestTrue(TEXT("One real step began before window"), Instance->GetPreviousPosition() < 0.24f);
		TestTrue(TEXT("One real step ended after window"), Instance->GetPosition() > 0.44f);
	}
	TestEqual(TEXT("Skipped stationary boss hit once"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	TestEqual(TEXT("One event"), Hits, 1); TestTrue(TEXT("Recovery"), C->GetCombatState() == ESoulsMeleeState::Recovery); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalDedup, "SoulsNew.TemporalWindow.C.SkipDoesNotDoubleHit", Flags)
bool FSoulsTemporalDedup::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); Step(F, C, 0.3f);
	TestEqual(TEXT("First native Tick"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	F.Process(C); Step(F, C, 0.3f); F.Close(C); Finish(F, C);
	TestEqual(TEXT("Ticks and natural closure share single-hit contract"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalMiss, "SoulsNew.TemporalWindow.D.SkipWithoutTarget", Flags)
bool FSoulsTemporalMiss::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(FVector(1000, 0, 0)); C->TryLightAttack(); Step(F, C, 0.6f); Finish(F, C);
	TestEqual(TEXT("Miss cannot fabricate hit"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	TestTrue(TEXT("Miss still finishes Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalEarlyInterrupt, "SoulsNew.TemporalWindow.E.InterruptedBeforeWindowNoCatchUp", Flags)
bool FSoulsTemporalEarlyInterrupt::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); Step(F, C, 0.1f); F.Interrupt(C); Step(F, C, 0.8f);
	TestEqual(TEXT("No deferred damage after interruption"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalFast, "SoulsNew.TemporalWindow.F.HighPlayRate", Flags)
bool FSoulsTemporalFast::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack();
	F.Mesh(C)->GetAnimInstance()->Montage_SetPlayRate(Montage(), 3.0f); Step(F, C, 0.2f);
	TestEqual(TEXT("Rate3 crosses same montage-time window once"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	TestTrue(TEXT("Recovery despite faster play"), C->GetCombatState() == ESoulsMeleeState::Recovery);
	Finish(F, C); TestTrue(TEXT("Natural Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalCleanup, "SoulsNew.TemporalWindow.G.Cleanup", Flags)
bool FSoulsTemporalCleanup::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); Step(F, C, 0.6f); Finish(F, C);
	TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); TestFalse(TEXT("Closed"), C->IsAttackWindowOpen());
	TestFalse(TEXT("No stale GUID"), C->GetActiveAttackId().IsValid()); TestFalse(TEXT("No stale processing"), F.Process(C));
	F.Advance(); TestTrue(TEXT("Next start"), C->TryLightAttack()); Step(F, C, 0.6f);
	TestEqual(TEXT("Snapshot and HitActors refreshed"), B->GetSoulsHealth()->GetCurrentHealth(), 425.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalNext, "SoulsNew.TemporalWindow.H.NextAttackWorks", Flags)
bool FSoulsTemporalNext::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); const FGuid First = C->GetActiveAttackId();
	Step(F, C, 0.6f); Finish(F, C); F.Advance(); C->TryLightAttack();
	TestTrue(TEXT("New causal ID"), C->GetActiveAttackId().IsValid() && First != C->GetActiveAttackId());
	Step(F, C, 0.3f); TestEqual(TEXT("Normal attack after catch-up hits again"), B->GetSoulsHealth()->GetCurrentHealth(), 425.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalSingle, "SoulsNew.TemporalWindow.I.ExistingSingleHitContract", Flags)
bool FSoulsTemporalSingle::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); Step(F, C, 0.25f);
	for (int32 I = 0; I < 6; ++I) { F.Process(C); Step(F, C, 0.02f); }
	Finish(F, C); TestEqual(TEXT("Multiple normal evaluations still only100"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalWindupOnly, "SoulsNew.TemporalWindow.J.WindupOnlyTargetExcluded", Flags)
bool FSoulsTemporalWindupOnly::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); B->SetActorLocation(FVector(1000, 0, 0));
	Step(F, C, 0.6f); TestEqual(TEXT("Initial candidate left before window"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalLateTarget, "SoulsNew.TemporalWindow.K.LateTargetConservativelyExcluded", Flags)
bool FSoulsTemporalLateTarget::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(FVector(1000, 0, 0)); C->TryLightAttack(); B->SetActorLocation(FVector(175, 0, 0));
	Step(F, C, 0.6f); TestEqual(TEXT("Cannot infer presence inside missed window"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalInterruptedActive, "SoulsNew.TemporalWindow.L.InterruptedInsideWindowNoExtraHit", Flags)
bool FSoulsTemporalInterruptedActive::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); Step(F, C, 0.3f); F.Interrupt(C); Step(F, C, 0.7f);
	TestEqual(TEXT("Only confirmed pre-interruption hit"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalMovingOwner, "SoulsNew.TemporalWindow.M.MovingOwnerConservativelyExcluded", Flags)
bool FSoulsTemporalMovingOwner::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack(); C->GetOwner()->SetActorLocation(FVector(10, 0, 0));
	Step(F, C, 0.6f); TestEqual(TEXT("No reconstruction of moving attack volume"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsTemporalOutAndBack, "SoulsNew.TemporalWindow.N.OutAndBackTargetExcluded", Flags)
bool FSoulsTemporalOutAndBack::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack();
	B->SetActorLocation(FVector(1000, 0, 0)); B->SetActorLocation(FVector(175, 0, 0));
	Step(F, C, 0.6f); TestEqual(TEXT("Matching endpoints cannot fabricate presence during missed Active"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	Finish(F, C); F.Advance(); C->TryLightAttack(); Step(F, C, 0.6f);
	TestEqual(TEXT("Old movement flags/subscriptions cleared for next attack"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f); return true;
}

#endif
