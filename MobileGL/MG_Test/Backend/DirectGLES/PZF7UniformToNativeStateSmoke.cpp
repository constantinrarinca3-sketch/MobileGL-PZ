#include "../../../MG_Backend/DirectGLES/PZF7UniformToNativeState.h"

#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace MobilePZ::PZF7;

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "PZF7_UNIFORM_TO_NATIVE_STATE_SMOKE_FAIL %s\n", message);
    std::exit(1);
}

int main() {
    TraceState state;
    Require(state.OnContextReady(2), "first_context_ready");
    Require(!state.OnContextReady(2), "duplicate_make_current_must_not_reset");

    std::array<std::uint8_t, 128> shadow{};
    for (std::size_t i = 0; i < shadow.size(); ++i) {
        shadow[i] = static_cast<std::uint8_t>((i * 17u + 3u) & 0xffu);
    }

    MatrixCallInput mvp;
    mvp.family = Family::Skinned;
    mvp.role = MatrixRole::Mvp;
    mvp.route = MatrixRoute::Uniform;
    mvp.outcome = MatrixOutcome::ShadowChanged;
    mvp.program = 4;
    mvp.lifetime = 44;
    mvp.location = 75;
    mvp.count = 1;
    mvp.type = 0x8b5c;
    mvp.beforeVersion = 9;
    mvp.afterVersion = 10;
    mvp.effectiveInputHash = HashBytes(shadow.data(), 64);
    mvp.uniformName = "ModelViewProjection";
    mvp.locationValid = true;
    mvp.typeMatch = true;
    const MatrixRecordResult mvpResult = state.RecordMatrix(mvp);
    Require(mvpResult.shouldLog && mvpResult.call.sequence == 1, "mvp_record");
    Require(std::strcmp(mvpResult.call.uniformName.data(), "ModelViewProjection") == 0,
            "uniform_name_termination");

    UboBindInput ring;
    ring.program = 4;
    ring.lifetime = 44;
    ring.contentVersion = 10;
    ring.frontendData = shadow.data();
    ring.frontendSize = shadow.size();
    ring.path = UboPath::Ring;
    ring.submittedData = shadow.data();
    ring.submittedSize = shadow.size();
    ring.submittedKnown = true;
    ring.freshSubmission = true;
    ring.ringGeneration = 7;
    ring.frameSerial = 100;
    ring.ringOffset = 4096;
    state.RecordUboBinding(ring);

    const CaptureDecision first = state.CaptureModelDraw(Family::Skinned, 4, 44, 0);
    Require(first.selected && first.capture == 1, "first_capture");
    Require(first.latestMvp.valid && first.latestMvp.location == 75, "latest_mvp");
    Require(first.binding.submittedKnown &&
            first.binding.frontendHash == first.binding.submittedHash,
            "matching_payload");
    Require(!state.CaptureModelDraw(Family::Skinned, 4, 44, 0).selected,
            "one_capture_per_present");

    MatrixCallInput collision = mvp;
    collision.role = MatrixRole::Other;
    collision.outcome = MatrixOutcome::RejectedOpaque;
    collision.location = 0;
    collision.type = 0x8b5e;
    collision.uniformName = "Texture";
    collision.opaque = true;
    collision.typeMatch = false;
    collision.effectiveInputHash = 0;
    state.RecordMatrix(collision);

    state.OnPresentBoundary();
    auto mismatch = shadow;
    mismatch[17] ^= 0xffu;
    ring.contentVersion = 11;
    ring.frontendData = shadow.data();
    ring.submittedData = mismatch.data();
    ring.frameSerial = 101;
    state.RecordUboBinding(ring);
    Require(state.CaptureModelDraw(Family::Skinned, 4, 44, 1).selected,
            "second_present_capture");

    const Summary summary = state.GetSummary();
    Require(summary.captures == 2 && summary.payloadMatch == 1 &&
            summary.payloadMismatch == 1, "payload_buckets");
    Require(summary.matrixLocationZeroOpaque == 1 &&
            summary.modelMatrixLocationZeroOpaque == 1 &&
            summary.outcomeRejectedOpaque == 1, "opaque_location_zero");

    state.OnContextDestroyed();
    Require(state.OnContextReady(3), "generation_rearm");
    const Summary rearmed = state.GetSummary();
    Require(rearmed.contextGeneration == 3 && rearmed.captures == 0 &&
            rearmed.matrixCalls == 0, "context_reset");

    std::puts("PZF7_UNIFORM_TO_NATIVE_STATE_SMOKE state=PASS payload_match=1 "
              "payload_mismatch=1 location0_opaque=1 rearm=PASS");
    return 0;
}
