// MobileGL - MobileGL/MG_Util/ShaderTranspiler/ShaderSourceProcessor.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once
#include <Includes.h>
#include <MG_State/GLState/ProgramState/ShaderObject.h>
#include <MG_Util/ShaderTranspiler/CompileEnv.h>

namespace MobileGL {
    enum class ShaderProfile {
        Core,
        Compatibility,
        ES,
    };

    namespace MG_Util {
        namespace ShaderTranspiler {
            // The whole source-rewriting pipeline. `env` is the compile-time snapshot of
            // everything outside (stage, source) this reads - advertised extensions and the
            // device-quirk inputs - so the transformation is a pure function of its three
            // arguments and can run on a worker thread.
            void PreprocessShaderSource(ShaderStage stage, String& source, const CompileEnv& env);
            // Convenience overload that resolves the current context's env itself. GL thread
            // only, and deliberately not used by the compile pipeline: it exists for the unit
            // tests and diagnostics that drive the preprocessor standalone.
            void PreprocessShaderSource(ShaderStage stage, String& source);

            // Some desktop-captured compute shaders build a workgroup-wide linear prefix scan
            // from subgroupInclusiveAdd plus a shared array of subgroup totals. Qualcomm's
            // Vulkan driver miscompiles that exact float InclusiveScan path for native subgroups
            // wider than the capture's 32 lanes. For the narrowly recognized, uniform-control-
            // flow template, replace the subgroup-local scan with a shared-memory, strict
            // left-fold over virtual 32-lane segments. Returns true only when the complete safe
            // template was recognized and rewritten. PreprocessShaderSource reaches this through
            // its device-quirk registry: by default only on detected Qualcomm Vulkan devices,
            // overridable either way with MOBILEGL_QUIRK_SUBGROUP_PREFIX_SCAN=1/0. The explicit
            // entry point exists for deterministic tests.
            Bool RewriteLinearSubgroupPrefixScanForVulkan(ShaderStage stage, Uint32 nativeSubgroupSize, String& source);

            struct PZChunkFragDepthProbe {
                Bool candidate = false;
                Bool contractMatched = false;
                Bool rewritten = false;
                SizeT fragDepthTokens = 0;
                SizeT chunkDepthTokens = 0;
                SizeT depthTexelTokens = 0;
                SizeT depthSamplerTokens = 0;
            };

            // Runs on the untouched application source, before version normalization or any
            // generic MobileGL pass. It recognizes the semantic PZ chunk contract rather than
            // unrelated colour-path details: one fragment-depth write, the unique
            // chunkDepth+depthTexel assignment, and the two uniforms that feed it. The probe
            // exposes bounded near-miss evidence without dumping shader source.
            PZChunkFragDepthProbe RewritePZChunkFragDepthClampEarly(ShaderStage stage, String& source);

            struct PZChunkAlphaTestProbe {
                Bool candidate = false;
                Bool contractMatched = false;
                Bool rewritten = false;
                SizeT fragDepthTokens = 0;
                SizeT fragColorTokens = 0;
                SizeT chunkDepthTokens = 0;
                SizeT depthTexelTokens = 0;
                SizeT depthSamplerTokens = 0;
                SizeT diffuseSamplerTokens = 0;
            };

            // Runs before generic preprocessing and recognizes the complete Project Zomboid
            // chunk-composite fragment contract.  The rewrite does not assume an alpha cutoff:
            // it adds uniforms for the legacy GL_ALPHA_TEST enable/function/reference state and
            // implements exactly that comparison against the final c*col alpha.  PZCompat fills
            // those uniforms at the target custom-program draw and logs the same state.
            PZChunkAlphaTestProbe RewritePZChunkAlphaTestEarly(ShaderStage stage, String& source);

            enum class PZCustomAlphaShaderKind {
                None,
                ChunkComposite,
                TileWithDepth,
                OpaqueWithDepth,
                SeamFix2,
            };

            struct PZCustomAlphaTestProbe {
                Bool candidate = false;
                Bool contractMatched = false;
                Bool rewritten = false;
                PZCustomAlphaShaderKind kind = PZCustomAlphaShaderKind::None;
                SizeT fragDepthTokens = 0;
                SizeT fragColorTokens = 0;
                SizeT depthSamplerTokens = 0;
                SizeT diffuseSamplerTokens = 0;
                SizeT maskSamplerTokens = 0;
            };

            // D4 extends the D3 causal-positive correction to the exact PZ shaders that
            // populate the chunk colour/depth pair.  Those custom fragment programs bypass
            // PZCompat's fixed-function fallback just like chunkShader did, so the legacy
            // alpha test must be represented explicitly in each program.  Only the four
            // captured semantic contracts below are accepted; all other shaders fail closed.
            PZCustomAlphaTestProbe RewritePZCustomAlphaTestFamilyEarly(ShaderStage stage, String& source);

            // Rewrites a "#version 330 core" directive that PreprocessShaderSource normalized down
            // from a legacy desktop version back up to "#version 460 core". Returns false (leaving
            // the source untouched) for anything else: ES, compatibility, or an already-modern
            // declaration. Exists so a shader that only parses under the laxer 460 rules - e.g. it
            // uses 420-era syntax without the matching #extension line, which real drivers tend to
            // accept - can be retried instead of failing to compile.
            Bool RetargetLegacyVersionDirectiveTo460(String& source);

            // GLSL reserves a few names glslang happily accepts as identifiers ("packed",
            // "row_major" outside a layout(...) list, the image*Shadow family). Returns the
            // compile-error text for the first violation, or nullopt for a clean source.
            std::optional<String> FindReservedIdentifierViolation(const String& source);

            // Explicit layout(location = N) qualifiers on default-block uniform declarations,
            // keyed by declared name (no "[0]" suffix). Multi-declarator statements assign
            // consecutive locations, advancing by the array element count.
            //
            // Exists because the single link-compatible parse runs under relaxed Vulkan rules,
            // where glslang's vkRelaxedRemapUniformVariable moves plain uniforms into
            // MGL_GLOBAL_UBO and DISCARDS their location qualifiers ("ignoring layout qualifier
            // for uniform location"); opaque uniforms keep theirs. This lexical side-channel
            // restores the discarded locations to the GL location assigner
            // (ProgramObject::DoReflection). It scans preprocessor-visible text, so a
            // declaration inside an inactive #if branch is still recorded - harmless unless a
            // pack declares the same uniform with different explicit locations in alternative
            // branches (none observed; explicit uniform locations have zero incidence in the
            // shader-pack corpus, this is an ARB_explicit_uniform_location conformance surface).
            UnorderedMap<String, Int> ExtractExplicitUniformLocations(const String& source);

            // Explicit layout(binding = N) on sampler/image uniforms, i.e. their initial
            // texture/image units. The Vulkan-client relaxed parse strips these before
            // mapIO can capture them, so they are recovered lexically (same narrow
            // grammar discipline as ExtractExplicitUniformLocations).
            UnorderedMap<String, Uint> ExtractExplicitOpaqueBindings(const String& source);
        } // namespace ShaderTranspiler
    } // namespace MG_Util
} // namespace MobileGL
