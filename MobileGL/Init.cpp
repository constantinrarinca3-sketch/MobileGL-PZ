// MobileGL - MobileGL/Init.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "Init.h"
#include "Config.h"
#include "PZOptLab.h"
#include <MG_Backend/BackendObjects.h>
#include <MG_Backend/DirectVulkan/DirectVulkan.h>
#include <MG_State/GLState/Core.h>
#include <MG_State/EGLState/Core.h>
#include <MG_Impl/GLImpl/Texture/ProxyTexture.h>
#include <MG_Impl/GLImpl/Framebuffer/GL_Framebuffer.h>
#include <MG_Impl/GLImpl/Sync/GL_Sync.h>
#include <MG_Util/Async/ShaderCompilePool.h>
#include <MG_Util/Debug/AbortTrace.h>
#include <MG_Util/ShaderTranspiler/ShaderCompiler.h>

#include <atomic>
#include <mutex>

namespace MobileGL {
    namespace {
        std::atomic<Bool> g_isInitialized = false;
        thread_local Bool tl_initializing = false;

        std::mutex& InitMutex() {
            static std::mutex mutex;
            return mutex;
        }

        void DestroyImpl(Bool logLifecycle) {
            if (!g_isInitialized) {
                return;
            }

            if (logLifecycle) {
                MGLOG_I("MobileGL closing...");
            }
#ifdef MOBILEPZ_OPT_LAB
            PZOptLab::EmitIncompleteSummaries();
#endif
            // First, before anything else is torn down. In-flight compile/link jobs own
            // their own inputs and are safe against everything below EXCEPT glslang's
            // process globals and the TShader/TProgram objects hanging off pGLContext,
            // both of which this function is about to destroy. This is the one
            // cancellation path in the whole design that waits.
            MG_Util::Async::ShaderCompilePool::Get().StopAndDrain();
            // GL syncs die with their contexts, and every context is gone by the
            // time full teardown runs: drain the live-sync registry while the
            // backend function table can still release the backend handles (and
            // before a re-initialized library could pair them with the wrong
            // backend's DeleteSync).
            MG_Impl::GLImpl::DestroyAllSyncObjects();
            MG_Backend::pActiveBackendObject.reset();
            MG_State::pGLContext.reset();
            MG_State::pEGLContext.reset();
            MG_Impl::GLImpl::TextureImpl::pProxyTextureManager.reset();
            MG_Impl::GLImpl::FramebufferImpl::pDefaultFramebufferInfo.reset();
            // Must run AFTER pGLContext.reset(). FinalizeProcess -> ShFinalize deletes
            // glslang's process-wide pool allocator and every cached built-in symbol table,
            // while the TShader/TProgram objects owned by the shader and program objects
            // still reference levels adopted from those tables. Finalizing first left live
            // glslang objects pointing at freed memory for the rest of the teardown.
            glslang::FinalizeProcess();
            // Immediately after, and never apart from it: FinalizeProcess just deleted the
            // built-in symbol tables the prewarm latch stands for, so leaving it set would
            // make the next Initialize() skip a prewarm it genuinely needs.
            MG_Util::ShaderTranspiler::ShaderCompiler::ResetPrewarmLatch();
            MG_Backend::gBackendFunctionsTable = {};
            g_isInitialized = false;
            if (logLifecycle) {
                MG_Util::Debug::Close();
            }

            // TODO: add and use Destroy functions for other subsystems
        }
    }

