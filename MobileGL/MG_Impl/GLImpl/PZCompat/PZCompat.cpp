// MobileGL-PZCompat - isolated Project Zomboid compatibility frontend.
// SPDX-License-Identifier: LGPL-3.0-only

#include "PZCompat.h"

#include "../Buffer/GL_Buffer.h"
#include "../Drawing/GL_Drawing.h"
#include "../Getter/GL_Getter.h"
#include "../Program/GL_Program.h"
#include "../RenderState/GL_RenderState.h"
#include "../Texture/GL_Texture.h"
#include "../VertexArray/GL_VertexArray.h"
#include <MG_State/GLState/Core.h>
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
#include <MG_Util/Texture/PZF14ClearToDetachState.h>
#include <thread>
#endif
#ifdef MOBILEPZ_V1_CANDIDATE
#include <MG_Util/PZV1/PZV1MapCompat.h>
#include <MG_Util/PZV1/PZV1QuadTargetTracker.h>
#endif
#if defined(MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF) || defined(MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY)
#include <MG_Util/Converters/MGToGL/RenderStateEnumConverter.h>
#endif
#if defined(MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX) || defined(MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF) || \
    defined(MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY) || \
    defined(MOBILEPZ_BUG002_WFX_BLEND_FIX) || defined(MOBILEPZ_BUG003_HORSE_BOUNDED_QUADS)
#include <atomic>
#endif

#include <cmath>
#include <cstdint>
#include <limits>

namespace MobileGL::MG_Impl::GLImpl::PZCompat {
    namespace {
        using Matrix = Array<GLfloat, 16>;

        struct CompatVertex {
            GLfloat position[4];
            GLfloat color[4];
            GLfloat texCoord[2];
            GLfloat normal[3];
        };

        struct ClientVertexArray {
            Bool enabled = false;
            GLint size = 4;
            GLenum type = GL_FLOAT;
            GLsizei stride = 0;
            const void* pointer = nullptr;
            GLuint arrayBuffer = 0;
        };

        struct LegacyValues {
            GLenum matrixMode = GL_MODELVIEW;
            GLfloat color[4] = {1.f, 1.f, 1.f, 1.f};
            GLfloat normal[3] = {0.f, 0.f, 1.f};
            GLfloat texCoord[2] = {0.f, 0.f};
            GLenum alphaFunc = GL_ALWAYS;
            GLfloat alphaRef = 0.f;
            Bool alphaTest = false;
            Bool lighting = false;
            Bool colorMaterial = false;
            Bool normalize = false;
            Array<Bool, 8> lights{};
            Array<Bool, 32> texture2D{};
            Array<GLenum, 32> texEnvMode{};
            GLenum colorMaterialFace = GL_FRONT_AND_BACK;
            GLenum colorMaterialMode = GL_AMBIENT_AND_DIFFUSE;

            LegacyValues() {
                texEnvMode.fill(GL_MODULATE);
            }
        };

        struct AttribSnapshot {
            GLbitfield mask = 0;
            LegacyValues values;
            Bool hasCoreState = false;
            RenderStateParameters coreState;
            Array<Bool, static_cast<SizeT>(CapabilityInput::CapabilityInputCount)> capabilities{};
            Int activeTextureUnit = 0;
            Int maxTouchedTextureUnit = -1;
            Vector<MG_State::GLState::TextureUnit> textureUnits;
        };

        struct ClientSnapshot {
            GLbitfield mask = 0;
            ClientVertexArray vertex;
            Bool hasCoreState = false;
            GLint vertexArray = 0;
            UniquePtr<MG_State::GLState::VertexArrayObject> vertexArrayState;
            GLint arrayBuffer = 0;
            GLint elementArrayBuffer = 0;
            GLint pixelPackBuffer = 0;
            GLint pixelUnpackBuffer = 0;
            PixelStoreParameters pack;
            PixelStoreParameters unpack;
        };

        struct Resources {
            GLuint program = 0;
            GLuint vao = 0;
            GLuint vbo = 0;
            GLint mvpLocation = -1;
            GLint textureMatrixLocation = -1;
            GLint textureEnabledLocation = -1;
            GLint textureLocation = -1;
            GLint alphaEnabledLocation = -1;
            GLint alphaFuncLocation = -1;
            GLint alphaRefLocation = -1;
            GLint texEnvModeLocation = -1;
            Bool failed = false;
        };

        struct State {
            const void* ownerContext = nullptr;
            LegacyValues values;
            Vector<Matrix> modelView{Matrix{}};
            Vector<Matrix> projection{Matrix{}};
            Vector<Matrix> texture{Matrix{}};
            Vector<AttribSnapshot> attribStack;
            Vector<ClientSnapshot> clientAttribStack;
            ClientVertexArray clientVertex;
            Bool inBegin = false;
            GLenum beginMode = GL_TRIANGLES;
            Vector<CompatVertex> vertices;
            Vector<CompatVertex> convertedVertices;
            Resources resources;
            Uint64 clientAttribPushHits = 0;
            Uint64 clientAttribPopHits = 0;
            Uint64 clientAttribRestoreHits = 0;

            State() {
                const Matrix identity = {
                    1.f, 0.f, 0.f, 0.f,
                    0.f, 1.f, 0.f, 0.f,
                    0.f, 0.f, 1.f, 0.f,
                    0.f, 0.f, 0.f, 1.f,
                };
                modelView[0] = identity;
                projection[0] = identity;
                texture[0] = identity;
                vertices.reserve(4096);
                convertedVertices.reserve(4096);
            }
        };

        State& CurrentState() {
            static thread_local State state;
            const void* context = MG_State::pGLContext.get();
            if (state.ownerContext != context) {
                state = State{};
                state.ownerContext = context;
            }
            return state;
        }

        void RecordError(ErrorCode code, const char* functionName, const String& message) {
            if (!MG_State::pGLContext) {
                MGLOG_E("PZCompat %s: %s", functionName, message.c_str());
                return;
            }
            MG_State::pGLContext->RecordError(
                code, MakeUnique<GenericErrorInfo>("PZCompat", functionName, message));
        }

        Matrix Identity() {
            return {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f,
            };
        }

        Matrix Multiply(const Matrix& left, const Matrix& right) {
            Matrix result{};
            // OpenGL matrices are column-major.  Compatibility transforms
            // post-multiply the current matrix: C = left * right.
            for (Int column = 0; column < 4; ++column) {
                for (Int row = 0; row < 4; ++row) {
                    GLfloat value = 0.f;
                    for (Int k = 0; k < 4; ++k) {
                        value += left[k * 4 + row] * right[column * 4 + k];
                    }
                    result[column * 4 + row] = value;
                }
            }
            return result;
        }

        Vector<Matrix>* SelectedStack(State& state) {
            switch (state.values.matrixMode) {
            case GL_MODELVIEW: return &state.modelView;
            case GL_PROJECTION: return &state.projection;
            case GL_TEXTURE: return &state.texture;
            default: return nullptr;
            }
        }

        Uint ActiveTextureUnit() {
            if (!MG_State::pGLContext) return 0;
            const Int unit = MG_State::pGLContext->GetActiveTextureUnit();
            return static_cast<Uint>(std::clamp<Int>(unit, 0, 31));
        }

        Bool HasAttribGroup(GLbitfield mask, GLbitfield group) {
            return (mask & group) != 0;
        }

        Bool IsP15TraceMilestone(Uint64 hits) {
            return hits == 1 || hits == 1024 || hits == 65536;
        }

#ifdef MOBILEPZ_V1_CANDIDATE
        Uint PZV1CurrentFrontFbo() {
            if (!MG_State::pGLContext) return 0;
            const auto& slot =
                MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw);
            const auto& framebuffer = slot.GetBoundObject();
            return framebuffer ? framebuffer->GetExternalIndex() : 0;
        }

        Uint64 PZV1SmallRenderTargetOnUnit0() {
            if (!MG_State::pGLContext) return 0;
            const auto& texture = MG_State::pGLContext->GetTextureUnitObject(0)
                .GetBindingSlot(TextureTarget::Texture2D)
                .GetBoundObject();
            if (!texture || texture->GetTarget() != TextureTarget::Texture2D ||
                texture->GetFormat() != TextureInternalFormat::RGBA8) {
                return 0;
            }
            const auto size = texture->GetBaseSize();
            if (size.x() <= 0 || size.y() <= 0 || size.x() > 512 || size.y() > 512) {
                return 0;
            }
            return texture->GetLifetimeId();
        }

        Bool PZV1HasWorldMapStencilSignature() {
            if (!MG_State::pGLContext) return false;
            const auto& parameters =
                MG_State::pGLContext->GetRenderStateParameters();
            const auto& frontStencil = parameters.StencilStates[0];
            return ::MobilePZ::V1::IsWorldMapStencilSignature(
                MG_State::pGLContext->IsCapabilityEnabled(
                    CapabilityInput::StencilTest),
                MG_State::pGLContext->IsCapabilityEnabled(
                    CapabilityInput::DepthTest),
                frontStencil.Func == DepthTestFunc::Equal,
                frontStencil.Ref);
        }

        Bool PZV1HasVboRendererProgramInterface(
            MG_State::GLState::ProgramObject& program) {
            return ::MobilePZ::V1::HasVboRendererProgramInterface(
                program.GetAttributeLocation("aPosition") >= 0,
                program.GetAttributeLocation("aColor") >= 0,
                program.GetUniformLocation("ModelViewProjection") >= 0,
                program.GetUniformLocation("userDepth") >= 0);
        }

        Bool ShouldConvertPZV1WorldMapQuadBatch(GLenum mode, GLsizei count) {
            if (!MG_State::pGLContext ||
                !::MobilePZ::V1::IsQuadBatch(mode, count)) {
                return false;
            }
            const auto& program = MG_State::pGLContext->GetProgramForDraw();
            if (!program) return false;
            if (::MobilePZ::V1::ShouldConvertMapSdfQuadBatch(
                    program->GetPZV1MapProgramRole(), mode, count)) {
                return true;
            }
            return ::MobilePZ::V1::ShouldConvertWorldMapVboQuadBatch(
                mode, count, PZV1HasWorldMapStencilSignature(),
                PZV1HasVboRendererProgramInterface(*program));
        }

        SizeT PZV1IndexTypeSize(GLenum type) {
            switch (type) {
            case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
            case GL_UNSIGNED_SHORT: return sizeof(GLushort);
            case GL_UNSIGNED_INT: return sizeof(GLuint);
            default: return 0;
            }
        }

        Bool SubmitPZV1QuadBatchArrays(GLint first, GLsizei count) {
            if (first < 0 || count < 4) return false;
            const auto lastFirst = static_cast<std::int64_t>(first) + count - 4;
            if (lastFirst > std::numeric_limits<GLint>::max()) return false;
            for (GLsizei offset = 0; offset < count; offset += 4) {
                GLImpl::DrawArrays(GL_TRIANGLE_FAN, first + offset, 4);
            }
            return true;
        }

        Bool PZV1ValidateAndGetLastIndexPointer(
            GLsizei count, GLenum type, const void* indices,
            SizeT& indexSize, std::uintptr_t& base) {
            indexSize = PZV1IndexTypeSize(type);
            if (indexSize == 0 || count < 4) return false;
            base = reinterpret_cast<std::uintptr_t>(indices);
            const std::uint64_t lastOffset64 =
                static_cast<std::uint64_t>(count - 4) * indexSize;
            if (lastOffset64 > std::numeric_limits<std::uintptr_t>::max()) {
                return false;
            }
            const auto lastOffset = static_cast<std::uintptr_t>(lastOffset64);
            return base <= std::numeric_limits<std::uintptr_t>::max() - lastOffset;
        }

