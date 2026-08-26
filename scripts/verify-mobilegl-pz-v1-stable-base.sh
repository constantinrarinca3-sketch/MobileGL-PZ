#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
DIRECT_GLES="$SOURCE_ROOT/MobileGL/MG_Backend/DirectGLES/DirectGLES.cpp"
BACKEND_OBJECT="$SOURCE_ROOT/MobileGL/MG_Backend/DirectGLES/BackendObject_DirectGLES.cpp"
INIT_SOURCE="$SOURCE_ROOT/MobileGL/Init.cpp"
BUILD_SCRIPT="$SOURCE_ROOT/scripts/build-mobilegl-pz-v1-android.sh"
PZ_COMPAT="$SOURCE_ROOT/MobileGL/MG_Impl/GLImpl/PZCompat/PZCompat.cpp"
MAP_COMPAT="$SOURCE_ROOT/MobileGL/MG_Util/PZV1/PZV1MapCompat.h"
PZ_TRACKER="$SOURCE_ROOT/MobileGL/MG_Util/PZV1/PZV1QuadTargetTracker.h"
FRAMEBUFFER_STATE="$SOURCE_ROOT/MobileGL/MG_State/GLState/FramebufferState/FramebufferObject.cpp"
PZF1_TEST="$SOURCE_ROOT/MobileGL/MG_Test/Program/ProgramUtilTest.cpp"
PZF1_V1_TEST="$SOURCE_ROOT/MobileGL/MG_Test/Program/PZF1V1ActivationTest.cpp"
PROGRAM_TEST_CMAKE="$SOURCE_ROOT/MobileGL/MG_Test/Program/CMakeLists.txt"
MAP_COMPAT_TEST="$SOURCE_ROOT/MobileGL/MG_Test/Program/PZV1MapCompatTest.cpp"
TRACKER_TEST="$SOURCE_ROOT/MobileGL/MG_Test/State/PZV1QuadTargetTrackerTest.cpp"
SOURCE_MANIFEST="$SOURCE_ROOT/SOURCE-FILES-SHA256.txt"

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

require_text() {
    local pattern="$1"
    local file="$2"
    rg -q -- "$pattern" "$file" || fail "missing '$pattern' in ${file#"$SOURCE_ROOT/"}"
}

window_body="$(sed -n \
    '/Bool BackendObject_DirectGLES::CreateEGLWindowSurface/,/Bool BackendObject_DirectGLES::CreateEGLPbufferSurface/p' \
    "$BACKEND_OBJECT")"
pbuffer_body="$(sed -n \
    '/Bool BackendObject_DirectGLES::CreateEGLPbufferSurface/,/Bool BackendObject_DirectGLES::InitPbufferSurface/p' \
    "$BACKEND_OBJECT")"
surface_release_body="$(sed -n \
    '/void BackendObject_DirectGLES::OnEGLSurfaceReleased/,/const RendererInfo& BackendObject_DirectGLES::GetRendererInfo/p' \
    "$BACKEND_OBJECT")"
native_surface_release_body="$(sed -n \
    '/Bool ReleaseSurface()/,/^    namespace {/p' \
    "$DIRECT_GLES")"

if grep -Eq 'DestroyEGLContext|ResetEGLRuntimeState' <<<"$window_body"; then
    fail "window-surface wrapper still destroys EGL runtime before P28M"
fi
if grep -Eq 'DestroyEGLContext|ResetEGLRuntimeState' <<<"$pbuffer_body"; then
    fail "pbuffer-surface wrapper still destroys EGL runtime before P28M"
fi

if rg -q -- \
    'PZV1Health|MOBILEGL_PZ_V1_HEALTH|MOBILEGL_PZ_V1_EGL_TRANSITION|MOBILEGL_PZ_V1_PRESENT|MOBILEGL_PZ_V1_QUAD4' \
    "$SOURCE_ROOT/MobileGL"; then
    fail "P28H or V1 route telemetry remains in production sources"
fi

require_text 'PZCOMPAT_P28M_CONTEXT_PRESERVE kind=window' "$DIRECT_GLES"
require_text 'PZCOMPAT_P28M_CONTEXT_PRESERVE kind=pbuffer' "$DIRECT_GLES"
require_text 'Bool ReleaseSurface\(\)' "$DIRECT_GLES"
require_text 'MOBILEGL_PZ_V1_EGL_IDLE_PRESERVE' "$DIRECT_GLES"
grep -Eq 'if \(!DirectGLES::ReleaseSurface\(\)\)' <<<"$surface_release_body" ||
    fail "surface release does not attempt context-preserving native-surface teardown"
grep -q 'ReleaseCurrent()' <<<"$native_surface_release_body" ||
    fail "native surface release does not detach the current context"
grep -q 'eglDestroySurface' <<<"$native_surface_release_body" ||
    fail "native surface release does not destroy the old EGLSurface"
grep -q 'g_Surface = EGL_NO_SURFACE' <<<"$native_surface_release_body" ||
    fail "native surface release does not clear the native surface handle"
if grep -q 'DestroyEGLContext' <<<"$native_surface_release_body"; then
    fail "native surface-only release still destroys the EGL context"