    void Initialize() {
        if (g_isInitialized) {
            MGLOG_D("MobileGL already initialized; skipping duplicate Initialize()");
            return;
        }

        MG_Util::Debug::InitFile();
#ifdef MOBILEPZ_V1_NG_WORLD_UNIFORM_LAYOUT_FIX
        MGLOG_I("MOBILEPZ_F1R1_NG_WORLD_LAYOUT_ACTIVE base=P15+P21A+DISPLAY route=CORE_VBO "
                "scope=exact_five_uniform_world_shader layout=MVP0_chunk1_zDepth2_useTexture3_DIFFUSE4 "
                "scalar_mvp_count_gt1=reject_before_write cp2_trace=off global_mvp_reroute=off");
#endif
#ifdef MOBILEPZ_CP2_TRACE
        MGLOG_I("MOBILEPZ_V104_CP2_ACTIVE schema=7 revision=CP2R5 base=P15+P21A+DISPLAY route=OPENGL_CORE "
                "mutation=none one_shot=1 capture_before_draw=1 "
                "target=CP1_SKINNED_VERTEX_SOURCE_SEMANTIC_PRE_LAYOUT auto_arm=1 "
                "compare=frontend_shadow_vs_native_ubo_and_vertex_shader");
#endif
#ifdef MOBILEPZ_V1_ARB_ROUTE
        MGLOG_I("MOBILEPZ_V1_START base=P15+P21A+DISPLAY route=ARB_VBO core_export_glBindBuffer=hidden");
#endif
#ifdef MOBILEGL_PZCOMPAT_ABORT_TRACE
        MG_Util::Debug::AbortTrace::Install();
#endif
        MGLOG_I("Initializing MobileGL...");
        MG_ConfigLoader::Init();
#ifdef MOBILEPZ_OPT_LAB
        PZOptLab::Initialize();
#endif
#ifdef MOBILEPZ_V1_CANDIDATE
        MGLOG_I("MOBILEGL_PZ_V1_FINAL_REPAIRED_ACTIVE schema=5 "
                "base=P7+P15+P21A+P28+P28M+P28V+PZF1+PZF16R1+PZF16R2 "
                "p28h_monitor=removed pzf2_through_pzf15_diagnostics=excluded "
                "quad4_tracker=producer_consumer_handoff "
                "map_world=uiworldmap_stencil+vbo_quad_batches+exact_sdf "
                "idle_surface_context_preserve=on "
                "device_validation=pending");
#endif
#ifdef MOBILEPZ_PZF23D2_CHUNK_FRAGDEPTH_CLAMP
        MGLOG_I("MOBILEPZ_PZF23D2_ACTIVE schema=2 bug=BUG-001_ROOF_AND_CUTAWAY_TEXTURES "
                "base=MAP_FIX_V2+V1 intervention=early_semantic_chunk_fragdepth_clamp "
                "expression=clamp(chunkDepth+depthTexel,0,1) "
                "evidence=fragment_preprocess+rewrite_or_nearmiss collector=PZF23D2 "
                "device_validation=pending");
#endif
#ifdef MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF
        MGLOG_I("MOBILEPZ_PZF23D3_ACTIVE schema=1 bug=BUG-001_ROOF_AND_CUTAWAY_TEXTURES "
                "base=MAP_FIX_V2+V1 intervention=exact_chunk_legacy_alpha_test "
                "proof=runtime_alpha_state+exact_uniform_emulation "
                "hardcoded_cutoff=NO collector=PZF23D3 device_validation=pending");
#endif
#ifdef MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY
        MGLOG_I("MOBILEPZ_PZF23D4_ACTIVE schema=1 bug=BUG-001_ROOF_AND_CUTAWAY_TEXTURES "
                "base=MAP_FIX_V2+V1+D3_CAUSAL_POSITIVE "
                "intervention=exact_custom_shader_legacy_alpha_test_family "
                "contracts=chunkShader+tileWithDepth+opaqueWithDepth+seamFix2 "
                "proof=runtime_alpha_state+per_contract_rewrite+target_draws "
                "hardcoded_cutoff=NO collector=PZF23D4 device_validation=pending");
#endif
#ifdef MOBILEPZ_SL1_SYNC_SHADER_LIFECYCLE
        // Controlled A/B for the run-dependent PZ SKINNED-program disappearance seen in
        // ZomDroidGLTrace. This changes only where compile/link work runs; it does not
        // redirect uniforms, alter shader text, suppress draws, or reset GL state.
        MG_Config::Features.AsyncShaderCompile = MG_Config::QuirkOverride::ForceOff;
#ifdef MOBILEPZ_PZF3_CROSS_FAMILY_TRACE
        MGLOG_I("MOBILEPZ_SL1_ACTIVE schema=1 base=V1.04 route=OPENGL_CORE "
                "async_shader_compile=forced_off lifecycle_log=bounded_128 "
                "render_mutation=none uniform_redirect=none draw_suppression=none");
#else
        MGLOG_I("MOBILEPZ_SL1_ACTIVE schema=1 base=V1.04+CP2R5 route=OPENGL_CORE "
                "async_shader_compile=forced_off lifecycle_log=bounded_128 "
                "render_mutation=none uniform_redirect=none draw_suppression=none");
#endif
#endif
#ifdef MOBILEPZ_PZF1_PZ_MATH_BUILTIN_RENAME
        MGLOG_I("MOBILEPZ_PZF1_ACTIVE schema=1 base=V1.04+SL1 route=OPENGL_CORE "
                "fix=lexical_preempt_rename names=clamp,max,min evidence=glslang_highp_collision "
                "render_mutation=shader_source_only");
#endif
#if defined(MOBILEPZ_PZF1_PZ_MATH_BUILTIN_RENAME) && \
    defined(MOBILEPZ_V1_NG_WORLD_UNIFORM_LAYOUT_FIX)
        MGLOG_I("MOBILEPZ_PZF2_ACTIVE schema=1 base=PZF1 delta=F1R1 "
                "scope=exact_five_uniform_world_shader diagnostics=CP2+SL1 "
                "assimp_change=none p28_change=none device_verdict=pending");
#endif
#ifdef MOBILEPZ_PZF3_CROSS_FAMILY_TRACE
        MGLOG_I("MOBILEPZ_PZF3_ACTIVE schema=1 base=PZF1 route=OPENGL_CORE "
                "families=SKINNED,STATIC_MODEL,WORLD_CHUNK,TEXTURED,OTHER "
                "stages=compile,link,program,draw,uniform,sampler,state "
                "auto_arm=1 bounded=1 rendering_mutation=none error_drain=none "
                "pzf2_f1r1=off p28_ebo_guard=off assimp_change=none");
#endif
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
        MGLOG_I("MOBILEPZ_PZF4_ACTIVE schema=1 base=PZF3+PZF1 route=OPENGL_CORE "
                "probe=draw_signature,uv_bounds,texture_shadow_alpha,any_samples_passed,fbo_output "
                "query=bounded_async_nonblocking object_identity=geometry_texture_signature "
                "rendering_mutation=none shader_rewrite=none alpha_change=none p28_change=none "
                "error_drain=none assimp_change=none");
#endif
#ifdef MOBILEPZ_PZF4R1_CONTEXT_LIFECYCLE_REPAIR
        MGLOG_I("MOBILEPZ_PZF4R1_ACTIVE schema=1 base=PZF4+PZF3+PZF1 "
                "repair=context_generation_rearm reject_counters=explicit "
                "signature_bounds=per_context counters=cumulative rendering_mutation=none");
#endif
#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
        MGLOG_I("MOBILEPZ_PZF5_ACTIVE schema=1 base=PZF4R1+PZF4+PZF3+PZF1 "
                "probe=offscreen_model_target_to_sampler lineage=native_texture_name "
                "readback=bounded_blocking_rows max_captures_per_context=16 "
                "temporary_read_state=restored rendering_output_mutation=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF6_DRAW_DELTA
        MGLOG_I("MOBILEPZ_PZF6_ACTIVE schema=1 base=PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=skinned_static_before_after_same_rows max_pairs_per_context=16 "
                "row_coverage=rotating_strata pzf5_consumer_capture=superseded "
                "temporary_read_state=restored rendering_output_mutation=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
        MGLOG_I("MOBILEPZ_PZF7_ACTIVE schema=1 base=PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=matrix4_call_to_shadow_to_submitted_global_ubo_payload "
                "families=SKINNED,STATIC_MODEL max_captures_per_context=16 "
                "max_captures_per_family=8 transfer_paths=ring,buffer_sub_data "
                "uniform_remap=none shader_rewrite=none rendering_output_mutation=none "
                "additional_gl_calls=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
        MGLOG_I("MOBILEPZ_PZF8_ACTIVE schema=1 base=PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=alpha_useful_offscreen_model_batch_before_after_consumer "
                "families=SKINNED,STATIC_MODEL max_batches_per_context=16 "
                "max_batches_per_primary_family=8 rows_per_batch=64 "
                "all_alpha_zero_start=reject pzf6_single_draw_readback=superseded "
                "uniform_remap=none shader_rewrite=none temporary_read_state=restored "
                "rendering_output_mutation=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
        MGLOG_I("MOBILEPZ_PZF9_ACTIVE schema=1 base=PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=model_sampler_to_client_unpack_to_cpu_shadow_to_driver_payload "
                "families=SKINNED,STATIC_MODEL identity=texture_lifetime "
                "upload_paths=TexImage2D,TexSubImage2D backend_payload=pre_glTexImage_or_SubImage "
                "max_observed_bytes_per_stage=16777216 max_model_logs_per_context=256 "
                "uniform_remap=none shader_rewrite=none rendering_output_mutation=none "
                "additional_gl_calls=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
        MGLOG_I("MOBILEPZ_PZF10_ACTIVE schema=1 base=PZF9+PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "repair=rgba8_copy_readback_subimage shadow=updated_and_clean "
                "paths=CopyTexImage2D,CopyTexSubImage2D max_staged_bytes=4194304 "
                "fallback=native_for_non_rgba8_or_error shader_rewrite=none uniform_remap=none "
                "scope=copy_texture_only");
#endif
#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
        MGLOG_I("MOBILEPZ_PZF11_ACTIVE schema=1 base=PZF10+PZF9+PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=model_sampler_cpu_shadow_to_exact_native_gpu_texture "
                "families=SKINNED,STATIC_MODEL all_zero_per_family=8 useful_controls_per_family=2 "
                "full_read_max_bytes=4194304 sampled_rows=64 temporary_read_fbo=restored_and_detached "
                "blocking_readback=bounded gl_finish=bounded shader_rewrite=none uniform_remap=none "
                "rendering_output_mutation=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
        MGLOG_I("MOBILEPZ_PZF12_ACTIVE schema=1 base=PZF11+PZF10+PZF9+PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=null_allocated_rgba8_draw_targets identity=texture_lifetime+fbo+attachment "
                "repair=reattach_from_frontend_state_on_native_name_mismatch "
                "candidate_size_max=512x512 max_target_states_per_context=256 max_log_lines_per_context=256 "
                "matching_path_mutation=none mismatch_path_mutation=fbo_attachment_resync "
                "additional_gl_calls=bounded_attachment_queries error_drain=none");
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
        MGLOG_I("MOBILEPZ_PZF13_ACTIVE schema=1 base=PZF12+PZF11+PZF10+PZF9+PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=null_texture_attach_status_draw_clear_blit_direct_image_lineage "
                "identity=texture_lifetime+fbo+attachment+native_name "
                "repair=reattach_before_first_draw_clear_or_blit_on_native_name_mismatch "
                "matching_path_mutation=none max_records=8192 max_identity_keys=512 "
                "additional_gl_calls=bounded_attachment_queries error_drain=none");
#endif
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
        MGLOG_I("MOBILEPZ_PZF14_ACTIVE schema=1 base=PZF13+PZF12+PZF11+PZF10+PZF9+PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=all_native_gles_draws_inside_null_texture_clear_to_detach_windows "
                "identity=texture_lifetime+front_fbo+attachment+client_sequence "
                "repair=backend_draw_fbo_or_program_rebind_only_on_proven_frontend_driver_mismatch "
                "matching_path_mutation=none max_windows=4096 max_state_queries=8192 "
                "max_raw_log_lines=512 max_session_log_lines=512 error_drain=none");
#endif
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        MGLOG_I("MOBILEPZ_PZF15_ACTIVE schema=1 base=PZF14+PZF13+PZF12+PZF11+PZF10+PZF9+PZF8+PZF7+PZF6+PZF5+PZF4R1+PZF4+PZF3+PZF1 "
                "probe=texturecombiner_lwjgl_resolve_to_legacy_entry_to_immediate_dispatch "
                "correlation=clear_to_detach_front_fbo_window events=begin,vertex,end,frontend_draw,immediate_attempt,immediate_dispatch "
                "max_event_log_lines=512 max_events_per_window=8 rendering_output_mutation=none "
                "additional_gl_calls=none error_drain=none");
#endif
#ifdef MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX
        MGLOG_I("MOBILEPZ_PZF16_ACTIVE schema=1 evidence=PZF15_FRONTEND_DRAW_NO_NATIVE_SUBMIT "
                "scope=draw_arrays+draw_elements+draw_range_elements exact_target=mode_0x7_count_4 "
                "fix=GL_QUADS_to_GL_TRIANGLE_FAN route=preserve_active_program_or_legacy_client_fallback "
                "guard=no_program_and_no_legacy_vertex_array_means_no_mutation state_mutation=none "
                "additional_gl_calls=none error_drain=none rollback=PZF15");
#endif
#if defined(MOBILEPZ_PERF005_SAMPLER_TELEMETRY) && !defined(MOBILEPZ_OPT_LAB)
        // Performance builds compile INFO and below out.  Use one bounded FATAL-level
        // identity record so the device collector can prove which A/B binary ran;
        // this is a marker only and does not terminate the process.
#ifdef MOBILEPZ_PERF005_PROGRAM_AWARE_SAMPLER_BINDING
        MGLOG_F("MOBILEPZ_PERF005_ACTIVE schema=1 variant=B_PROGRAM_AWARE_SAMPLER_BINDING "
                "base=PZF23D4+PERF001+PERF002+PERF003 perf004=off "
                "sampler_binding_mask=on full_walk_fallback=no_program_or_non_draw telemetry_calls=4096");
#else
        MGLOG_F("MOBILEPZ_PERF005_ACTIVE schema=1 variant=A_CUMULATIVE_COMPARATOR "
                "base=PZF23D4+PERF001+PERF002+PERF003 perf004=off "
                "sampler_binding_mask=off full_walk_fallback=all_calls telemetry_calls=4096");
#endif
#endif
        MGLOG_I("Config loaded");
        MG_State::Init();
        MGLOG_D("MG_State initialized");
        MG_Backend::Init();
        MGLOG_D("MG_Backend initialized");
        MG_Impl::Init();
        MGLOG_D("MG_Impl initialized");
        glslang::InitializeProcess();
        // On the GL thread, before any worker can exist. glslang builds its built-in symbol
        // tables lazily under a process-wide lock held for the whole build, so without this
        // the first concurrent compiles of a shaderpack all serialize behind the very first
        // parse and asynchronous compilation looks like it is doing nothing.
        //
        // Gated on the flag, because the problem it solves only exists when there are
        // workers: with compilation synchronous, nothing ever contends for that lock and the
        // three throwaway parses buy nothing - they just add to every eglInitialize. Read the
        // flag here rather than inside PrewarmBuiltins so ShaderCompiler keeps no dependency
        // on the async subsystem (ProgramUtilTest compiles that file without it).
        if (MG_Util::Async::AsyncShaderCompileEnabled()) {
            MG_Util::ShaderTranspiler::ShaderCompiler::PrewarmBuiltins();
        }
        MGLOG_D("glslang initialized");
        g_isInitialized = true;
        MGLOG_I("MobileGL initialized");
    }

    void EnsureInitialized() {
        if (g_isInitialized.load(std::memory_order_acquire)) {
            return;
        }
        // Re-entrant call while this thread is already inside Initialize()
        // (e.g. an init step routing back through a public entry point).
        if (tl_initializing) {
            return;
        }
        const std::lock_guard<std::mutex> lock(InitMutex());
        if (g_isInitialized.load(std::memory_order_acquire)) {
            return;
        }
        tl_initializing = true;
        Initialize();
        tl_initializing = false;
    }

    void Destroy() {
        DestroyImpl(true);
    }

    // MobileGL's lifecycle is owned entirely by the host-API layers
    // (EGL/WGL/CGL): initialization happens lazily on the first entry point
    // via EnsureInitialized(), and full teardown happens deterministically
    // when the last EGL display is terminated with nothing current (EGLImpl
    // calls Destroy()). There is intentionally no backend-initializing static
    // constructor, no static destructor, and no DllMain: the global singletons
    // use leak-at-exit storage (see GlobalObjects.cpp), so a process that exits
    // without eglTerminate simply leaks them to the OS instead of running
    // backend destructors during static teardown. macOS has a lightweight
    // dyld constructor that installs NSOpenGL dispatch hooks only; full backend
    // initialization still enters here from the first hooked CGL context.
} // namespace MobileGL