        Bool SubmitPZV1QuadBatchElements(GLsizei count, GLenum type,
                                         const void* indices) {
            SizeT indexSize = 0;
            std::uintptr_t base = 0;
            if (!PZV1ValidateAndGetLastIndexPointer(
                    count, type, indices, indexSize, base)) {
                return false;
            }
            for (GLsizei offset = 0; offset < count; offset += 4) {
                const auto quadIndices = reinterpret_cast<const void*>(
                    base + static_cast<std::uintptr_t>(offset) * indexSize);
                GLImpl::DrawElements(GL_TRIANGLE_FAN, 4, type, quadIndices);
            }
            return true;
        }

        Bool SubmitPZV1QuadBatchRangeElements(
            GLuint start, GLuint end, GLsizei count, GLenum type,
            const void* indices) {
            SizeT indexSize = 0;
            std::uintptr_t base = 0;
            if (!PZV1ValidateAndGetLastIndexPointer(
                    count, type, indices, indexSize, base)) {
                return false;
            }
            for (GLsizei offset = 0; offset < count; offset += 4) {
                const auto quadIndices = reinterpret_cast<const void*>(
                    base + static_cast<std::uintptr_t>(offset) * indexSize);
                GLImpl::DrawRangeElements(
                    GL_TRIANGLE_FAN, start, end, 4, type, quadIndices);
            }
            return true;
        }

#endif

#if defined(MOBILEPZ_BUG002_WFX_BLEND_FIX) || defined(MOBILEPZ_BUG003_HORSE_BOUNDED_QUADS)
        SharedPtr<MG_State::GLState::ProgramObject> CurrentBugFixProgram() {
            if (!MG_State::pGLContext) return nullptr;
            return MG_State::pGLContext->GetCurrentProgram();
        }

        Bool HasExactBugFixInterface(
            MG_State::GLState::ProgramObject& program,
            ::MobilePZ::Bug002Bug003::ProgramRole role) {
            if (program.GetActiveAttributesCount() != 3 ||
                program.GetUniformLocation("DIFFUSE") < 0 ||
                program.GetAttributeLocation("aColor") < 0) {
                return false;
            }
            if (role == ::MobilePZ::Bug002Bug003::ProgramRole::WeatherFx) {
                return program.GetAttributeLocation("aPos") >= 0 &&
                       program.GetAttributeLocation("aUV") >= 0;
            }
            if (role == ::MobilePZ::Bug002Bug003::ProgramRole::HorseQuad) {
                return program.GetAttributeLocation("aPosition") >= 0 &&
                       program.GetAttributeLocation("aUV1") >= 0;
            }
            return false;
        }

        Bool IsBugFixLogMilestone(Uint64 hits) {
            return hits == 1 || hits == 16 || hits == 1024 || hits == 65536;
        }
#endif

#ifdef MOBILEPZ_BUG003_HORSE_BOUNDED_QUADS
        Bool IsBoundedHorseQuad(GLenum mode, GLsizei count,
                                SharedPtr<MG_State::GLState::ProgramObject>& program) {
            if (mode != GL_QUADS || count != 4) return false;
            program = CurrentBugFixProgram();
            if (!program || program->GetPZBug002Bug003Role() !=
                    ::MobilePZ::Bug002Bug003::ProgramRole::HorseQuad) {
                return false;
            }
            return HasExactBugFixInterface(
                *program, ::MobilePZ::Bug002Bug003::ProgramRole::HorseQuad);
        }

        void RecordBoundedHorseQuad(const char* route,
                                    const MG_State::GLState::ProgramObject& program,
                                    GLenum indexType) {
            static std::atomic<Uint64> hits{0};
            const Uint64 hit = hits.fetch_add(1, std::memory_order_relaxed) + 1;
            if (!IsBugFixLogMilestone(hit)) return;
            MGLOG_W("MGLPZ_BUG003_HORSE_QUAD hit=%llu route=%s program=%u lifetime=%llu "
                    "shader_fp=%016llx old_mode=0x%x new_mode=0x%x count=4 index_type=0x%x",
                    static_cast<unsigned long long>(hit), route,
                    program.GetExternalIndex(),
                    static_cast<unsigned long long>(program.GetLifetimeId()),
                    static_cast<unsigned long long>(program.GetPZBug002Bug003Fingerprint()),
                    GL_QUADS, GL_TRIANGLE_FAN, indexType);
        }
#endif

#ifdef MOBILEPZ_BUG002_WFX_BLEND_FIX
        Bool SubmitWeatherFxBlendFix(GLuint start, GLuint end, GLsizei count,
                                     GLenum type, const void* indices) {
            if (count != 6 || type != GL_UNSIGNED_SHORT || !MG_State::pGLContext ||
                PZV1CurrentFrontFbo() != 0) {
                return false;
            }
            auto program = CurrentBugFixProgram();
            if (!program || program->GetPZBug002Bug003Role() !=
                    ::MobilePZ::Bug002Bug003::ProgramRole::WeatherFx ||
                !HasExactBugFixInterface(
                    *program, ::MobilePZ::Bug002Bug003::ProgramRole::WeatherFx)) {
                return false;
            }

            const Bool blendWasEnabled = MG_State::pGLContext->IsCapabilityEnabled(
                CapabilityInput::Blend);
            if (!blendWasEnabled) GLImpl::Enable(GL_BLEND);
            GLImpl::DrawRangeElements(
                GL_TRIANGLES, start, end, count, type, indices);
            if (!blendWasEnabled) GLImpl::Disable(GL_BLEND);

            static std::atomic<Uint64> hits{0};
            const Uint64 hit = hits.fetch_add(1, std::memory_order_relaxed) + 1;
            if (IsBugFixLogMilestone(hit)) {
                MGLOG_W("MGLPZ_BUG002_WFX_BLEND hit=%llu program=%u lifetime=%llu "
                        "shader_fp=%016llx fbo=0 mode=0x%x count=6 type=0x%x "
                        "blend_before=%d blend_during=1 blend_restored=%d",
                        static_cast<unsigned long long>(hit),
                        program->GetExternalIndex(),
                        static_cast<unsigned long long>(program->GetLifetimeId()),
                        static_cast<unsigned long long>(program->GetPZBug002Bug003Fingerprint()),
                        GL_TRIANGLES, type, blendWasEnabled ? 1 : 0,
                        blendWasEnabled ? 1 : 0);
            }
            return true;
        }
#endif

#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        Uint PZF15CurrentFrontFbo() {
            if (!MG_State::pGLContext) return 0;
            const auto& slot = MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw);
            const auto& framebuffer = slot.GetBoundObject();
            return framebuffer ? framebuffer->GetExternalIndex() : 0;
        }

        Uint64 PZF15ThreadHash() {
            return static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }

        void PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent event,
                                   GLenum mode = 0, GLsizei count = 0,
                                   Bool requireActiveWindow = false) {
            if (requireActiveWindow && !::MobilePZ::PZF14::HasActiveWindows()) return;
            const Uint currentFrontFbo = PZF15CurrentFrontFbo();
            const auto observation = ::MobilePZ::PZF14::RecordSubmissionEvent(
                event, currentFrontFbo, mode, count, PZF15ThreadHash());
            if (!observation.shouldLog) return;
            MGLOG_I("MOBILEPZ_PZF15_SUBMISSION_EVENT sequence=%llu event=%s kind_hits=%llu "
                    "active_windows=%llu matched_window=%d window_generation=%u "
                    "window_lifetime=%llu window_front_texture=%u window_fbo=%u "
                    "window_client_sequence=%llu current_front_fbo=%u mode=0x%x count=%d "
                    "thread_hash=%llu rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
                    static_cast<unsigned long long>(observation.sequence),
                    ::MobilePZ::PZF14::SubmissionEventName(event),
                    static_cast<unsigned long long>(observation.kindHits),
                    static_cast<unsigned long long>(observation.activeWindows),
                    observation.matchedExpectedFbo ? 1 : 0,
                    observation.window.generation,
                    static_cast<unsigned long long>(observation.window.lifetime),
                    observation.window.frontTexture, observation.window.frontFbo,
                    static_cast<unsigned long long>(observation.window.clientSequence),
                    currentFrontFbo, mode, count,
                    static_cast<unsigned long long>(PZF15ThreadHash()));
        }
#endif

        Uint32 EnabledAttributeMask(const MG_State::GLState::VertexArrayObject& vao) {
            Uint32 mask = 0;
            const auto& attributes = vao.GetAllAttributes();
            for (Uint index = 0; index < attributes.size() && index < 32; ++index) {
                if (attributes[index].Enabled) mask |= Uint32{1} << index;
            }
            return mask;
        }

        Uint32 DifferentAttributeMask(const MG_State::GLState::VertexArrayObject& active,
                                      const MG_State::GLState::VertexArrayObject& saved) {
            Uint32 mask = 0;
            const auto& activeAttributes = active.GetAllAttributes();
            const auto& savedAttributes = saved.GetAllAttributes();
            for (Uint index = 0; index < activeAttributes.size() && index < 32; ++index) {
                const auto& a = activeAttributes[index];
                const auto& s = savedAttributes[index];
                if (a.Enabled != s.Enabled || a.Size != s.Size || a.Type != s.Type ||
                    a.Normalized != s.Normalized || a.Stride != s.Stride ||
                    a.Offset != s.Offset || a.IsInteger != s.IsInteger ||
                    a.IsLong != s.IsLong || a.IsBgra != s.IsBgra ||
                    a.Divisor != s.Divisor || a.Buffer != s.Buffer ||
                    a.LegacyStride != s.LegacyStride ||
                    a.LegacyPointer != s.LegacyPointer) {
                    mask |= Uint32{1} << index;
                }
            }
            return mask;
        }

