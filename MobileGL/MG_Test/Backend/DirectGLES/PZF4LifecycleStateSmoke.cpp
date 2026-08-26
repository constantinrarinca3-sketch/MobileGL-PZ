#include "../../../MG_Backend/DirectGLES/PZF4LifecycleState.h"

#include <cstdio>

namespace {

using MobilePZ::PZF4R1::LifecycleState;

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::fprintf(stderr, "PZF4R1_LIFECYCLE_SMOKE_FAIL %s\n", message);
    return false;
}

} // namespace

int main() {
    LifecycleState rearm;
    if (!Check(rearm.OnContextReady(1), "context A did not arm")) return 1;
    rearm.OnContextDestroyed();
    if (!Check(rearm.OnContextReady(2), "context B did not rearm")) return 1;

    const auto begin = rearm.Begin(6, 1);
    if (!Check(begin.allowed && begin.firstInGeneration, "first valid B draw was rejected")) return 1;
    const auto reserve = rearm.Reserve(0, 0x504a463400000001ull);
    if (!Check(reserve.selected && reserve.drawId == 1, "first B draw was not selected as DRAW_ID=1")) return 1;
    if (!Check(rearm.BeginCalls() == 1 && rearm.Selected() == 1 && rearm.SignatureCount() == 1,
               "rearm counters are wrong")) return 1;

    LifecycleState gap;
    if (!Check(gap.OnContextReady(7), "gap context did not arm")) return 1;
    gap.OnContextDestroyed();
    const auto rejected = gap.Begin(3, 1);
    if (!Check(!rejected.allowed &&
                   rejected.reject == LifecycleState::RejectReason::ContextNotReady,
               "draw in context-not-ready gap was not rejected")) return 1;
    if (!Check(gap.BeginCalls() == 1 && gap.RejectContextNotReady() == 1 &&
                   gap.Selected() == 0 && gap.SignatureCount() == 0,
               "gap rejection mutated selection state")) return 1;

    LifecycleState invalidCounts;
    if (!Check(invalidCounts.OnContextReady(9), "invalid-count context did not arm")) return 1;
    if (!Check(invalidCounts.Begin(0, 1).reject == LifecycleState::RejectReason::CountZero,
               "count-zero draw was not classified")) return 1;
    if (!Check(invalidCounts.Begin(1, 0).reject == LifecycleState::RejectReason::InstancesZero,
               "instances-zero draw was not classified")) return 1;
    if (!Check(invalidCounts.RejectCountZero() == 1 &&
                   invalidCounts.RejectInstancesZero() == 1,
               "invalid-count counters are wrong")) return 1;

    LifecycleState duplicate;
    if (!Check(duplicate.OnContextReady(10), "duplicate context did not arm")) return 1;
    if (!Check(duplicate.Begin(1, 1).allowed &&
                   duplicate.Reserve(0, 0x100).selected,
               "duplicate seed was not selected")) return 1;
    if (!Check(duplicate.Begin(1, 1).allowed &&
                   duplicate.Reserve(0, 0x100).reject ==
                       LifecycleState::RejectReason::DuplicateSignature &&
                   duplicate.RejectDuplicateSignature() == 1,
               "duplicate signature was not classified")) return 1;

    LifecycleState familyLimit;
    if (!Check(familyLimit.OnContextReady(11), "family-limit context did not arm")) return 1;
    for (std::uint64_t i = 0; i < LifecycleState::kFamilyLimits[4]; ++i) {
        if (!Check(familyLimit.Begin(1, 1).allowed &&
                       familyLimit.Reserve(4, 0x200 + i).selected,
                   "family-limit seed was not selected")) return 1;
    }
    if (!Check(familyLimit.Begin(1, 1).allowed &&
                   familyLimit.Reserve(4, 0x2ff).reject ==
                       LifecycleState::RejectReason::FamilyLimit &&
                   familyLimit.RejectFamilyLimit() == 1,
               "family limit was not classified")) return 1;

    LifecycleState globalLimit;
    if (!Check(globalLimit.OnContextReady(12), "global-limit context did not arm")) return 1;
    std::uint64_t signature = 0x1000;
    for (std::size_t family = 0; family < LifecycleState::kFamilyCount; ++family) {
        for (std::uint64_t i = 0; i < LifecycleState::kFamilyLimits[family]; ++i) {
            if (!Check(globalLimit.Begin(1, 1).allowed &&
                           globalLimit.Reserve(family, signature++).selected,
                       "global-limit seed was not selected")) return 1;
        }
    }
    if (!Check(globalLimit.SignatureCount() == LifecycleState::kMaxSignatures &&
                   globalLimit.Begin(1, 1).allowed &&
                   globalLimit.Reserve(0, signature).reject ==
                       LifecycleState::RejectReason::GlobalLimit &&
                   globalLimit.RejectGlobalLimit() == 1,
               "global limit was not classified")) return 1;

    LifecycleState generations;
    if (!Check(generations.OnContextReady(20), "generation context did not arm")) return 1;
    if (!Check(generations.Begin(1, 1).allowed &&
                   generations.Reserve(0, 0x300).selected,
               "generation seed was not selected")) return 1;
    if (!Check(!generations.OnContextReady(20) && generations.SignatureCount() == 1,
               "redundant MakeCurrent reset the generation")) return 1;
    generations.OnContextDestroyed();
    if (!Check(generations.OnContextReady(21) && generations.SignatureCount() == 0 &&
                   generations.Selected() == 1,
               "new generation did not reset bounded selection only")) return 1;

    std::puts("PZF4R1_LIFECYCLE_SMOKE lifecycle_rearm=PASS begin_calls=1 selected=1 signatures=1 draw_id=1");
    std::puts("PZF4R1_LIFECYCLE_SMOKE context_not_ready=PASS gl_calls=0 reject_context_not_ready=1");
    std::puts("PZF4R1_LIFECYCLE_SMOKE reject_counters=PASS count_zero=1 instances_zero=1 duplicate_signature=1 family_limit=1 global_limit=1");
    std::puts("PZF4R1_LIFECYCLE_SMOKE generation_bounds=PASS redundant_ready_preserved=1 next_generation_reset=1 cumulative_selected=1");
    return 0;
}