fi
require_text 'MOBILEGL_PZ_V1_FINAL_REPAIRED_ACTIVE schema=5' "$INIT_SOURCE"
require_text 'idle_surface_context_preserve=on' "$INIT_SOURCE"
require_text 'quad4_tracker=producer_consumer_handoff' "$INIT_SOURCE"
require_text 'map_world=uiworldmap_stencil\+vbo_quad_batches\+exact_sdf' "$INIT_SOURCE"
require_text 'VertexArrayImpl::InvalidateVAOBindingCache\(\)' "$DIRECT_GLES"
require_text 'm_syncedIndexBufferLifetimeId' \
    "$SOURCE_ROOT/MobileGL/MG_Backend/DirectGLES/Managers.h"
require_text 'RestoreClientAttribStateFrom' \
    "$PZ_COMPAT"
require_text 'name == "clamp".*name == "max".*name == "min"' \
    "$SOURCE_ROOT/MobileGL/MG_Util/ShaderTranspiler/EsslBuiltinFunctionNames.h"
require_text 'defined\(MOBILEPZ_PZF1_PZ_MATH_BUILTIN_RENAME\).*defined\(MOBILEPZ_V1_CANDIDATE\)' \
    "$PZF1_TEST"
require_text 'V1EnablesAcceptedProjectZomboidMathNames' "$PZF1_V1_TEST"
require_text 'target_compile_definitions\(PZF1V1ActivationTest PRIVATE' "$PROGRAM_TEST_CMAKE"
require_text 'MOBILEPZ_V1_CANDIDATE=1' "$PROGRAM_TEST_CMAKE"
require_text 'PZV1MapCompatTest' "$PROGRAM_TEST_CMAKE"
require_text 'IsQuadBatch' "$MAP_COMPAT"
require_text 'IsWorldMapStencilSignature' "$MAP_COMPAT"
require_text 'HasVboRendererProgramInterface' "$MAP_COMPAT"
require_text 'ShouldConvertWorldMapVboQuadBatch' "$MAP_COMPAT"
require_text 'ShouldConvertPZV1WorldMapQuadBatch' "$PZ_COMPAT"
require_text 'SubmitPZV1QuadBatchArrays' "$PZ_COMPAT"
require_text 'SubmitPZV1QuadBatchElements' "$PZ_COMPAT"
require_text 'SubmitPZV1QuadBatchRangeElements' "$PZ_COMPAT"
require_text 'WorldMapRouteNeedsStateInterfaceAndQuadBatch' "$MAP_COMPAT_TEST"
require_text 'GLImpl::DrawRangeElements\(' "$PZ_COMPAT"
require_text 'GL_TRIANGLE_FAN' "$PZ_COMPAT"
require_text 'PZV1SmallRenderTargetOnUnit0' "$PZ_COMPAT"
require_text 'TryConsumeCompositeTexture' "$PZ_COMPAT"
require_text 'MarkFramebufferQuadWritten' "$PZ_COMPAT"
require_text 'PublishAndDeactivateFramebuffer' "$PZ_TRACKER"
require_text 'PublishAndDeactivateFramebuffer' "$FRAMEBUFFER_STATE"
require_text 'quadWritten' "$PZ_TRACKER"
require_text 'DirectMapCollisionIsFailClosed' "$TRACKER_TEST"
if rg -q -- 'WasNullAllocated' "$PZ_COMPAT"; then
    fail "quad draw path takes the NULL-allocation mutex"
fi
require_text 'MOBILEPZ_V1_CANDIDATE excludes legacy/test option' "$SOURCE_ROOT/CMakeLists.txt"
require_text 'MOBILEPZ_V1_BUILD_ID_HEX' "$SOURCE_ROOT/CMakeLists.txt"
require_text '--build-id=0x' "$SOURCE_ROOT/CMakeLists.txt"
require_text 'SOURCE-FILES-SHA256.txt' "$BUILD_SCRIPT"
require_text 'sha256sum --quiet -c' "$BUILD_SCRIPT"

legacy_options=(
    MOBILEGL_PZCOMPAT_ABORT_TRACE
    MOBILEPZ_V1_ARB_ROUTE
    MOBILEPZ_CP2_TRACE
    MOBILEPZ_SL1_SYNC_SHADER_LIFECYCLE
    MOBILEPZ_PZF1_PZ_MATH_BUILTIN_RENAME
    MOBILEPZ_V1_NG_WORLD_UNIFORM_LAYOUT_FIX
    MOBILEPZ_PZF3_CROSS_FAMILY_TRACE
    MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
    MOBILEPZ_PZF4R1_CONTEXT_LIFECYCLE_REPAIR
    MOBILEPZ_PZF5_RENDER_TARGET_READBACK
    MOBILEPZ_PZF6_DRAW_DELTA
    MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
    MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
    MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
    MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
    MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
    MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
    MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
    MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
    MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
    MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX
)

for option in "${legacy_options[@]}"; do
    require_text "-D${option}=OFF" "$BUILD_SCRIPT"
    if rg -q -- "-D${option}=ON" "$BUILD_SCRIPT"; then
        fail "legacy/test option enabled by stable build helper: $option"
    fi
done

test -f "$SOURCE_MANIFEST" || fail "missing SOURCE-FILES-SHA256.txt"
(cd "$SOURCE_ROOT" && sha256sum --quiet -c "$SOURCE_MANIFEST") ||
    fail "source manifest does not match the final tree"

echo "PASS: final repaired source invariants (device validation remains separate)"