#ifdef MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX
        enum class PZF16Quad4Route : Uint {
            ArraysActiveProgram,
            ArraysLegacyClient,
            ArraysNoSafeRoute,
            ElementsActiveProgram,
            ElementsLegacyClient,
            ElementsNoSafeRoute,
            RangeElementsActiveProgram,
            RangeElementsLegacyClient,
            RangeElementsNoSafeRoute,
            Count,
        };

        const char* PZF16Quad4RouteName(PZF16Quad4Route route) {
            switch (route) {
            case PZF16Quad4Route::ArraysActiveProgram: return "DRAW_ARRAYS_ACTIVE_PROGRAM";
            case PZF16Quad4Route::ArraysLegacyClient: return "DRAW_ARRAYS_LEGACY_CLIENT";
            case PZF16Quad4Route::ArraysNoSafeRoute: return "DRAW_ARRAYS_NO_SAFE_ROUTE";
            case PZF16Quad4Route::ElementsActiveProgram: return "DRAW_ELEMENTS_ACTIVE_PROGRAM";
            case PZF16Quad4Route::ElementsLegacyClient: return "DRAW_ELEMENTS_LEGACY_CLIENT";
            case PZF16Quad4Route::ElementsNoSafeRoute: return "DRAW_ELEMENTS_NO_SAFE_ROUTE";
            case PZF16Quad4Route::RangeElementsActiveProgram: return "DRAW_RANGE_ELEMENTS_ACTIVE_PROGRAM";
            case PZF16Quad4Route::RangeElementsLegacyClient: return "DRAW_RANGE_ELEMENTS_LEGACY_CLIENT";
            case PZF16Quad4Route::RangeElementsNoSafeRoute: return "DRAW_RANGE_ELEMENTS_NO_SAFE_ROUTE";
            case PZF16Quad4Route::Count: break;
            }
            return "UNKNOWN";
        }

        Bool IsPZF16Quad4(GLenum mode, GLsizei count) {
            return mode == GL_QUADS && count == 4;
        }

        Bool IsPZF16LogMilestone(Uint64 hits) {
            return hits == 1 || hits == 16 || hits == 1024 || hits == 65536;
        }

        void PZF16RecordQuad4Route(PZF16Quad4Route route, GLint first, GLsizei count,
                                   GLenum indexType, Bool legacyVertexEnabled) {
            static std::atomic<Uint64> routeHits[static_cast<SizeT>(PZF16Quad4Route::Count)]{};
            const SizeT routeIndex = static_cast<SizeT>(route);
            const Uint64 hits = routeHits[routeIndex].fetch_add(1, std::memory_order_relaxed) + 1;
            if (!IsPZF16LogMilestone(hits)) return;

            Uint currentFbo = 0;
            Uint currentProgram = 0;
            Uint currentVao = 0;
            Uint32 enabledAttributeMask = 0;
            if (MG_State::pGLContext) {
                currentFbo = PZF15CurrentFrontFbo();
                const auto& program = MG_State::pGLContext->GetCurrentProgram();
                if (program) currentProgram = program->GetExternalIndex();
                const auto& vao = MG_State::pGLContext->GetBoundVertexArray();
                if (vao) {
                    currentVao = vao->GetExternalIndex();
                    enabledAttributeMask = EnabledAttributeMask(*vao);
                }
            }

            MGLOG_I("MOBILEPZ_PZF16_QUAD4_ROUTE route=%s hits=%llu active_window=%d "
                    "current_front_fbo=%u current_program=%u current_vao=%u enabled_attr_mask=0x%x "
                    "legacy_vertex_enabled=%d original_mode=0x%x translated_mode=0x%x first=%d "
                    "count=%d index_type=0x%x semantic=single_quad_equivalent state_mutation=none "
                    "additional_gl_calls=none error_drain=none",
                    PZF16Quad4RouteName(route), static_cast<unsigned long long>(hits),
                    ::MobilePZ::PZF14::HasActiveWindows() ? 1 : 0, currentFbo,
                    currentProgram, currentVao, enabledAttributeMask,
                    legacyVertexEnabled ? 1 : 0, GL_QUADS, GL_TRIANGLE_FAN,
                    first, count, indexType);
        }
