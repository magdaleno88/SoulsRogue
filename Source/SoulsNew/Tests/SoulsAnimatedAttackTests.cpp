#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SoulsMeleeTestFixture.h"
#include "Combat/Animation/SoulsAnimNotifyState_MeleeWindow.h"
#include "Animation/AnimSequence.h"
#include "UObject/Package.h"
#include "Items/SoulsItemComponent.h"

using namespace SoulsMeleeTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedStart, "SoulsNew.AnimatedAttack.A.StartIsHarmlessWindup", Flags)
bool FSoulsAnimatedStart::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss();
	TestTrue(TEXT("Montage starts"), C->TryLightAttack());
	TestTrue(TEXT("Windup"), C->GetCombatState() == ESoulsMeleeState::Windup);
	TestTrue(TEXT("Execution identity"), C->GetActiveAttackId().IsValid());
	TestTrue(TEXT("Real montage instance"), C->GetAttackMontageInstanceId() != INDEX_NONE);
	TestEqual(TEXT("No input-time damage"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	TestFalse(TEXT("No overlapping attack"), C->TryLightAttack()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedBeforeWindow, "SoulsNew.AnimatedAttack.B.BeforeWindowCannotSweep", Flags)
bool FSoulsAnimatedBeforeWindow::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(); C->TryLightAttack();
	TestFalse(TEXT("Windup cannot process"), F.Process(C));
	TestFalse(TEXT("Windup cannot close absent window"), F.Close(C));
	TestEqual(TEXT("Target untouched"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedBossWindow, "SoulsNew.AnimatedAttack.C.WindowDamagesVitalityBoss", Flags)
bool FSoulsAnimatedBossWindow::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* B = F.Boss(); C->TryLightAttack();
	TestTrue(TEXT("Open window"), F.Open(C)); TestTrue(TEXT("Active sweep"), F.Process(C));
	TestEqual(TEXT("Vitality boss 625 to 525"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedClose, "SoulsNew.AnimatedAttack.D.ClosedWindowCannotDamage", Flags)
bool FSoulsAnimatedClose::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(); C->TryLightAttack(); F.Open(C);
	TestTrue(TEXT("Close"), F.Close(C)); TestTrue(TEXT("Recovery"), C->GetCombatState() == ESoulsMeleeState::Recovery);
	TestFalse(TEXT("No recovery sweep"), F.Process(C)); TestFalse(TEXT("No reopening"), F.Open(C));
	TestFalse(TEXT("Recovery blocks input"), C->TryLightAttack());
	TestEqual(TEXT("Target untouched"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedDedup, "SoulsNew.AnimatedAttack.E.SingleHitAcrossWindowTicks", Flags)
bool FSoulsAnimatedDedup::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(); F.Collision(T, TEXT("SecondBody"));
	int32 Events = 0; T->FindComponentByClass<USoulsHealthComponent>()->DamageReceived().AddLambda(
		[&](USoulsHealthComponent*, const FSoulsDamageContext&, float, float) { ++Events; });
	C->TryLightAttack(); F.Open(C); for (int32 I = 0; I < 5; ++I) { F.Process(C); }
	TestEqual(TEXT("Exactly one confirmed hit"), Events, 1);
	TestEqual(TEXT("Exactly 100 loss"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedNext, "SoulsNew.AnimatedAttack.F.NextAttackCanHitSameTarget", Flags)
bool FSoulsAnimatedNext::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(); C->TryLightAttack(); const FGuid First = C->GetActiveAttackId();
	F.Open(C); F.Process(C); C->CancelCurrentAttack(); F.Advance();
	TestTrue(TEXT("Next execution"), C->TryLightAttack()); TestTrue(TEXT("New identity"), First != C->GetActiveAttackId());
	F.Open(C); F.Process(C); TestEqual(TEXT("Dedup resets per attack"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 100.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedInterruptWindup, "SoulsNew.AnimatedAttack.G.InterruptWindup", Flags)
bool FSoulsAnimatedInterruptWindup::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(); auto* Anim = F.Mesh(C)->GetAnimInstance();
	const auto Mode = Anim->RootMotionMode; C->TryLightAttack(); F.Interrupt(C);
	TestTrue(TEXT("Idle after engine interruption"), C->GetCombatState() == ESoulsMeleeState::Idle);
	TestFalse(TEXT("No active identity"), C->GetActiveAttackId().IsValid()); TestFalse(TEXT("No late window"), F.Open(C));
	TestTrue(TEXT("Root motion mode restored"), Anim->RootMotionMode == Mode);
	TestEqual(TEXT("No interrupted damage"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedInterruptActive, "SoulsNew.AnimatedAttack.H.InterruptActive", Flags)
bool FSoulsAnimatedInterruptActive::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(); C->TryLightAttack(); F.Open(C); F.Process(C); F.Interrupt(C);
	TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); TestFalse(TEXT("Window closed"), C->IsAttackWindowOpen());
	TestFalse(TEXT("No later sweep"), F.Process(C)); TestEqual(TEXT("Only prior confirmed damage"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedInterruptRecovery, "SoulsNew.AnimatedAttack.I.InterruptRecovery", Flags)
bool FSoulsAnimatedInterruptRecovery::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); C->TryLightAttack(); F.Open(C); F.Close(C); F.Interrupt(C);
	TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); F.Advance();
	TestTrue(TEXT("Not stuck after interrupted recovery"), C->TryLightAttack()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedIdempotent, "SoulsNew.AnimatedAttack.J.IdempotentCleanupAndStaleEnd", Flags)
bool FSoulsAnimatedIdempotent::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); C->TryLightAttack(); const int32 OldId = C->GetAttackMontageInstanceId();
	auto* OldInstance = F.Mesh(C)->GetAnimInstance()->GetMontageInstanceForID(OldId);
	const FOnMontageBlendingOutStarted OldBlendOut = OldInstance->OnMontageBlendingOutStarted;
	const FOnMontageEnded OldEnd = OldInstance->OnMontageEnded;
	F.Open(C); TestTrue(TEXT("First End"), F.Close(C)); TestFalse(TEXT("Duplicate End"), F.Close(C));
	C->CancelCurrentAttack(); C->CancelCurrentAttack(); F.Mesh(C)->GetAnimInstance()->DispatchQueuedAnimEvents(); F.Advance();
	TestTrue(TEXT("New attack"), C->TryLightAttack()); F.Open(C); const FGuid NewId = C->GetActiveAttackId();
	TestFalse(TEXT("Old notify cannot end new window"), C->EndAttackWindow(F.Mesh(C), Montage(), OldId));
	OldBlendOut.ExecuteIfBound(Montage(), true); OldEnd.ExecuteIfBound(Montage(), true);
	TestTrue(TEXT("New window intact"), C->IsAttackWindowOpen()); TestEqual(TEXT("New identity intact"), C->GetActiveAttackId(), NewId); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedNoMontage, "SoulsNew.AnimatedAttack.K.MissingMontageFailsCleanly", Flags)
bool FSoulsAnimatedNoMontage::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); C->SetLightAttackMontage(nullptr);
	TestFalse(TEXT("Missing configuration rejected"), C->TryLightAttack()); TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle);
	C->SetLightAttackMontage(Montage()); TestTrue(TEXT("Failed attempt did not reserve cooldown"), C->TryLightAttack()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedNoAnim, "SoulsNew.AnimatedAttack.L.MissingAnimInstanceFailsCleanly", Flags)
bool FSoulsAnimatedNoAnim::RunTest(const FString&)
{
	FFixture F; auto* A = F.Actor(); auto* Mesh = F.Animation(A); Mesh->SetAnimInstanceClass(nullptr); auto* C = F.Combat(A);
	TestNull(TEXT("No animation instance"), Mesh->GetAnimInstance()); TestFalse(TEXT("Rejected"), C->TryLightAttack());
	TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); TestFalse(TEXT("No identity"), C->GetActiveAttackId().IsValid()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedPlayFailure, "SoulsNew.AnimatedAttack.M.MontagePlayFailureCleansUp", Flags)
bool FSoulsAnimatedPlayFailure::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* Empty = NewObject<UAnimMontage>(); Empty->SetSkeleton(Montage()->GetSkeleton());
	auto* Anim = F.Mesh(C)->GetAnimInstance(); const auto Mode = Anim->RootMotionMode; C->SetLightAttackMontage(Empty);
	TestFalse(TEXT("Zero-length montage cannot play"), C->TryLightAttack()); TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle);
	TestFalse(TEXT("Identity released"), C->GetActiveAttackId().IsValid()); TestTrue(TEXT("Root mode restored"), Anim->RootMotionMode == Mode);
	C->SetLightAttackMontage(Montage()); TestTrue(TEXT("Failure does not reserve cooldown"), C->TryLightAttack()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedLateTarget, "SoulsNew.AnimatedAttack.N.TargetEntersDuringWindow", Flags)
bool FSoulsAnimatedLateTarget::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target(FVector(1000, 0, 0)); C->TryLightAttack(); F.Open(C); F.Process(C);
	TestEqual(TEXT("Initially misses"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f);
	T->SetActorLocation(FVector(150, 0, 0)); F.Process(C); F.Process(C);
	TestEqual(TEXT("Later target hit once"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedProvenance, "SoulsNew.AnimatedAttack.O.RejectWrongNotifySource", Flags)
bool FSoulsAnimatedProvenance::RunTest(const FString&)
{
	FFixture F; auto* C = F.Combat(F.Actor()); auto* Other = F.Animation(F.Actor()); C->TryLightAttack(); const int32 Id = C->GetAttackMontageInstanceId();
	TestFalse(TEXT("Foreign mesh"), C->BeginAttackWindow(Other, Montage(), Id));
	TestFalse(TEXT("Foreign animation"), C->BeginAttackWindow(F.Mesh(C), nullptr, Id));
	TestFalse(TEXT("Absent execution context"), C->BeginAttackWindow(F.Mesh(C), Montage(), INDEX_NONE));
	TestTrue(TEXT("Correct provenance"), F.Open(C)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedOwnerDeath, "SoulsNew.AnimatedAttack.P.OwnerDeathClosesWindow", Flags)
bool FSoulsAnimatedOwnerDeath::RunTest(const FString&)
{
	FFixture F; auto* A = F.Actor(); auto* H = F.Health(A); auto* C = F.Combat(A); C->TryLightAttack(); F.Open(C);
	H->TakeDamage(1000); TestTrue(TEXT("Idle on death"), C->GetCombatState() == ESoulsMeleeState::Idle);
	TestFalse(TEXT("Dead owner cannot restart"), C->TryLightAttack()); TestFalse(TEXT("No post-death sweep"), F.Process(C)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedEndPlay, "SoulsNew.AnimatedAttack.Q.OwnerEndPlayReleasesState", Flags)
bool FSoulsAnimatedEndPlay::RunTest(const FString&)
{
	FFixture F; auto* A = F.Actor(); auto* C = F.Combat(A); C->TryLightAttack(); F.Open(C); A->Destroy();
	TestTrue(TEXT("Idle after owner teardown"), C->GetCombatState() == ESoulsMeleeState::Idle); TestFalse(TEXT("Identity invalidated"), C->GetActiveAttackId().IsValid()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedAmbiguousMesh, "SoulsNew.AnimatedAttack.R.AmbiguousMeshRejected", Flags)
bool FSoulsAnimatedAmbiguousMesh::RunTest(const FString&)
{
	FFixture F; auto* A = F.Actor(); auto* C = F.Combat(A); F.Animation(A, TEXT("OtherMesh"));
	TestFalse(TEXT("No arbitrary animation-source selection"), C->TryLightAttack()); TestTrue(TEXT("Idle"), C->GetCombatState() == ESoulsMeleeState::Idle); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedAssetLoad, "SoulsNew.AnimatedAttack.V.MontageLoadsAndUsesOwnPath", Flags)
bool FSoulsAnimatedAssetLoad::RunTest(const FString&)
{
	auto* M = Montage(); if (!TestNotNull(TEXT("Own montage"), M)) { return false; }
	TestEqual(TEXT("Own package"), M->GetOutermost()->GetName(), FString(TEXT("/Game/SoulsNew/Animations/AM_Souls_LightAttack")));
	TestEqual(TEXT("One animation slot"), M->SlotAnimTracks.Num(), 1);
	if (M->SlotAnimTracks.Num() == 1)
	{
		TestEqual(TEXT("Existing ABP slot"), M->SlotAnimTracks[0].SlotName, FName(TEXT("DefaultSlot")));
		const auto& Segments = M->SlotAnimTracks[0].AnimTrack.AnimSegments; TestEqual(TEXT("One source segment"), Segments.Num(), 1);
		if (Segments.Num() == 1) { TestEqual(TEXT("Existing compatible source"), GetPathNameSafe(Segments[0].GetAnimReference()), FString(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"))); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedSkeleton, "SoulsNew.AnimatedAttack.W.RealPlayerSkeletonAndComponents", Flags)
bool FSoulsAnimatedSkeleton::RunTest(const FString&)
{
	FFixture F; auto* P = F.Player(); TestTrue(TEXT("Exact real-player skeleton"), P->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton() == Montage()->GetSkeleton());
	TestNotNull(TEXT("Real ABP instance"), P->GetMesh()->GetAnimInstance());
	TArray<USoulsMeleeCombatComponent*> Combat; P->GetComponents(Combat); TestEqual(TEXT("One native melee"), Combat.Num(), 1);
	TArray<USoulsHealthComponent*> Health; P->GetComponents(Health); TestEqual(TEXT("One native health"), Health.Num(), 1);
	TArray<USoulsItemComponent*> Items; P->GetComponents(Items); TestEqual(TEXT("One native inventory"), Items.Num(), 1);
	TestTrue(TEXT("Native default montage resolved"), P->GetSoulsMeleeCombat()->GetLightAttackMontage() == Montage()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedNotify, "SoulsNew.AnimatedAttack.X.ExactlyOneNativeWindowNotify", Flags)
bool FSoulsAnimatedNotify::RunTest(const FString&)
{
	auto* M = Montage(); if (!TestNotNull(TEXT("Montage"), M)) { return false; } TestEqual(TEXT("Exactly one notify event"), M->Notifies.Num(), 1);
	if (M->Notifies.Num() == 1)
	{
		auto* N = Cast<USoulsAnimNotifyState_MeleeWindow>(M->Notifies[0].NotifyStateClass); TestNotNull(TEXT("Own native notify"), N);
		if (N) { TestTrue(TEXT("Native branching point dispatch"), N->bIsNativeBranchingPoint); TestTrue(TEXT("Actual event branching point"), M->Notifies[0].IsBranchingPoint()); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAnimatedTiming, "SoulsNew.AnimatedAttack.Y.AuthoredWindowTiming", Flags)
bool FSoulsAnimatedTiming::RunTest(const FString&)
{
	auto* M = Montage(); if (!TestNotNull(TEXT("Montage"), M) || M->Notifies.Num() != 1) { return false; }
	TestEqual(TEXT("Montage length"), M->GetPlayLength(), 1.0f);
	TestTrue(TEXT("Windup .24 seconds"), FMath::IsNearlyEqual(M->Notifies[0].GetTime(), 0.24f));
	TestTrue(TEXT("Active .20 seconds"), FMath::IsNearlyEqual(M->Notifies[0].GetDuration(), 0.20f));
	TestTrue(TEXT("Window fully inside montage"), M->Notifies[0].GetTime() > 0 && M->Notifies[0].GetTime() + M->Notifies[0].GetDuration() < M->GetPlayLength());
	TestTrue(TEXT("Blend in .08"), FMath::IsNearlyEqual(M->BlendIn.GetBlendTime(), 0.08f));
	TestTrue(TEXT("Blend out .10"), FMath::IsNearlyEqual(M->BlendOut.GetBlendTime(), 0.10f)); return true;
}

#endif