#endif

        AttribSnapshot CaptureAttribSnapshot(GLbitfield mask, const LegacyValues& values) {
            AttribSnapshot snapshot;
            snapshot.mask = mask;
            snapshot.values = values;
            if (!MG_State::pGLContext) return snapshot;

            auto& context = *MG_State::pGLContext;
            snapshot.hasCoreState = true;
            snapshot.coreState = context.GetRenderStateParameters();
            for (SizeT i = 0; i < snapshot.capabilities.size(); ++i) {
                snapshot.capabilities[i] =
                    context.IsCapabilityEnabled(static_cast<CapabilityInput>(i));
            }

            if (HasAttribGroup(mask, GL_TEXTURE_BIT)) {
                snapshot.activeTextureUnit = context.GetActiveTextureUnit();
                snapshot.maxTouchedTextureUnit = context.GetMaxTouchedTextureUnit();
                snapshot.textureUnits.reserve(
                    static_cast<SizeT>(std::max(snapshot.maxTouchedTextureUnit + 1, 0)));
                for (Int unit = 0; unit <= snapshot.maxTouchedTextureUnit; ++unit) {
                    snapshot.textureUnits.push_back(context.GetTextureUnitObject(unit));
                }
            }
            return snapshot;
        }

        void RestoreLegacyValues(LegacyValues& destination, const AttribSnapshot& snapshot) {
            const LegacyValues& source = snapshot.values;
            const GLbitfield mask = snapshot.mask;

            if (HasAttribGroup(mask, GL_CURRENT_BIT)) {
                std::copy_n(source.color, 4, destination.color);
                std::copy_n(source.normal, 3, destination.normal);
                std::copy_n(source.texCoord, 2, destination.texCoord);
            }
            if (HasAttribGroup(mask, GL_COLOR_BUFFER_BIT) || HasAttribGroup(mask, GL_ENABLE_BIT)) {
                destination.alphaFunc = source.alphaFunc;
                destination.alphaRef = source.alphaRef;
                destination.alphaTest = source.alphaTest;
            }
            if (HasAttribGroup(mask, GL_LIGHTING_BIT) || HasAttribGroup(mask, GL_ENABLE_BIT)) {
                destination.lighting = source.lighting;
                destination.colorMaterial = source.colorMaterial;
                destination.normalize = source.normalize;
                destination.lights = source.lights;
                destination.colorMaterialFace = source.colorMaterialFace;
                destination.colorMaterialMode = source.colorMaterialMode;
            }
            if (HasAttribGroup(mask, GL_TEXTURE_BIT) || HasAttribGroup(mask, GL_ENABLE_BIT)) {
                destination.texture2D = source.texture2D;
                destination.texEnvMode = source.texEnvMode;
            }
        }

        void RestoreCapability(const AttribSnapshot& snapshot, CapabilityInput capability) {
            MG_State::pGLContext->SetCapability(
                capability, snapshot.capabilities[static_cast<SizeT>(capability)]);
        }

        void RestoreTextureUnits(const AttribSnapshot& snapshot) {
            if (!HasAttribGroup(snapshot.mask, GL_TEXTURE_BIT) || !MG_State::pGLContext) return;

            auto& context = *MG_State::pGLContext;
            const Int currentMax = context.GetMaxTouchedTextureUnit();
            const Int restoreMax = std::max(currentMax, snapshot.maxTouchedTextureUnit);
            for (Int unit = 0; unit <= restoreMax; ++unit) {
                if (unit <= snapshot.maxTouchedTextureUnit) {
                    context.GetTextureUnitObject(unit) =
                        snapshot.textureUnits[static_cast<SizeT>(unit)];
                    continue;
                }

                // A unit first touched inside the pushed scope was provably at its
                // default bindings at push time.  Reconstruct that state instead of
                // leaving a temporary texture visible after glPopAttrib.
                MG_State::GLState::TextureUnit defaultUnit;
                for (auto& slot : defaultUnit.GetAllBindingSlots()) {
                    slot.Bind(context.GetDefaultTextureObject(slot.GetTarget()));
                }
                context.GetTextureUnitObject(unit) = Move(defaultUnit);
            }
            context.SetActiveTextureUnit(snapshot.activeTextureUnit);
            context.BumpTextureBindGeneration();
        }

        void RestoreCoreState(const AttribSnapshot& snapshot) {
            if (!snapshot.hasCoreState || !MG_State::pGLContext) return;

            auto& context = *MG_State::pGLContext;
            const RenderStateParameters& state = snapshot.coreState;
            const GLbitfield mask = snapshot.mask;

            if (HasAttribGroup(mask, GL_ENABLE_BIT)) {
                for (SizeT i = 0; i < snapshot.capabilities.size(); ++i) {
                    RestoreCapability(snapshot, static_cast<CapabilityInput>(i));
                }
                for (Uint i = 0; i < state.BlendStates.size(); ++i) {
                    context.SetCapabilityIndexed(CapabilityInput::Blend, i,
                                                 state.BlendStates[i].Enabled);
                }
            }

            if (HasAttribGroup(mask, GL_POINT_BIT)) {
                context.SetPointSize(state.PointSize);
                context.SetPointFadeThresholdSize(state.PointFadeThresholdSize);
                context.SetPointSpriteCoordOrigin(state.PointSpriteCoordOrigin);
                RestoreCapability(snapshot, CapabilityInput::ProgramPointSize);
            }
            if (HasAttribGroup(mask, GL_LINE_BIT)) {
                context.SetLineWidth(state.LineWidth);
                RestoreCapability(snapshot, CapabilityInput::LineSmooth);
            }
            if (HasAttribGroup(mask, GL_POLYGON_BIT)) {
                context.SetCullFaceMode(state.CullFaceModeSetting);
                context.SetFrontFaceMode(state.FrontFaceModeSetting);
                context.SetPolygonMode(state.PolygonModeFront, state.PolygonModeBack);
                context.SetPolygonOffset(state.PolygonOffsetFactor, state.PolygonOffsetUnits);
                RestoreCapability(snapshot, CapabilityInput::CullFace);
                RestoreCapability(snapshot, CapabilityInput::PolygonOffsetFill);
                RestoreCapability(snapshot, CapabilityInput::PolygonOffsetLine);
                RestoreCapability(snapshot, CapabilityInput::PolygonOffsetPoint);
                RestoreCapability(snapshot, CapabilityInput::PolygonSmooth);
            }
            if (HasAttribGroup(mask, GL_DEPTH_BUFFER_BIT)) {
                context.SetDepthFunc(state.DepthFunc);
                context.SetDepthMask(state.DepthMask);
                context.SetClearDepth(state.ClearDepth);
                RestoreCapability(snapshot, CapabilityInput::DepthTest);
            }
            if (HasAttribGroup(mask, GL_STENCIL_BUFFER_BIT)) {
                for (StencilFace face : {StencilFace::Front, StencilFace::Back}) {
                    const auto& saved =
                        state.StencilStates[static_cast<SizeT>(face)];
                    context.SetStencilFunc(face, saved.Func, saved.Ref, saved.ValueMask);
                    context.SetStencilMask(face, saved.WriteMask);
                    context.SetStencilOp(face, saved.FailOp, saved.PassDepthFailOp,
                                         saved.PassDepthPassOp);
                }
                context.SetClearStencil(static_cast<Int>(state.ClearStencil));
                RestoreCapability(snapshot, CapabilityInput::StencilTest);
            }
            if (HasAttribGroup(mask, GL_VIEWPORT_BIT)) {
                context.SetViewport(state.Viewport);
                context.SetDepthRange(state.DepthRange);
            }
            if (HasAttribGroup(mask, GL_COLOR_BUFFER_BIT)) {
                for (Uint i = 0; i < state.BlendStates.size(); ++i) {
                    const auto& blend = state.BlendStates[i];
                    context.SetCapabilityIndexed(CapabilityInput::Blend, i, blend.Enabled);
                    context.SetBlendFuncIndexed(i, blend.SrcFactorRGB, blend.DstFactorRGB,
                                                blend.SrcFactorAlpha, blend.DstFactorAlpha);
                    context.SetBlendEquationIndexed(i, blend.ColorEquation, blend.AlphaEquation);
                    context.SetColorMaskIndexed(i, state.ColorMasks[i]);
                }
                context.SetLogicOp(state.LogicOp);
                context.SetClearColor(state.ClearColor);
                context.SetBlendColor(state.BlendColor);
                RestoreCapability(snapshot, CapabilityInput::ColorLogicOp);
                RestoreCapability(snapshot, CapabilityInput::Dither);
            }
            if (HasAttribGroup(mask, GL_HINT_BIT)) {
                context.SetHint(GL_LINE_SMOOTH_HINT, state.LineSmoothHint);
                context.SetHint(GL_POLYGON_SMOOTH_HINT, state.PolygonSmoothHint);
                context.SetHint(GL_TEXTURE_COMPRESSION_HINT, state.TextureCompressionHint);
                context.SetHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT,
                                state.FragmentShaderDerivativeHint);
            }
            if (HasAttribGroup(mask, GL_SCISSOR_BIT)) {
                context.SetScissorBox(state.ScissorBox);
                RestoreCapability(snapshot, CapabilityInput::ScissorTest);
            }
            if (HasAttribGroup(mask, GL_MULTISAMPLE_BIT)) {
                context.SetSampleCoverage(state.SampleCoverageValue,
                                          state.SampleCoverageInvert);
                context.SetSampleMaskValue(state.SampleMaskValue);
                RestoreCapability(snapshot, CapabilityInput::Multisample);
                RestoreCapability(snapshot, CapabilityInput::SampleAlphaToCoverage);
                RestoreCapability(snapshot, CapabilityInput::SampleAlphaToOne);
                RestoreCapability(snapshot, CapabilityInput::SampleCoverage);
                RestoreCapability(snapshot, CapabilityInput::SampleMask);
            }

            RestoreTextureUnits(snapshot);
        }

        ClientSnapshot CaptureClientSnapshot(GLbitfield mask,
                                             const ClientVertexArray& vertex) {
            ClientSnapshot snapshot;
            snapshot.mask = mask;
            snapshot.vertex = vertex;
            if (!MG_State::pGLContext) return snapshot;

            snapshot.hasCoreState = true;
            GLImpl::GetIntegerv(GL_VERTEX_ARRAY_BINDING, &snapshot.vertexArray);
            if (HasAttribGroup(mask, GL_CLIENT_VERTEX_ARRAY_BIT)) {
                const auto& boundVertexArray = MG_State::pGLContext->GetBoundVertexArray();
                if (boundVertexArray) {
                    snapshot.vertexArrayState =
                        MakeUnique<MG_State::GLState::VertexArrayObject>(*boundVertexArray);
                }
            }
            GLImpl::GetIntegerv(GL_ARRAY_BUFFER_BINDING, &snapshot.arrayBuffer);
            GLImpl::GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,
                                &snapshot.elementArrayBuffer);
            GLImpl::GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,
                                &snapshot.pixelPackBuffer);
            GLImpl::GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,
                                &snapshot.pixelUnpackBuffer);
            snapshot.pack = MG_State::pGLContext->GetPixelStoreParameters(false);
            snapshot.unpack = MG_State::pGLContext->GetPixelStoreParameters(true);
            return snapshot;
        }

        void RestorePixelStore(const PixelStoreParameters& parameters, Bool unpack) {
            auto& context = *MG_State::pGLContext;
            const PixelStoreParam alignment = unpack ? PixelStoreParam::UnpackAlignment
                                                     : PixelStoreParam::PackAlignment;
            const PixelStoreParam rowLength = unpack ? PixelStoreParam::UnpackRowLength
                                                     : PixelStoreParam::PackRowLength;
            const PixelStoreParam imageHeight = unpack ? PixelStoreParam::UnpackImageHeight
                                                       : PixelStoreParam::PackImageHeight;
            const PixelStoreParam skipRows = unpack ? PixelStoreParam::UnpackSkipRows
                                                    : PixelStoreParam::PackSkipRows;
            const PixelStoreParam skipPixels = unpack ? PixelStoreParam::UnpackSkipPixels
                                                      : PixelStoreParam::PackSkipPixels;
            const PixelStoreParam skipImages = unpack ? PixelStoreParam::UnpackSkipImages
                                                      : PixelStoreParam::PackSkipImages;
            const PixelStoreParam swapBytes = unpack ? PixelStoreParam::UnpackSwapBytes
                                                     : PixelStoreParam::PackSwapBytes;
            const PixelStoreParam lsbFirst = unpack ? PixelStoreParam::UnpackLSBFirst
                                                    : PixelStoreParam::PackLSBFirst;
            context.SetPixelStoreParam(alignment, parameters.Alignment);
            context.SetPixelStoreParam(rowLength, parameters.RowLength);
            context.SetPixelStoreParam(imageHeight, parameters.ImageHeight);
            context.SetPixelStoreParam(skipRows, parameters.SkipRows);
            context.SetPixelStoreParam(skipPixels, parameters.SkipPixels);
            context.SetPixelStoreParam(skipImages, parameters.SkipImages);
            context.SetPixelStoreParam(swapBytes, parameters.SwapBytes ? 1 : 0);
            context.SetPixelStoreParam(lsbFirst, parameters.LSBFirst ? 1 : 0);
        }

        void RestoreClientSnapshot(State& state, const ClientSnapshot& snapshot) {
            if (HasAttribGroup(snapshot.mask, GL_CLIENT_VERTEX_ARRAY_BIT)) {
                state.clientVertex = snapshot.vertex;
            }
            if (!snapshot.hasCoreState || !MG_State::pGLContext) return;

            if (HasAttribGroup(snapshot.mask, GL_CLIENT_VERTEX_ARRAY_BIT)) {
                GLImpl::BindVertexArray(static_cast<GLuint>(snapshot.vertexArray));
                const auto& boundVertexArray = MG_State::pGLContext->GetBoundVertexArray();
                if (boundVertexArray && snapshot.vertexArrayState) {
                    const Uint32 activeEnabled = EnabledAttributeMask(*boundVertexArray);
                    const Uint32 savedEnabled = EnabledAttributeMask(*snapshot.vertexArrayState);
                    const Uint32 changedMask =
                        DifferentAttributeMask(*boundVertexArray, *snapshot.vertexArrayState);
                    const Bool restored = boundVertexArray->RestoreClientAttribStateFrom(
                        *snapshot.vertexArrayState);
                    ++state.clientAttribPopHits;
                    if (restored) {
                        ++state.clientAttribRestoreHits;
                        if (IsP15TraceMilestone(state.clientAttribRestoreHits)) {
                            MGLOG_I(
                                "PZCOMPAT_P15_CLIENT_ATTRIB_RESTORE vao=%u "
                                "changed_attr_mask=0x%x saved_enabled_mask=0x%x "
                                "active_at_pop_enabled_mask=0x%x generic_vertex_state=1 "
                                "semantic_applied=1 hit=%llu",
                                static_cast<GLuint>(snapshot.vertexArray), changedMask,
                                savedEnabled, activeEnabled,
                                static_cast<unsigned long long>(state.clientAttribRestoreHits));
                        }
                    } else if (IsP15TraceMilestone(state.clientAttribPopHits)) {
                        MGLOG_I(
                            "PZCOMPAT_P15_CLIENT_ATTRIB_POP vao=%u changed_attr_mask=0 "
                            "semantic_applied=0 hit=%llu",
                            static_cast<GLuint>(snapshot.vertexArray),
                            static_cast<unsigned long long>(state.clientAttribPopHits));
                    }
                }
                GLImpl::BindBuffer(GL_ARRAY_BUFFER,
                                   static_cast<GLuint>(snapshot.arrayBuffer));
                GLImpl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                   static_cast<GLuint>(snapshot.elementArrayBuffer));
            }
            if (HasAttribGroup(snapshot.mask, GL_CLIENT_PIXEL_STORE_BIT)) {
                GLImpl::BindBuffer(GL_PIXEL_PACK_BUFFER,
                                   static_cast<GLuint>(snapshot.pixelPackBuffer));
                GLImpl::BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                                   static_cast<GLuint>(snapshot.pixelUnpackBuffer));
                RestorePixelStore(snapshot.pack, false);
                RestorePixelStore(snapshot.unpack, true);
            }
        }

        Bool IsCompareFunc(GLenum function) {
            switch (function) {
            case GL_NEVER:
            case GL_LESS:
            case GL_EQUAL:
            case GL_LEQUAL:
            case GL_GREATER:
            case GL_NOTEQUAL:
            case GL_GEQUAL:
            case GL_ALWAYS:
                return true;
            default:
                return false;
            }
        }

        Bool CompileShaderChecked(GLenum type, const char* source, GLuint& shader) {
            shader = GLImpl::CreateShader(type);
            if (shader == 0) return false;
            const GLchar* sources[] = {source};
            GLImpl::ShaderSource(shader, 1, sources, nullptr);
            GLImpl::CompileShader(shader);
            GLint status = GL_FALSE;
            GLImpl::GetShaderiv(shader, GL_COMPILE_STATUS, &status);
            if (status == GL_TRUE) return true;

            GLint length = 0;
            GLImpl::GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            Vector<GLchar> log(static_cast<SizeT>(std::max(length, 1)));
            GLImpl::GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
            MGLOG_E("PZCompat fallback shader compile failed: %s", log.data());
            GLImpl::DeleteShader(shader);
            shader = 0;
            return false;
        }

        Bool EnsureResources(State& state) {
            Resources& resources = state.resources;
            if (resources.program != 0) return true;
            if (resources.failed) return false;

            static constexpr const char* vertexSource = R"GLSL(
#version 330 core
layout(location = 0) in vec4 pzPosition;
layout(location = 1) in vec4 pzColor;
layout(location = 2) in vec2 pzTexCoord;
layout(location = 3) in vec3 pzNormal;
uniform mat4 pzMVP;
uniform mat4 pzTextureMatrix;
out vec4 pzVColor;
out vec2 pzVTexCoord;
out vec3 pzVNormal;
void main() {
    gl_Position = pzMVP * pzPosition;
    pzVColor = pzColor;
    pzVTexCoord = (pzTextureMatrix * vec4(pzTexCoord, 0.0, 1.0)).xy;
    pzVNormal = pzNormal;
}
)GLSL";

            static constexpr const char* fragmentSource = R"GLSL(
#version 330 core
in vec4 pzVColor;
in vec2 pzVTexCoord;
in vec3 pzVNormal;
uniform sampler2D pzTexture;
uniform int pzTextureEnabled;
uniform int pzTexEnvMode;
uniform int pzAlphaEnabled;
uniform int pzAlphaFunc;
uniform float pzAlphaRef;
out vec4 pzFragColor;

bool pzAlphaPass(float value) {
    if (pzAlphaFunc == 512) return false;                 // GL_NEVER
    if (pzAlphaFunc == 513) return value < pzAlphaRef;   // GL_LESS
    if (pzAlphaFunc == 514) return value == pzAlphaRef;  // GL_EQUAL
    if (pzAlphaFunc == 515) return value <= pzAlphaRef;  // GL_LEQUAL
    if (pzAlphaFunc == 516) return value > pzAlphaRef;   // GL_GREATER
    if (pzAlphaFunc == 517) return value != pzAlphaRef;  // GL_NOTEQUAL
    if (pzAlphaFunc == 518) return value >= pzAlphaRef;  // GL_GEQUAL
    return true;                                         // GL_ALWAYS
}

void main() {
    vec4 color = pzVColor;
    if (pzTextureEnabled != 0) {
        vec4 sampleColor = texture(pzTexture, pzVTexCoord);
        if (pzTexEnvMode == 7681) {                      // GL_REPLACE
            color = sampleColor;
        } else if (pzTexEnvMode == 260) {                // GL_ADD
            color = vec4(color.rgb + sampleColor.rgb, color.a * sampleColor.a);
        } else if (pzTexEnvMode == 8449) {               // GL_DECAL
            color.rgb = mix(color.rgb, sampleColor.rgb, sampleColor.a);
        } else {
            color *= sampleColor;                        // GL_MODULATE / safe fallback
        }
    }
    if (pzAlphaEnabled != 0 && !pzAlphaPass(color.a)) discard;
    pzFragColor = color;
}
)GLSL";

            GLuint vertexShader = 0;
            GLuint fragmentShader = 0;
            if (!CompileShaderChecked(GL_VERTEX_SHADER, vertexSource, vertexShader) ||
                !CompileShaderChecked(GL_FRAGMENT_SHADER, fragmentSource, fragmentShader)) {
                if (vertexShader) GLImpl::DeleteShader(vertexShader);
                if (fragmentShader) GLImpl::DeleteShader(fragmentShader);
                resources.failed = true;
                return false;
            }

            resources.program = GLImpl::CreateProgram();
            GLImpl::AttachShader(resources.program, vertexShader);
            GLImpl::AttachShader(resources.program, fragmentShader);
            GLImpl::LinkProgram(resources.program);
            GLint linkStatus = GL_FALSE;
            GLImpl::GetProgramiv(resources.program, GL_LINK_STATUS, &linkStatus);
            GLImpl::DeleteShader(vertexShader);
            GLImpl::DeleteShader(fragmentShader);
            if (linkStatus != GL_TRUE) {
                GLint length = 0;
                GLImpl::GetProgramiv(resources.program, GL_INFO_LOG_LENGTH, &length);
                Vector<GLchar> log(static_cast<SizeT>(std::max(length, 1)));
                GLImpl::GetProgramInfoLog(resources.program, static_cast<GLsizei>(log.size()), nullptr, log.data());
                MGLOG_E("PZCompat fallback program link failed: %s", log.data());
                GLImpl::DeleteProgram(resources.program);
                resources.program = 0;
                resources.failed = true;
                return false;
            }

            GLImpl::GenVertexArrays(1, &resources.vao);
            GLImpl::GenBuffers(1, &resources.vbo);
            resources.mvpLocation = GLImpl::GetUniformLocation(resources.program, "pzMVP");
            resources.textureMatrixLocation = GLImpl::GetUniformLocation(resources.program, "pzTextureMatrix");
            resources.textureEnabledLocation = GLImpl::GetUniformLocation(resources.program, "pzTextureEnabled");
            resources.textureLocation = GLImpl::GetUniformLocation(resources.program, "pzTexture");
            resources.alphaEnabledLocation = GLImpl::GetUniformLocation(resources.program, "pzAlphaEnabled");
            resources.alphaFuncLocation = GLImpl::GetUniformLocation(resources.program, "pzAlphaFunc");
            resources.alphaRefLocation = GLImpl::GetUniformLocation(resources.program, "pzAlphaRef");
            resources.texEnvModeLocation = GLImpl::GetUniformLocation(resources.program, "pzTexEnvMode");
            MGLOG_I("PZCOMPAT_MARKER fallback-pipeline-ready program=%u vao=%u vbo=%u",
                    resources.program, resources.vao, resources.vbo);
            return true;
        }

        void UploadUniforms(State& state) {
            Resources& resources = state.resources;
            const Matrix mvp = Multiply(state.projection.back(), state.modelView.back());
            GLImpl::UniformMatrix4fv(resources.mvpLocation, 1, GL_FALSE, mvp.data());
            GLImpl::UniformMatrix4fv(resources.textureMatrixLocation, 1, GL_FALSE, state.texture.back().data());
            GLImpl::Uniform1i(resources.textureLocation, 0);
            GLImpl::Uniform1i(resources.textureEnabledLocation, state.values.texture2D[0] ? 1 : 0);
            GLImpl::Uniform1i(resources.alphaEnabledLocation, state.values.alphaTest ? 1 : 0);
            GLImpl::Uniform1i(resources.alphaFuncLocation, static_cast<GLint>(state.values.alphaFunc));
            GLImpl::Uniform1f(resources.alphaRefLocation, state.values.alphaRef);
            GLImpl::Uniform1i(resources.texEnvModeLocation, static_cast<GLint>(state.values.texEnvMode[0]));
        }

        struct SavedBindings {
            GLint program = 0;
            GLint vao = 0;
            GLint arrayBuffer = 0;
            GLint elementBuffer = 0;
            GLint activeTexture = GL_TEXTURE0;
        };

        SavedBindings SaveBindings() {
            SavedBindings saved;
            GLImpl::GetIntegerv(GL_CURRENT_PROGRAM, &saved.program);
            GLImpl::GetIntegerv(GL_VERTEX_ARRAY_BINDING, &saved.vao);
            GLImpl::GetIntegerv(GL_ARRAY_BUFFER_BINDING, &saved.arrayBuffer);
            GLImpl::GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &saved.elementBuffer);
            GLImpl::GetIntegerv(GL_ACTIVE_TEXTURE, &saved.activeTexture);
            return saved;
        }

        void RestoreBindings(const SavedBindings& saved) {
            GLImpl::BindVertexArray(static_cast<GLuint>(saved.vao));
            GLImpl::BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(saved.arrayBuffer));
            GLImpl::UseProgram(static_cast<GLuint>(saved.program));
            GLImpl::ActiveTexture(static_cast<GLenum>(saved.activeTexture));
        }

        GLenum ConvertImmediateVertices(State& state) {
            state.convertedVertices.clear();
            const auto& input = state.vertices;
            switch (state.beginMode) {
            case GL_QUADS:
                state.convertedVertices.reserve((input.size() / 4) * 6);
                for (SizeT i = 0; i + 3 < input.size(); i += 4) {
                    state.convertedVertices.push_back(input[i]);
                    state.convertedVertices.push_back(input[i + 1]);
                    state.convertedVertices.push_back(input[i + 2]);
                    state.convertedVertices.push_back(input[i]);
                    state.convertedVertices.push_back(input[i + 2]);
                    state.convertedVertices.push_back(input[i + 3]);
                }
                return GL_TRIANGLES;
            case GL_QUAD_STRIP:
                if (input.size() < 4) return GL_TRIANGLES;
                state.convertedVertices.reserve(((input.size() - 2) / 2) * 6);
                for (SizeT i = 0; i + 3 < input.size(); i += 2) {
                    state.convertedVertices.push_back(input[i]);
                    state.convertedVertices.push_back(input[i + 1]);
                    state.convertedVertices.push_back(input[i + 3]);
                    state.convertedVertices.push_back(input[i]);
                    state.convertedVertices.push_back(input[i + 3]);
                    state.convertedVertices.push_back(input[i + 2]);
                }
                return GL_TRIANGLES;
            case GL_POLYGON:
                if (input.size() < 3) return GL_TRIANGLES;
                state.convertedVertices.reserve((input.size() - 2) * 3);
                for (SizeT i = 1; i + 1 < input.size(); ++i) {
                    state.convertedVertices.push_back(input[0]);
                    state.convertedVertices.push_back(input[i]);
                    state.convertedVertices.push_back(input[i + 1]);
                }
                return GL_TRIANGLES;
            default:
                return state.beginMode;
            }
        }

        void DrawImmediate(State& state) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
            PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::ImmediateAttempt,
                                  state.beginMode, static_cast<GLsizei>(state.vertices.size()));
#endif
            if (state.vertices.empty()) return;
            if (!EnsureResources(state)) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
                PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::ImmediateResourceFailure,
                                      state.beginMode, static_cast<GLsizei>(state.vertices.size()));
#endif
                return;
            }
            const GLenum drawMode = ConvertImmediateVertices(state);
            const Vector<CompatVertex>* vertices = &state.vertices;
            if (!state.convertedVertices.empty() || state.beginMode == GL_QUADS ||
                state.beginMode == GL_QUAD_STRIP || state.beginMode == GL_POLYGON) {
                vertices = &state.convertedVertices;
            }
            if (vertices->empty()) return;

            const SavedBindings saved = SaveBindings();
            GLImpl::ActiveTexture(GL_TEXTURE0);
            GLImpl::UseProgram(state.resources.program);
            GLImpl::BindVertexArray(state.resources.vao);
            GLImpl::BindBuffer(GL_ARRAY_BUFFER, state.resources.vbo);
            GLImpl::BufferData(GL_ARRAY_BUFFER,
                               static_cast<GLsizeiptr>(vertices->size() * sizeof(CompatVertex)),
                               vertices->data(), GL_STREAM_DRAW);
            GLImpl::VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CompatVertex),
                                        reinterpret_cast<const void*>(offsetof(CompatVertex, position)));
            GLImpl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CompatVertex),
                                        reinterpret_cast<const void*>(offsetof(CompatVertex, color)));
            GLImpl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CompatVertex),
                                        reinterpret_cast<const void*>(offsetof(CompatVertex, texCoord)));
            GLImpl::VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(CompatVertex),
                                        reinterpret_cast<const void*>(offsetof(CompatVertex, normal)));
            for (GLuint index = 0; index < 4; ++index) GLImpl::EnableVertexAttribArray(index);
            UploadUniforms(state);
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
            PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::ImmediateDispatch,
                                  drawMode, static_cast<GLsizei>(vertices->size()));
#endif
            GLImpl::DrawArrays(drawMode, 0, static_cast<GLsizei>(vertices->size()));
            RestoreBindings(saved);
        }

        template <typename DrawFunction>
        Bool DrawClientArray(State& state, DrawFunction&& draw) {
            if (!state.clientVertex.enabled || !MG_State::pGLContext ||
                MG_State::pGLContext->GetCurrentProgram() != nullptr) {
                return false;
            }
            if (!EnsureResources(state)) return true;

            const SavedBindings saved = SaveBindings();
            GLImpl::ActiveTexture(GL_TEXTURE0);
            GLImpl::UseProgram(state.resources.program);
            GLImpl::BindVertexArray(state.resources.vao);
            GLImpl::BindBuffer(GL_ARRAY_BUFFER, state.clientVertex.arrayBuffer);
            GLImpl::VertexAttribPointer(0, state.clientVertex.size, state.clientVertex.type, GL_FALSE,
                                        state.clientVertex.stride, state.clientVertex.pointer);
            GLImpl::EnableVertexAttribArray(0);
            GLImpl::DisableVertexAttribArray(1);
            GLImpl::DisableVertexAttribArray(2);
            GLImpl::DisableVertexAttribArray(3);
            GLImpl::VertexAttrib4f(1, state.values.color[0], state.values.color[1],
                                   state.values.color[2], state.values.color[3]);
            GLImpl::VertexAttrib2f(2, state.values.texCoord[0], state.values.texCoord[1]);
            GLImpl::VertexAttrib3f(3, state.values.normal[0], state.values.normal[1], state.values.normal[2]);
            GLImpl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(saved.elementBuffer));
            UploadUniforms(state);
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
            PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::ClientArrayDispatch,
                                  0, 0, true);
#endif
            draw();
            RestoreBindings(saved);
            return true;
        }

#ifdef MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF
        void PZF23D3PrepareCustomProgramDraw(const char* route, GLenum mode, GLsizei count) {
            if (!MG_State::pGLContext) return;
            const auto& program = MG_State::pGLContext->GetCurrentProgram();
            if (!program) return;

            const GLint enabledLocation = program->GetUniformLocation("pzf23d3AlphaEnabled");
            const GLint functionLocation = program->GetUniformLocation("pzf23d3AlphaFunc");
            const GLint referenceLocation = program->GetUniformLocation("pzf23d3AlphaRef");
            if (enabledLocation < 0 || functionLocation < 0 || referenceLocation < 0) return;

            // The injected names are already unique to D3.  Requiring the original chunk
            // interface as well makes the draw gate independently fail closed if a future shader
            // happens to reuse one of those names.
            if (program->GetUniformLocation("chunkDepth") < 0 ||
                program->GetUniformLocation("DEPTH") < 0 ||
                program->GetUniformLocation("DIFFUSE") < 0 ||
                program->GetUniformLocation("useTexture") < 0) {
                return;
            }

            State& state = CurrentState();
            GLImpl::Uniform1i(enabledLocation, state.values.alphaTest ? 1 : 0);
            GLImpl::Uniform1i(functionLocation, static_cast<GLint>(state.values.alphaFunc));
            GLImpl::Uniform1f(referenceLocation, state.values.alphaRef);

            static std::atomic<Uint32> targetDrawSerial{0};
            const Uint32 serial = targetDrawSerial.fetch_add(1, std::memory_order_relaxed) + 1;
            constexpr Uint32 kMaxLoggedTargetDraws = 128;
            if (serial > kMaxLoggedTargetDraws) {
                if (serial == kMaxLoggedTargetDraws + 1) {
                    MGLOG_I("PZF23D3_TARGET_DRAW_LOG_CAP schema=1 logged=%u uniforms_continue=YES",
                            kMaxLoggedTargetDraws);
                }
                return;
            }

            const auto& parameters = MG_State::pGLContext->GetRenderStateParameters();
            const auto& blend = parameters.BlendStates[0];
            MGLOG_I("PZF23D3_TARGET_DRAW schema=1 serial=%u route=%s mode=0x%x count=%d "
                    "front_program=%u alpha_enabled=%d alpha_func=0x%x alpha_ref=%.9g "
                    "alpha_uniforms_uploaded=YES semantic_active=%d "
                    "blend_enabled=%d blend_src_rgb=0x%x blend_dst_rgb=0x%x "
                    "blend_src_alpha=0x%x blend_dst_alpha=0x%x "
                    "depth_test=%d depth_mask=%d depth_func=0x%x state_level=front",
                    serial, route, mode, count, program->GetExternalIndex(),
                    state.values.alphaTest ? 1 : 0, state.values.alphaFunc,
                    static_cast<double>(state.values.alphaRef),
                    state.values.alphaTest && state.values.alphaFunc != GL_ALWAYS ? 1 : 0,
                    blend.Enabled ? 1 : 0,
                    MG_Util::ConvertBlendFactorToGLEnum(blend.SrcFactorRGB),
                    MG_Util::ConvertBlendFactorToGLEnum(blend.DstFactorRGB),
                    MG_Util::ConvertBlendFactorToGLEnum(blend.SrcFactorAlpha),
                    MG_Util::ConvertBlendFactorToGLEnum(blend.DstFactorAlpha),
                    parameters.DepthTestEnabled ? 1 : 0, parameters.DepthMask ? 1 : 0,
                    MG_Util::ConvertDepthTestFuncToGLEnum(parameters.DepthFunc));
        }
#endif

#ifdef MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY
        void PZF23D4PrepareCustomProgramDraw(const char* route, GLenum mode, GLsizei count) {
            if (!MG_State::pGLContext) return;
            const auto& program = MG_State::pGLContext->GetCurrentProgram();
            if (!program) return;

            const GLint enabledLocation = program->GetUniformLocation("pzf23d4AlphaEnabled");
            const GLint functionLocation = program->GetUniformLocation("pzf23d4AlphaFunc");
            const GLint referenceLocation = program->GetUniformLocation("pzf23d4AlphaRef");
            if (enabledLocation < 0 || functionLocation < 0 || referenceLocation < 0) return;

            enum class Family : Uint32 {
                ChunkComposite = 0,
                DepthTileProducer = 1,
                SeamProducer = 2,
                Count = 3,
            };
            Family family = Family::Count;
#if MOBILEGL_LOG_ACTIVE_LEVEL <= MOBILEGL_LOG_LEVEL_INFO
            const char* familyName = "unknown";
#endif
            const Bool hasDiffuse = program->GetUniformLocation("DIFFUSE") >= 0;
            const Bool hasDepth = program->GetUniformLocation("DEPTH") >= 0;
            if (hasDiffuse && hasDepth && program->GetUniformLocation("chunkDepth") >= 0 &&
                program->GetUniformLocation("useTexture") >= 0) {
                family = Family::ChunkComposite;
#if MOBILEGL_LOG_ACTIVE_LEVEL <= MOBILEGL_LOG_LEVEL_INFO
                familyName = "chunk_composite";
#endif
            } else if (hasDiffuse && hasDepth && program->GetUniformLocation("MASK") >= 0 &&
                       program->GetUniformLocation("zDepthBlendZ") >= 0 &&
                       program->GetUniformLocation("zDepthBlendToZ") >= 0) {
                family = Family::SeamProducer;
#if MOBILEGL_LOG_ACTIVE_LEVEL <= MOBILEGL_LOG_LEVEL_INFO
                familyName = "seam_producer";
#endif
            } else if (hasDiffuse && hasDepth && program->GetUniformLocation("zDepthBlendZ") >= 0 &&
                       program->GetUniformLocation("zDepthBlendToZ") >= 0) {
                family = Family::DepthTileProducer;
#if MOBILEGL_LOG_ACTIVE_LEVEL <= MOBILEGL_LOG_LEVEL_INFO
                familyName = "depth_tile_producer";
#endif
            }
            if (family == Family::Count) return;

            State& state = CurrentState();
            GLImpl::Uniform1i(enabledLocation, state.values.alphaTest ? 1 : 0);
            GLImpl::Uniform1i(functionLocation, static_cast<GLint>(state.values.alphaFunc));
            GLImpl::Uniform1f(referenceLocation, state.values.alphaRef);

#if MOBILEGL_LOG_ACTIVE_LEVEL <= MOBILEGL_LOG_LEVEL_INFO
            static std::atomic<Uint32> familySerials[static_cast<SizeT>(Family::Count)]{};
            const SizeT familyIndex = static_cast<SizeT>(family);
            const Uint32 serial = familySerials[familyIndex].fetch_add(1, std::memory_order_relaxed) + 1;
            constexpr Uint32 kMaxLoggedDrawsPerFamily = 64;
            if (serial > kMaxLoggedDrawsPerFamily) {
                if (serial == kMaxLoggedDrawsPerFamily + 1) {
                    MGLOG_I("PZF23D4_TARGET_DRAW_LOG_CAP schema=1 family=%s logged=%u uniforms_continue=YES",
                            familyName, kMaxLoggedDrawsPerFamily);
                }
                return;
            }

            const auto& parameters = MG_State::pGLContext->GetRenderStateParameters();
            const auto& blend = parameters.BlendStates[0];
            MGLOG_I("PZF23D4_TARGET_DRAW schema=1 family=%s family_serial=%u route=%s mode=0x%x count=%d "
                    "front_program=%u alpha_enabled=%d alpha_func=0x%x alpha_ref=%.9g "
                    "uniforms_uploaded=YES semantic_active=%d blend_enabled=%d "
                    "depth_test=%d depth_mask=%d depth_func=0x%x state_level=front",
                    familyName, serial, route, mode, count, program->GetExternalIndex(),
                    state.values.alphaTest ? 1 : 0, state.values.alphaFunc,
                    static_cast<double>(state.values.alphaRef),
                    state.values.alphaTest && state.values.alphaFunc != GL_ALWAYS ? 1 : 0,
                    blend.Enabled ? 1 : 0, parameters.DepthTestEnabled ? 1 : 0,
                    parameters.DepthMask ? 1 : 0,
                    MG_Util::ConvertDepthTestFuncToGLEnum(parameters.DepthFunc));
#endif
        }
#endif
    }

    Bool SetLegacyCapability(GLenum cap, Bool enabled) {
        State& state = CurrentState();
        switch (cap) {
        case GL_ALPHA_TEST: state.values.alphaTest = enabled; return true;
        case GL_LIGHTING: state.values.lighting = enabled; return true;
        case GL_COLOR_MATERIAL: state.values.colorMaterial = enabled; return true;
        case GL_NORMALIZE: state.values.normalize = enabled; return true;
        case GL_TEXTURE_1D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
            // PZ only needs the enable/disable call to be accepted for these
            // legacy texture targets.  DirectGLES sampling remains explicit.
            return true;
        case GL_TEXTURE_2D:
            state.values.texture2D[ActiveTextureUnit()] = enabled;
            return true;
        default:
            if (cap >= GL_LIGHT0 && cap <= GL_LIGHT7) {
                state.values.lights[static_cast<SizeT>(cap - GL_LIGHT0)] = enabled;
                return true;
            }
            return false;
        }
    }

    Bool GetLegacyCapability(GLenum cap, GLboolean* enabled) {
        if (!enabled) return false;
        State& state = CurrentState();
        switch (cap) {
        case GL_ALPHA_TEST: *enabled = state.values.alphaTest ? GL_TRUE : GL_FALSE; return true;
        case GL_LIGHTING: *enabled = state.values.lighting ? GL_TRUE : GL_FALSE; return true;
        case GL_COLOR_MATERIAL: *enabled = state.values.colorMaterial ? GL_TRUE : GL_FALSE; return true;
        case GL_NORMALIZE: *enabled = state.values.normalize ? GL_TRUE : GL_FALSE; return true;
        case GL_TEXTURE_2D:
            *enabled = state.values.texture2D[ActiveTextureUnit()] ? GL_TRUE : GL_FALSE;
            return true;
        case GL_TEXTURE_1D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
            *enabled = GL_FALSE;
            return true;
        default:
            if (cap >= GL_LIGHT0 && cap <= GL_LIGHT7) {
                *enabled = state.values.lights[static_cast<SizeT>(cap - GL_LIGHT0)] ? GL_TRUE : GL_FALSE;
                return true;
            }
            return false;
        }
    }

    void AlphaFunc(GLenum function, GLclampf reference) {
        if (!IsCompareFunc(function)) {
            RecordError(ErrorCode::InvalidEnum, __func__, "unsupported alpha comparison function");
            return;
        }
        State& state = CurrentState();
        state.values.alphaFunc = function;
        state.values.alphaRef = std::clamp(reference, 0.f, 1.f);
    }

    void MatrixMode(GLenum mode) {
        if (mode != GL_MODELVIEW && mode != GL_PROJECTION && mode != GL_TEXTURE) {
            RecordError(ErrorCode::InvalidEnum, __func__, "mode must be MODELVIEW, PROJECTION, or TEXTURE");
            return;
        }
        CurrentState().values.matrixMode = mode;
    }

    void LoadIdentity() {
        State& state = CurrentState();
        if (auto* stack = SelectedStack(state)) stack->back() = Identity();
    }

    void LoadMatrixf(const GLfloat* matrix) {
        if (!matrix) {
            RecordError(ErrorCode::InvalidValue, __func__, "matrix pointer is null");
            return;
        }
        State& state = CurrentState();
        if (auto* stack = SelectedStack(state)) std::copy_n(matrix, 16, stack->back().begin());
    }

    void MultMatrixf(const GLfloat* matrix) {
        if (!matrix) {
            RecordError(ErrorCode::InvalidValue, __func__, "matrix pointer is null");
            return;
        }
        State& state = CurrentState();
        if (auto* stack = SelectedStack(state)) {
            Matrix right{};
            std::copy_n(matrix, 16, right.begin());
            stack->back() = Multiply(stack->back(), right);
        }
    }

    void PushMatrix() {
        State& state = CurrentState();
        auto* stack = SelectedStack(state);
        const SizeT limit = state.values.matrixMode == GL_MODELVIEW ? 32 : 2;
        if (!stack || stack->size() >= limit) {
            RecordError(ErrorCode::StackOverflow, __func__, "matrix stack limit reached");
            return;
        }
        stack->push_back(stack->back());
    }

    void PopMatrix() {
        State& state = CurrentState();
        auto* stack = SelectedStack(state);
        if (!stack || stack->size() <= 1) {
            RecordError(ErrorCode::StackUnderflow, __func__, "cannot pop the last matrix");
            return;
        }
        stack->pop_back();
    }

    void Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
               GLdouble nearValue, GLdouble farValue) {
        if (left == right || bottom == top || nearValue == farValue) {
            RecordError(ErrorCode::InvalidValue, __func__, "orthographic bounds have zero extent");
            return;
        }
        Matrix matrix = Identity();
        matrix[0] = static_cast<GLfloat>(2.0 / (right - left));
        matrix[5] = static_cast<GLfloat>(2.0 / (top - bottom));
        matrix[10] = static_cast<GLfloat>(-2.0 / (farValue - nearValue));
        matrix[12] = static_cast<GLfloat>(-(right + left) / (right - left));
        matrix[13] = static_cast<GLfloat>(-(top + bottom) / (top - bottom));
        matrix[14] = static_cast<GLfloat>(-(farValue + nearValue) / (farValue - nearValue));
        MultMatrixf(matrix.data());
    }

    void Rotate(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
        const GLdouble length = std::sqrt(x * x + y * y + z * z);
        if (length == 0.0) return;
        x /= length;
        y /= length;
        z /= length;
        const GLdouble radians = angle * 3.14159265358979323846 / 180.0;
        const GLdouble c = std::cos(radians);
        const GLdouble s = std::sin(radians);
        const GLdouble oneMinusC = 1.0 - c;
        Matrix matrix = {
            static_cast<GLfloat>(x*x*oneMinusC + c),
            static_cast<GLfloat>(y*x*oneMinusC + z*s),
            static_cast<GLfloat>(x*z*oneMinusC - y*s), 0.f,
            static_cast<GLfloat>(x*y*oneMinusC - z*s),
            static_cast<GLfloat>(y*y*oneMinusC + c),
            static_cast<GLfloat>(y*z*oneMinusC + x*s), 0.f,
            static_cast<GLfloat>(x*z*oneMinusC + y*s),
            static_cast<GLfloat>(y*z*oneMinusC - x*s),
            static_cast<GLfloat>(z*z*oneMinusC + c), 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        MultMatrixf(matrix.data());
    }

    void Scale(GLdouble x, GLdouble y, GLdouble z) {
        Matrix matrix = Identity();
        matrix[0] = static_cast<GLfloat>(x);
        matrix[5] = static_cast<GLfloat>(y);
        matrix[10] = static_cast<GLfloat>(z);
        MultMatrixf(matrix.data());
    }

    void Translate(GLdouble x, GLdouble y, GLdouble z) {
        Matrix matrix = Identity();
        matrix[12] = static_cast<GLfloat>(x);
        matrix[13] = static_cast<GLfloat>(y);
        matrix[14] = static_cast<GLfloat>(z);
        MultMatrixf(matrix.data());
    }

    void Color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::Color);
#endif
        State& state = CurrentState();
        state.values.color[0] = red;
        state.values.color[1] = green;
        state.values.color[2] = blue;
        state.values.color[3] = alpha;
    }

    void Normal(GLfloat x, GLfloat y, GLfloat z) {
        State& state = CurrentState();
        state.values.normal[0] = x;
        state.values.normal[1] = y;
        state.values.normal[2] = z;
    }

    void TexCoord(GLfloat s, GLfloat t) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::TexCoord);
#endif
        State& state = CurrentState();
        state.values.texCoord[0] = s;
        state.values.texCoord[1] = t;
    }

    void TexEnvi(GLenum target, GLenum pname, GLint param) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::TexEnv,
                              target, static_cast<GLsizei>(param));
#endif
        if (target != GL_TEXTURE_ENV || pname != GL_TEXTURE_ENV_MODE) {
            RecordError(ErrorCode::InvalidEnum, __func__, "PZCompat supports GL_TEXTURE_ENV_MODE only");
            return;
        }
        switch (param) {
        case GL_MODULATE:
        case GL_REPLACE:
        case GL_ADD:
        case GL_DECAL:
        case GL_COMBINE:
            CurrentState().values.texEnvMode[ActiveTextureUnit()] = static_cast<GLenum>(param);
            return;
        default:
            RecordError(ErrorCode::InvalidEnum, __func__, "unsupported texture environment mode");
        }
    }

    void ColorMaterial(GLenum face, GLenum mode) {
        if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) {
            RecordError(ErrorCode::InvalidEnum, __func__, "invalid material face");
            return;
        }
        State& state = CurrentState();
        state.values.colorMaterialFace = face;
        state.values.colorMaterialMode = mode;
    }

    void Lightf(GLenum light, GLenum pname, GLfloat param) {
        if (light < GL_LIGHT0 || light > GL_LIGHT7) {
            RecordError(ErrorCode::InvalidEnum, __func__, "invalid light index");
            return;
        }
        (void)pname;
        (void)param;
    }

    void Lightfv(GLenum light, GLenum pname, const GLfloat* params) {
        if (!params || light < GL_LIGHT0 || light > GL_LIGHT7) {
            RecordError(!params ? ErrorCode::InvalidValue : ErrorCode::InvalidEnum, __func__,
                        !params ? "params pointer is null" : "invalid light index");
            return;
        }
        (void)pname;
    }

    void Materialfv(GLenum face, GLenum pname, const GLfloat* params) {
        if (!params) {
            RecordError(ErrorCode::InvalidValue, __func__, "params pointer is null");
            return;
        }
        if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) {
            RecordError(ErrorCode::InvalidEnum, __func__, "invalid material face");
            return;
        }
        (void)pname;
    }

    void PushAttrib(GLbitfield mask) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::PushAttrib,
                              static_cast<GLenum>(mask));
#endif
        State& state = CurrentState();
        if (state.attribStack.size() >= 16) {
            RecordError(ErrorCode::StackOverflow, __func__, "attribute stack limit reached");
            return;
        }
        state.attribStack.push_back(CaptureAttribSnapshot(mask, state.values));
    }

    void PopAttrib() {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::PopAttrib);
#endif
        State& state = CurrentState();
        if (state.attribStack.empty()) {
            RecordError(ErrorCode::StackUnderflow, __func__, "attribute stack is empty");
            return;
        }
        const AttribSnapshot snapshot = Move(state.attribStack.back());
        state.attribStack.pop_back();
        RestoreLegacyValues(state.values, snapshot);
        RestoreCoreState(snapshot);
    }

    void PushClientAttrib(GLbitfield mask) {
        State& state = CurrentState();
        if (state.clientAttribStack.size() >= 16) {
            RecordError(ErrorCode::StackOverflow, __func__, "client attribute stack limit reached");
            return;
        }
        if (HasAttribGroup(mask, GL_CLIENT_VERTEX_ARRAY_BIT)) {
            ++state.clientAttribPushHits;
            if (IsP15TraceMilestone(state.clientAttribPushHits)) {
                GLint vertexArray = 0;
                GLImpl::GetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
                MGLOG_I(
                    "PZCOMPAT_P15_CLIENT_ATTRIB_CENSUS mask=0x%x vertex_array=%d "
                    "semantic_applied=0 hit=%llu",
                    mask, vertexArray,
                    static_cast<unsigned long long>(state.clientAttribPushHits));
            }
        }
        state.clientAttribStack.push_back(CaptureClientSnapshot(mask, state.clientVertex));
    }

    void PopClientAttrib() {
        State& state = CurrentState();
        if (state.clientAttribStack.empty()) {
            RecordError(ErrorCode::StackUnderflow, __func__, "client attribute stack is empty");
            return;
        }
        const ClientSnapshot snapshot = Move(state.clientAttribStack.back());
        state.clientAttribStack.pop_back();
        RestoreClientSnapshot(state, snapshot);
    }

    void Begin(GLenum mode) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::Begin, mode);
#endif
        State& state = CurrentState();
        if (state.inBegin) {
            RecordError(ErrorCode::InvalidOperation, __func__, "nested glBegin is not allowed");
            return;
        }
        switch (mode) {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_QUADS:
        case GL_QUAD_STRIP:
        case GL_POLYGON:
            break;
        default:
            RecordError(ErrorCode::InvalidEnum, __func__, "unsupported immediate primitive mode");
            return;
        }
        state.inBegin = true;
        state.beginMode = mode;
        state.vertices.clear();
    }

    void End() {
        State& state = CurrentState();
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::End, state.beginMode,
                              static_cast<GLsizei>(state.vertices.size()));
#endif
        if (!state.inBegin) {
            RecordError(ErrorCode::InvalidOperation, __func__, "glEnd without glBegin");
            return;
        }
        state.inBegin = false;
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        if (state.vertices.empty()) {
            PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::EmptyEnd,
                                  state.beginMode, 0);
        }
#endif
        DrawImmediate(state);
        state.vertices.clear();
        state.convertedVertices.clear();
    }

    void Vertex(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
        State& state = CurrentState();
        if (!state.inBegin) {
            RecordError(ErrorCode::InvalidOperation, __func__, "vertex outside glBegin/glEnd");
            return;
        }
        if (state.vertices.size() >= 1'048'576) {
            RecordError(ErrorCode::OutOfMemory, __func__, "immediate batch exceeds one million vertices");
            return;
        }
        CompatVertex vertex = {
            {x, y, z, w},
            {state.values.color[0], state.values.color[1], state.values.color[2], state.values.color[3]},
            {state.values.texCoord[0], state.values.texCoord[1]},
            {state.values.normal[0], state.values.normal[1], state.values.normal[2]},
        };
        state.vertices.push_back(vertex);
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::Vertex,
                              state.beginMode, 1);
#endif
    }

    void EnableClientState(GLenum cap) {
        if (cap == GL_VERTEX_ARRAY) {
            CurrentState().clientVertex.enabled = true;
            return;
        }
        if (cap == GL_COLOR_ARRAY || cap == GL_NORMAL_ARRAY || cap == GL_TEXTURE_COORD_ARRAY) {
            // PZ explicitly disables these around the sole legacy client-array
            // draw path.  Accepting them keeps the state machine deterministic;
            // PZCompat does not advertise arbitrary legacy arrays.
            return;
        }
        RecordError(ErrorCode::InvalidEnum, __func__, "unsupported legacy client array");
    }

    void DisableClientState(GLenum cap) {
        if (cap == GL_VERTEX_ARRAY) {
            CurrentState().clientVertex.enabled = false;
            return;
        }
        if (cap == GL_COLOR_ARRAY || cap == GL_NORMAL_ARRAY || cap == GL_TEXTURE_COORD_ARRAY) return;
        RecordError(ErrorCode::InvalidEnum, __func__, "unsupported legacy client array");
    }

    void VertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
        if (size < 2 || size > 4 || stride < 0) {
            RecordError(size < 2 || size > 4 ? ErrorCode::InvalidValue : ErrorCode::InvalidValue,
                        __func__, "invalid vertex pointer size or stride");
            return;
        }
        switch (type) {
        case GL_SHORT:
        case GL_INT:
        case GL_FLOAT:
        case GL_DOUBLE:
            break;
        default:
            RecordError(ErrorCode::InvalidEnum, __func__, "unsupported vertex pointer type");
            return;
        }
        GLint arrayBuffer = 0;
        GLImpl::GetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        State& state = CurrentState();
        state.clientVertex.size = size;
        state.clientVertex.type = type;
        state.clientVertex.stride = stride;
        state.clientVertex.pointer = pointer;
        state.clientVertex.arrayBuffer = static_cast<GLuint>(arrayBuffer);
    }

    Bool DrawArrays(GLenum mode, GLint first, GLsizei count) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::FrontendDraw,
                              mode, count, true);
#endif
#ifdef MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF
        PZF23D3PrepareCustomProgramDraw("DrawArrays", mode, count);
#endif
#ifdef MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY
        PZF23D4PrepareCustomProgramDraw("DrawArrays", mode, count);
#endif
        State& state = CurrentState();
#ifdef MOBILEPZ_BUG003_HORSE_BOUNDED_QUADS
        SharedPtr<MG_State::GLState::ProgramObject> horseProgram;
        if (IsBoundedHorseQuad(mode, count, horseProgram)) {
            RecordBoundedHorseQuad("DrawArrays", *horseProgram, 0);
            GLImpl::DrawArrays(GL_TRIANGLE_FAN, first, count);
            return true;
        }
#endif
#ifdef MOBILEPZ_V1_CANDIDATE
        if (ShouldConvertPZV1WorldMapQuadBatch(mode, count) &&
            SubmitPZV1QuadBatchArrays(first, count)) {
            return true;
        }
#endif
#ifdef MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX
        if (IsPZF16Quad4(mode, count) && MG_State::pGLContext) {
            if (MG_State::pGLContext->GetCurrentProgram() != nullptr) {
                PZF16RecordQuad4Route(PZF16Quad4Route::ArraysActiveProgram,
                                      first, count, 0, state.clientVertex.enabled);
                GLImpl::DrawArrays(GL_TRIANGLE_FAN, first, count);
                return true;
            }
            if (state.clientVertex.enabled) {
                PZF16RecordQuad4Route(PZF16Quad4Route::ArraysLegacyClient,
                                      first, count, 0, true);
                return DrawClientArray(state, [&] {
                    GLImpl::DrawArrays(GL_TRIANGLE_FAN, first, count);
                });
            }
            PZF16RecordQuad4Route(PZF16Quad4Route::ArraysNoSafeRoute,
                                  first, count, 0, false);
        }
#endif
        return DrawClientArray(state, [&] { GLImpl::DrawArrays(mode, first, count); });
    }

    Bool DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::FrontendDraw,
                              mode, count, true);
#endif
#ifdef MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF
        PZF23D3PrepareCustomProgramDraw("DrawElements", mode, count);
#endif
#ifdef MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY
        PZF23D4PrepareCustomProgramDraw("DrawElements", mode, count);
#endif
        State& state = CurrentState();
#ifdef MOBILEPZ_BUG003_HORSE_BOUNDED_QUADS
        SharedPtr<MG_State::GLState::ProgramObject> horseProgram;
        if (IsBoundedHorseQuad(mode, count, horseProgram)) {
            RecordBoundedHorseQuad("DrawElements", *horseProgram, type);
            GLImpl::DrawElements(GL_TRIANGLE_FAN, count, type, indices);
            return true;
        }
#endif
#ifdef MOBILEPZ_V1_CANDIDATE
        if (ShouldConvertPZV1WorldMapQuadBatch(mode, count) &&
            SubmitPZV1QuadBatchElements(count, type, indices)) {
            return true;
        }
#endif
#ifdef MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX
        if (IsPZF16Quad4(mode, count) && MG_State::pGLContext) {
            if (MG_State::pGLContext->GetCurrentProgram() != nullptr) {
                PZF16RecordQuad4Route(PZF16Quad4Route::ElementsActiveProgram,
                                      0, count, type, state.clientVertex.enabled);
                GLImpl::DrawElements(GL_TRIANGLE_FAN, count, type, indices);
                return true;
            }
            if (state.clientVertex.enabled) {
                PZF16RecordQuad4Route(PZF16Quad4Route::ElementsLegacyClient,
                                      0, count, type, true);
                return DrawClientArray(state, [&] {
                    GLImpl::DrawElements(GL_TRIANGLE_FAN, count, type, indices);
                });
            }
            PZF16RecordQuad4Route(PZF16Quad4Route::ElementsNoSafeRoute,
                                  0, count, type, false);
        }
#endif
        return DrawClientArray(state, [&] { GLImpl::DrawElements(mode, count, type, indices); });
    }

    Bool DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                           GLenum type, const void* indices) {
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
        PZF15RecordSubmission(::MobilePZ::PZF14::SubmissionEvent::FrontendDraw,
                              mode, count, true);
#endif
#ifdef MOBILEPZ_PZF23D3_CHUNK_ALPHA_TEST_PROOF
        PZF23D3PrepareCustomProgramDraw("DrawRangeElements", mode, count);
#endif
#ifdef MOBILEPZ_PZF23D4_CUSTOM_ALPHA_TEST_FAMILY
        PZF23D4PrepareCustomProgramDraw("DrawRangeElements", mode, count);
#endif
        State& state = CurrentState();
#ifdef MOBILEPZ_BUG002_WFX_BLEND_FIX
        if (mode == GL_TRIANGLES &&
            SubmitWeatherFxBlendFix(start, end, count, type, indices)) {
            return true;
        }
#endif
#ifdef MOBILEPZ_BUG003_HORSE_BOUNDED_QUADS
        SharedPtr<MG_State::GLState::ProgramObject> horseProgram;
        if (IsBoundedHorseQuad(mode, count, horseProgram)) {
            RecordBoundedHorseQuad("DrawRangeElements", *horseProgram, type);
            GLImpl::DrawRangeElements(
                GL_TRIANGLE_FAN, start, end, count, type, indices);
            return true;
        }
#endif
#ifdef MOBILEPZ_V1_CANDIDATE
        if (ShouldConvertPZV1WorldMapQuadBatch(mode, count) &&
            SubmitPZV1QuadBatchRangeElements(
                start, end, count, type, indices)) {
            return true;
        }

        if (mode == GL_QUADS && count == 4 && MG_State::pGLContext) {
            // R2 admits world quads by depth state and TextureCombiner producer
            // quads by their active target window.  R1 also dropped the later
            // depth-disabled compositor quad, which is not "preserved" by the
            // core path: GL_QUADS is rejected there.  Recover exactly one such
            // consumer only when unit 0 carries the same small NULL-allocated
            // RGBA8 texture published by a completed producer window.  Static
            // UI atlases never receive this lifetime handoff.
            const Bool depthEnabled =
                MG_State::pGLContext->IsCapabilityEnabled(CapabilityInput::DepthTest);
            const Uint currentFbo = PZV1CurrentFrontFbo();
            const Bool targetWindow = !depthEnabled &&
                ::MobilePZ::V1::IsActiveFramebuffer(currentFbo);
            const Bool activeProgram =
                MG_State::pGLContext->GetCurrentProgram() != nullptr;
            if (activeProgram || state.clientVertex.enabled) {
                Bool translate = depthEnabled || targetWindow;
                if (!translate) {
                    translate = ::MobilePZ::V1::TryConsumeCompositeTexture(
                        PZV1SmallRenderTargetOnUnit0());
                }
                if (translate && activeProgram) {
                    GLImpl::DrawRangeElements(
                        GL_TRIANGLE_FAN, start, end, count, type, indices);
                    if (targetWindow) {
                        ::MobilePZ::V1::MarkFramebufferQuadWritten(currentFbo);
                    }
                    return true;
                }
                if (translate && state.clientVertex.enabled) {
                    const Bool submitted = DrawClientArray(state, [&] {
                        GLImpl::DrawRangeElements(
                            GL_TRIANGLE_FAN, start, end, count, type, indices);
                    });
                    if (submitted && targetWindow) {
                        ::MobilePZ::V1::MarkFramebufferQuadWritten(currentFbo);
                    }
                    return submitted;
                }
            }
        }
#endif
#ifdef MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX
        if (IsPZF16Quad4(mode, count) && MG_State::pGLContext) {
            if (MG_State::pGLContext->GetCurrentProgram() != nullptr) {
                PZF16RecordQuad4Route(PZF16Quad4Route::RangeElementsActiveProgram,
                                      0, count, type, state.clientVertex.enabled);
                GLImpl::DrawRangeElements(GL_TRIANGLE_FAN, start, end, count, type, indices);
                return true;
            }
            if (state.clientVertex.enabled) {
                PZF16RecordQuad4Route(PZF16Quad4Route::RangeElementsLegacyClient,
                                      0, count, type, true);
                return DrawClientArray(state, [&] {
                    GLImpl::DrawRangeElements(GL_TRIANGLE_FAN, start, end, count, type, indices);
                });
            }
            PZF16RecordQuad4Route(PZF16Quad4Route::RangeElementsNoSafeRoute,
                                  0, count, type, false);
        }
#endif
        return DrawClientArray(state, [&] { GLImpl::DrawRangeElements(mode, start, end, count, type, indices); });
    }

    GLuint GenLists(GLsizei range) {
        if (range < 0) {
            RecordError(ErrorCode::InvalidValue, __func__, "range cannot be negative");
        }
        static std::once_flag once;
        std::call_once(once, [] {
            MGLOG_I("PZCOMPAT_MARKER display-lists-disabled PZ-font-cache-will-use-direct-rendering");
        });
        return 0;
    }

    GLboolean IsList(GLuint list) {
        (void)list;
        return GL_FALSE;
    }

    void DeleteLists(GLuint list, GLsizei range) {
        (void)list;
        if (range < 0) RecordError(ErrorCode::InvalidValue, __func__, "range cannot be negative");
    }

    void NewList(GLuint list, GLenum mode) {
        (void)list;
        (void)mode;
        RecordError(ErrorCode::InvalidOperation, __func__, "display lists are deliberately disabled");
    }

    void EndList() {
        RecordError(ErrorCode::InvalidOperation, __func__, "display lists are deliberately disabled");
    }

    void CallList(GLuint list) {
        (void)list;
        RecordError(ErrorCode::InvalidOperation, __func__, "display lists are deliberately disabled");
    }

    void DebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count,
                             const GLuint* ids, GLboolean enabled) {
        (void)source;
        (void)type;
        (void)severity;
        (void)count;
        (void)ids;
        (void)enabled;
    }

    void DebugMessageCallback(GLDEBUGPROC callback, const void* userParam) {
        (void)callback;
        (void)userParam;
    }

    void TextureView(GLuint texture, GLenum target, GLuint originalTexture, GLenum internalFormat,
                     GLuint minLevel, GLuint numLevels, GLuint minLayer, GLuint numLayers) {
        (void)texture;
        (void)target;
        (void)originalTexture;
        (void)internalFormat;
        (void)minLevel;
        (void)numLevels;
        (void)minLayer;
        (void)numLayers;
        RecordError(ErrorCode::InvalidOperation, __func__,
                    "GL_ARB_texture_view is not advertised by the DirectGLES GL 4.0 profile");
    }
}
