// MobileGL-PZ BUG-002/003 bounded observation-only diagnostics.
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <Includes.h>

#ifdef MOBILEPZ_BUG002_WFX_QUAD_DIAG

#include <MG_State/GLState/Core.h>
#include <MG_State/GLState/ProgramState/ProgramObject.h>
#include <MG_State/GLState/TextureState/TextureObject.h>
#include <chrono>

namespace MobileGL::MG_Util::BUGWeatherQuadDiag {
    constexpr Uint32 kSchema = 2;
    constexpr SizeT kProgramLimit = 256;
    constexpr SizeT kQuadSignatureLimit = 256;
    constexpr SizeT kQuadProgramReserveLimit = 128;
    constexpr Uint32 kQuadDetailedEventLimit = 384;
    constexpr Uint32 kWeatherEventLimit = 512;
    constexpr Uint64 kSampleIntervalMs = 5000;
    constexpr Uint32 kNativeLinesPerDraw = 8;
    constexpr Uint32 kAttributeLineLimit = 16;
    constexpr Uint32 kTextureLineLimit = 4;

    enum TokenBit : Uint32 {
        TokenWeatherFx = 1u << 0,
        TokenWeather = 1u << 1,
        TokenRain = 1u << 2,
        TokenSnow = 1u << 3,
        TokenPrecip = 1u << 4,
        TokenWind = 1u << 5,
        TokenParticle = 1u << 6,
        TokenCloud = 1u << 7,
        TokenFog = 1u << 8,
        TokenPosition = 1u << 9,
        TokenTexcoord = 1u << 10,
        TokenUv = 1u << 11,
        TokenColor = 1u << 12,
        TokenAlpha = 1u << 13,
    };

    constexpr Uint32 kDirectWeatherMask = TokenWeatherFx | TokenWeather | TokenRain |
        TokenSnow | TokenPrecip | TokenWind | TokenParticle | TokenCloud | TokenFog;

    struct ProgramRecord {
        std::atomic<Uint64> lifetime{0};
        Uint external = 0;
        Uint64 shaderFingerprint = 0;
        Uint32 tokenMask = 0;
        Bool layoutAccepted = false;
        Bool weatherCandidate = false;
        std::atomic<Uint32> weatherEvents{0};
        std::atomic<Uint64> lastSampleMs{0};
    };

    struct LastUse {
        Uint64 sequence = 0;
        Uint external = 0;
        Uint64 lifetime = 0;
    };

    struct CompatQuad {
        Bool active = false;
        Bool capture = false;
        const char* route = "none";
        GLenum mode = 0;
        GLsizei count = 0;
        GLenum type = 0;
        Bool indexed = false;
        Bool clientArray = false;
    };

    struct PendingDraw {
        Bool active = false;
        Uint64 sequence = 0;
        Bool weather = false;
        Bool quad = false;
        Uint32 nativeLines = 0;
        GLenum sourceMode = 0;
        GLsizei sourceCount = 0;
    };

    inline std::array<ProgramRecord, kProgramLimit> g_programs{};
    inline std::array<std::atomic<Uint64>, kQuadSignatureLimit> g_quadSignatures{};
    inline std::array<std::atomic<Uint64>, kQuadProgramReserveLimit> g_quadPrograms{};
    inline std::atomic<Uint64> g_sequence{1};
    inline std::atomic<Uint32> g_weatherEvents{0};
    inline std::atomic<Uint32> g_quadDetailedEvents{0};
    inline std::atomic<Bool> g_programCapLogged{false};
    inline std::atomic<Bool> g_quadCapLogged{false};
    inline std::atomic<Bool> g_quadProgramCapLogged{false};
    inline std::atomic<Bool> g_quadEventCapLogged{false};
    inline std::atomic<Bool> g_weatherCapLogged{false};
    inline thread_local LastUse g_lastUse{};
    inline thread_local CompatQuad g_compatQuad{};
    inline thread_local PendingDraw g_pending{};

    inline void Log(const char* format, ...) {
        char buffer[3072];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        __android_log_print(ANDROID_LOG_WARN, "MGLPZ-BUGDIAG", "%s", buffer);
    }

    inline Uint64 NowMs() {
        return static_cast<Uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    inline Uint64 HashBytes(Uint64 hash, const void* data, SizeT size) {
        constexpr Uint64 prime = 1099511628211ull;
        const auto* bytes = static_cast<const Uint8*>(data);
        for (SizeT i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= prime;
        }
        return hash;
    }

    inline Uint64 HashString(std::string_view text) {
        return HashBytes(1469598103934665603ull, text.data(), text.size());
    }

    inline char LowerAscii(char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    inline Bool ContainsFold(std::string_view haystack, std::string_view needle) {
        if (needle.empty() || haystack.size() < needle.size()) return false;
        for (SizeT start = 0; start + needle.size() <= haystack.size(); ++start) {
            Bool equal = true;
            for (SizeT i = 0; i < needle.size(); ++i) {
                if (LowerAscii(haystack[start + i]) != LowerAscii(needle[i])) {
                    equal = false;
                    break;
                }
            }
            if (equal) return true;
        }
        return false;
    }

    inline Uint32 SourceTokens(std::string_view source) {
        Uint32 mask = 0;
        if (ContainsFold(source, "weatherfx")) mask |= TokenWeatherFx;
        if (ContainsFold(source, "weather")) mask |= TokenWeather;
        if (ContainsFold(source, "rain")) mask |= TokenRain;
        if (ContainsFold(source, "snow")) mask |= TokenSnow;
        if (ContainsFold(source, "precip")) mask |= TokenPrecip;
        if (ContainsFold(source, "wind")) mask |= TokenWind;
        if (ContainsFold(source, "particle")) mask |= TokenParticle;
        if (ContainsFold(source, "cloud")) mask |= TokenCloud;
        if (ContainsFold(source, "fog")) mask |= TokenFog;
        if (ContainsFold(source, "position")) mask |= TokenPosition;
        if (ContainsFold(source, "texcoord")) mask |= TokenTexcoord;
        if (ContainsFold(source, "uv")) mask |= TokenUv;
        if (ContainsFold(source, "color") || ContainsFold(source, "colour")) mask |= TokenColor;
        if (ContainsFold(source, "alpha")) mask |= TokenAlpha;
        return mask;
    }

    inline void AppendToken(char* output, SizeT outputSize, SizeT& used, const char* name) {
        if (used >= outputSize) return;
        const int written = std::snprintf(output + used, outputSize - used, "%s%s", used ? "," : "", name);
        if (written > 0) used = std::min(outputSize, used + static_cast<SizeT>(written));
    }

    inline void FormatTokens(Uint32 mask, char* output, SizeT outputSize) {
        if (outputSize == 0) return;
        output[0] = '\0';
        SizeT used = 0;
        if (mask & TokenWeatherFx) AppendToken(output, outputSize, used, "weatherfx");
        if (mask & TokenWeather) AppendToken(output, outputSize, used, "weather");
        if (mask & TokenRain) AppendToken(output, outputSize, used, "rain");
        if (mask & TokenSnow) AppendToken(output, outputSize, used, "snow");
        if (mask & TokenPrecip) AppendToken(output, outputSize, used, "precip");
        if (mask & TokenWind) AppendToken(output, outputSize, used, "wind");
        if (mask & TokenParticle) AppendToken(output, outputSize, used, "particle");
        if (mask & TokenCloud) AppendToken(output, outputSize, used, "cloud");
        if (mask & TokenFog) AppendToken(output, outputSize, used, "fog");
        if (mask & TokenPosition) AppendToken(output, outputSize, used, "position");
        if (mask & TokenTexcoord) AppendToken(output, outputSize, used, "texcoord");
        if (mask & TokenUv) AppendToken(output, outputSize, used, "uv");
        if (mask & TokenColor) AppendToken(output, outputSize, used, "color");
        if (mask & TokenAlpha) AppendToken(output, outputSize, used, "alpha");
        if (used == 0) std::snprintf(output, outputSize, "none");
    }

    inline Bool LegacyQuadMode(GLenum mode) {
        return mode == GL_QUADS || mode == GL_QUAD_STRIP || mode == GL_POLYGON;
    }

    inline ProgramRecord* FindOrClaimProgram(Uint64 lifetime, Bool& claimed) {
        claimed = false;
        if (lifetime == 0) return nullptr;
        for (auto& record : g_programs) {
            Uint64 observed = record.lifetime.load(std::memory_order_acquire);
            if (observed == lifetime) return &record;
            if (observed != 0) continue;
            Uint64 expected = 0;
            if (record.lifetime.compare_exchange_strong(expected, lifetime, std::memory_order_acq_rel)) {
                claimed = true;
                return &record;
            }
            if (expected == lifetime) return &record;
        }
        if (!g_programCapLogged.exchange(true)) {
            Log("BUG004_PROGRAM_CAP schema=%u limit=%zu further_programs=unclassified", kSchema, kProgramLimit);
        }
        return nullptr;
    }

    inline ProgramRecord* ClassifyProgram(const SharedPtr<MG_State::GLState::ProgramObject>& program,
                                          const char* firstRoute, GLenum firstMode, GLsizei firstCount) {
        if (!program) return nullptr;
        Bool claimed = false;
        ProgramRecord* record = FindOrClaimProgram(program->GetLifetimeId(), claimed);
        if (!record || !claimed) return record;

        record->external = program->GetExternalIndex();
        Uint64 fingerprint = 1469598103934665603ull;
        Uint32 tokens = 0;
        for (const auto& shader : program->GetAttachedShaders()) {
            if (!shader) continue;
            const Uint32 stage = static_cast<Uint32>(shader->GetShaderStage());
            fingerprint = HashBytes(fingerprint, &stage, sizeof(stage));
            const String& source = shader->GetShaderSource();
            fingerprint = HashBytes(fingerprint, source.data(), source.size());
            tokens |= SourceTokens(source);
        }

        Bool hasPosition = false;
        Bool hasUv = false;
        Bool hasColor = false;
        const Int attributeCount = program->GetActiveAttributesCount();
        for (Int i = 0; i < attributeCount; ++i) {
            const String& name = program->GetActiveAttribName(static_cast<Uint>(i));
            hasPosition = hasPosition || ContainsFold(name, "position") || ContainsFold(name, "vertex");
            hasUv = hasUv || ContainsFold(name, "texcoord") || ContainsFold(name, "uv");
            hasColor = hasColor || ContainsFold(name, "color") || ContainsFold(name, "colour");
        }

        record->shaderFingerprint = fingerprint;
        record->tokenMask = tokens;
        record->layoutAccepted = hasPosition && hasUv;
        // Schema 1 incorrectly discarded programs without lexical weather
        // tokens. Schema 2 keeps the tokens only as evidence and samples every
        // program/route without using them as a gate.
        record->weatherCandidate = true;

        char tokenText[256];
        FormatTokens(tokens, tokenText, sizeof(tokenText));
        Log("BUG004_PROGRAM_CENSUS schema=%u program=%u lifetime=%llu shader_fp=%016llx "
            "tokens=0x%08x token_names=%s match=%u reason=%s attrs=%d pos=%u uv=%u color=%u "
            "first_route=%s mode=0x%x count=%d",
            kSchema, record->external, static_cast<unsigned long long>(program->GetLifetimeId()),
            static_cast<unsigned long long>(fingerprint), tokens, tokenText,
            record->weatherCandidate ? 1u : 0u,
            "route_census_no_lexical_filter",
            attributeCount, hasPosition ? 1u : 0u, hasUv ? 1u : 0u, hasColor ? 1u : 0u,
            firstRoute, firstMode, firstCount);
        Log("BUG004_PROGRAM_LAYOUT schema=%u program=%u lifetime=%llu attrs=%d pos=%u uv=%u color=%u "
            "decision=%s reason=%s",
            kSchema, record->external, static_cast<unsigned long long>(program->GetLifetimeId()), attributeCount,
            hasPosition ? 1u : 0u, hasUv ? 1u : 0u, hasColor ? 1u : 0u,
            record->layoutAccepted ? "accepted" : "rejected",
            !hasPosition ? "missing_position" : (!hasUv ? "missing_texcoord_or_uv" : "position_and_uv_present"));

        const Uint32 lines = std::min<Uint32>(static_cast<Uint32>(std::max(attributeCount, 0)),
                                              kAttributeLineLimit);
        for (Uint32 i = 0; i < lines; ++i) {
            const String& name = program->GetActiveAttribName(i);
            Log("BUG004_PROGRAM_ATTR schema=%u program=%u lifetime=%llu active_index=%u location=%d "
                "type=0x%x size=%d name=%s name_fp=%016llx",
                kSchema, record->external, static_cast<unsigned long long>(program->GetLifetimeId()), i,
                program->GetAttributeLocation(name), program->GetActiveAttribType(i),
                program->GetActiveAttribArraySize(i), name.c_str(),
                static_cast<unsigned long long>(HashString(name)));
        }
        if (attributeCount > static_cast<Int>(kAttributeLineLimit)) {
            Log("BUG004_PROGRAM_ATTR_CAP schema=%u program=%u lifetime=%llu logged=%u total=%d",
                kSchema, record->external, static_cast<unsigned long long>(program->GetLifetimeId()),
                kAttributeLineLimit, attributeCount);
        }
        return record;
    }

    inline void OnUseProgram(const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        g_lastUse.sequence = g_sequence.fetch_add(1, std::memory_order_relaxed);
        g_lastUse.external = program ? program->GetExternalIndex() : 0;
        g_lastUse.lifetime = program ? program->GetLifetimeId() : 0;
    }

    inline Uint64 QuadSignature(Uint64 lifetime, const char* route, GLenum mode, GLsizei count,
                                Bool indexed, GLsizei instances) {
        Uint64 hash = HashString(route);
        hash = HashBytes(hash, &lifetime, sizeof(lifetime));
        hash = HashBytes(hash, &mode, sizeof(mode));
        hash = HashBytes(hash, &count, sizeof(count));
        hash = HashBytes(hash, &indexed, sizeof(indexed));
        hash = HashBytes(hash, &instances, sizeof(instances));
        return hash == 0 ? 1 : hash;
    }

    inline Bool ClaimQuadSignature(Uint64 signature) {
        for (auto& slot : g_quadSignatures) {
            Uint64 observed = slot.load(std::memory_order_acquire);
            if (observed == signature) return false;
            if (observed != 0) continue;
            Uint64 expected = 0;
            if (slot.compare_exchange_strong(expected, signature, std::memory_order_acq_rel)) return true;
            if (expected == signature) return false;
        }
        if (!g_quadCapLogged.exchange(true)) {
            Log("BUG004_COMPAT_SIGNATURE_CAP schema=%u limit=%zu further_signatures=suppressed", kSchema, kQuadSignatureLimit);
        }
        return false;
    }

    inline Bool ClaimQuadProgram(Uint64 lifetime) {
        // Program 0 is still a useful fixed-function/compatibility bucket.
        const Uint64 key = lifetime == 0 ? 1 : lifetime;
        for (auto& slot : g_quadPrograms) {
            Uint64 observed = slot.load(std::memory_order_acquire);
            if (observed == key) return false;
            if (observed != 0) continue;
            Uint64 expected = 0;
            if (slot.compare_exchange_strong(expected, key, std::memory_order_acq_rel)) return true;
            if (expected == key) return false;
        }
        if (!g_quadProgramCapLogged.exchange(true)) {
            Log("BUG004_COMPAT_PROGRAM_CAP schema=%u limit=%zu further_programs=suppressed",
                kSchema, kQuadProgramReserveLimit);
        }
        return false;
    }

    inline Bool ClaimQuadDetailedEvent() {
        const Uint32 event = g_quadDetailedEvents.fetch_add(1, std::memory_order_relaxed);
        if (event < kQuadDetailedEventLimit) return true;
        if (!g_quadEventCapLogged.exchange(true)) {
            Log("BUG004_COMPAT_EVENT_CAP schema=%u limit=%u further_details=suppressed counters_continue=1",
                kSchema, kQuadDetailedEventLimit);
        }
        return false;
    }

    inline void ObserveCompatQuad(const char* route, GLenum mode, GLsizei count, GLenum type,
                                  Bool indexed, Bool clientArray) {
        // Schema 2 observes every PZCompat primitive. LegacyQuadMode remains
        // evidence in the record, never a capture filter.
        const auto& current = MG_State::pGLContext ? MG_State::pGLContext->GetCurrentProgram()
                                                   : SharedPtr<MG_State::GLState::ProgramObject>{};
        const Uint64 lifetime = current ? current->GetLifetimeId() : 0;
        const Uint external = current ? current->GetExternalIndex() : 0;
        const Uint64 signature = QuadSignature(lifetime, route, mode, count, indexed, 1);
        const Bool newProgram = ClaimQuadProgram(lifetime);
        const Bool newSignature = ClaimQuadSignature(signature);
        const Bool capture = (newProgram || newSignature) && ClaimQuadDetailedEvent();
        if (capture && newProgram) {
            Log("BUG004_COMPAT_PROGRAM schema=%u route=%s program=%u lifetime=%llu first_route=1 "
                "reserved_program_slot=1",
                kSchema, route, external, static_cast<unsigned long long>(lifetime));
        }
        if (capture && newSignature) {
            Log("BUG004_COMPAT_ENTRY schema=%u stage=pzcompat route=%s program=%u lifetime=%llu "
                "mode=0x%x count=%d indexed=%u type=0x%x client_array=%u legacy_quad_mode=%u "
                "outcome=observed_before_translation primitive_filter=none",
                kSchema, route, external, static_cast<unsigned long long>(lifetime), mode, count,
                indexed ? 1u : 0u, type, clientArray ? 1u : 0u, LegacyQuadMode(mode) ? 1u : 0u);
        }
        // Keep the compat handoff for every quad, but detailed route logging is
        // enabled only for a new signature or a program-reserved first quad.
        g_compatQuad = {true, capture, route, mode, count, type, indexed, clientArray};
    }

    class ScopedCompatQuad {
    public:
        ScopedCompatQuad(const char* route, GLenum mode, GLsizei count, GLenum type,
                         Bool indexed, Bool clientArray) {
            ObserveCompatQuad(route, mode, count, type, indexed, clientArray);
        }
        ~ScopedCompatQuad() { g_compatQuad = {}; }
        ScopedCompatQuad(const ScopedCompatQuad&) = delete;
        ScopedCompatQuad& operator=(const ScopedCompatQuad&) = delete;
    };

    inline void ObserveRejectedPrimitive(const char* route, GLenum mode) {
        const auto& current = MG_State::pGLContext ? MG_State::pGLContext->GetCurrentProgram()
                                                   : SharedPtr<MG_State::GLState::ProgramObject>{};
        const Uint64 lifetime = current ? current->GetLifetimeId() : 0;
        const Uint external = current ? current->GetExternalIndex() : 0;
        const Uint64 signature = QuadSignature(lifetime, route, mode, 0, false, 1);
        const Bool newProgram = ClaimQuadProgram(lifetime);
        const Bool newSignature = ClaimQuadSignature(signature);
        const Bool capture = (newProgram || newSignature) && ClaimQuadDetailedEvent();
        if (capture) {
            Log("BUG004_REJECTED_PRIMITIVE schema=%u stage=frontend route=%s program=%u lifetime=%llu "
                "mode=0x%x count=unknown legacy_quad_mode=%u outcome=rejected "
                "reason=primitive_mode_not_accepted",
                kSchema, route, external, static_cast<unsigned long long>(lifetime), mode,
                LegacyQuadMode(mode) ? 1u : 0u);
        }
        g_compatQuad = {};
    }

    inline TextureTarget SamplerTarget(GLenum type) {
        switch (type) {
        case GL_SAMPLER_1D: case GL_INT_SAMPLER_1D: case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_SAMPLER_1D_SHADOW: return TextureTarget::Texture1D;
        case GL_SAMPLER_2D: case GL_INT_SAMPLER_2D: case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW: return TextureTarget::Texture2D;
        case GL_SAMPLER_3D: case GL_INT_SAMPLER_3D: case GL_UNSIGNED_INT_SAMPLER_3D:
            return TextureTarget::Texture3D;
        case GL_SAMPLER_CUBE: case GL_INT_SAMPLER_CUBE: case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_SAMPLER_CUBE_SHADOW: return TextureTarget::TextureCubeMap;
        case GL_SAMPLER_1D_ARRAY: case GL_INT_SAMPLER_1D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_1D_ARRAY_SHADOW: return TextureTarget::Texture1DArray;
        case GL_SAMPLER_2D_ARRAY: case GL_INT_SAMPLER_2D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW: return TextureTarget::Texture2DArray;
        case GL_SAMPLER_CUBE_MAP_ARRAY: case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY: case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
            return TextureTarget::TextureCubeMapArray;
        case GL_SAMPLER_2D_RECT: case GL_INT_SAMPLER_2D_RECT: case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
        case GL_SAMPLER_2D_RECT_SHADOW: return TextureTarget::TextureRectangle;
        case GL_SAMPLER_2D_MULTISAMPLE: case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: return TextureTarget::Texture2DMultisample;
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return TextureTarget::Texture2DMultisampleArray;
        case GL_SAMPLER_BUFFER: case GL_INT_SAMPLER_BUFFER: case GL_UNSIGNED_INT_SAMPLER_BUFFER:
            return TextureTarget::TextureBuffer;
        default: return TextureTarget::Unknown;
        }
    }

    inline void LogVisibleState(Uint64 sequence,
                                const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        if (!MG_State::pGLContext || !program) return;
        const auto& state = MG_State::pGLContext->GetRenderStateParameters();
        const auto& blend = state.BlendStates[0];
        const auto& mask = state.ColorMasks[0];
        const auto& fbo = MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw).GetBoundObject();
        const Uint fboName = fbo ? fbo->GetExternalIndex() : 0;
        const Bool fboComplete = !fbo || fbo->IsDefaultFramebuffer() || fbo->CheckCompleteness();
        Log("BUG004_VISIBLE_STATE schema=%u seq=%llu program=%u lifetime=%llu fbo=%u fbo_complete=%u "
            "blend=%u src_rgb=%u dst_rgb=%u src_a=%u dst_a=%u eq_rgb=%u eq_a=%u "
            "depth=%u depth_write=%u depth_func=%u cull=%u cull_mode=%u cmask=%u%u%u%u "
            "viewport=%d,%d,%d,%d scissor=%u,%d,%d,%d,%d",
            kSchema, static_cast<unsigned long long>(sequence), program->GetExternalIndex(),
            static_cast<unsigned long long>(program->GetLifetimeId()), fboName, fboComplete ? 1u : 0u,
            blend.Enabled ? 1u : 0u, static_cast<Uint>(blend.SrcFactorRGB), static_cast<Uint>(blend.DstFactorRGB),
            static_cast<Uint>(blend.SrcFactorAlpha), static_cast<Uint>(blend.DstFactorAlpha),
            static_cast<Uint>(blend.ColorEquation), static_cast<Uint>(blend.AlphaEquation),
            state.DepthTestEnabled ? 1u : 0u, state.DepthMask ? 1u : 0u, static_cast<Uint>(state.DepthFunc),
            state.CullFaceEnabled ? 1u : 0u, static_cast<Uint>(state.CullFaceModeSetting),
            mask.x() ? 1u : 0u, mask.y() ? 1u : 0u, mask.z() ? 1u : 0u, mask.w() ? 1u : 0u,
            state.Viewport.x(), state.Viewport.y(), state.Viewport.z(), state.Viewport.w(),
            state.ScissorTestEnabled ? 1u : 0u, state.ScissorBox.x(), state.ScissorBox.y(),
            state.ScissorBox.z(), state.ScissorBox.w());

        Uint32 textureLines = 0;
        const Uint uniformCount = program->GetUniformCount();
        for (Uint uniformIndex = 0; uniformIndex < uniformCount && textureLines < kTextureLineLimit; ++uniformIndex) {
            const GLenum type = program->GetActiveUniformType(uniformIndex);
            const TextureTarget target = SamplerTarget(type);
            if (target == TextureTarget::Unknown) continue;
            const String& name = program->GetActiveUniformName(uniformIndex);
            const Int location = program->GetUniformLocation(name);
            if (location < 0) continue;
            const Int unit = program->GetUniformSamplerOrImageUnitIndex(static_cast<Uint>(location));
            if (unit < 0 || unit >= MG_State::GLState::TextureState::MAX_TEXTURE_IMAGE_UNITS) continue;
            auto& textureUnit = MG_State::pGLContext->GetTextureUnitObject(unit);
            const auto& texture = textureUnit.GetBindingSlot(target).GetBoundObject();
            const auto& unitSampler = textureUnit.GetSamplerObject();
            const auto& sampler = unitSampler ? unitSampler : (texture ? texture->GetSamplerObject() : nullptr);
            const Bool mipmapped = sampler && sampler->GetMipmapMode() != SamplerMipmapMode::None;
            Log("BUG004_VISIBLE_TEXTURE schema=%u seq=%llu uniform=%s uniform_fp=%016llx type=0x%x unit=%d "
                "target=%d texture=%u texture_lifetime=%llu complete=%u mip_complete=%u sampler=%u "
                "sampler_lifetime=%llu min_filter=%d mip_filter=%d",
                kSchema, static_cast<unsigned long long>(sequence), name.c_str(),
                static_cast<unsigned long long>(HashString(name)), type, unit, static_cast<Int>(target),
                texture ? texture->GetExternalIndex() : 0,
                static_cast<unsigned long long>(texture ? texture->GetLifetimeId() : 0),
                texture && texture->IsComplete() ? 1u : 0u,
                texture && texture->IsMipmapCompleteForFilterCached(mipmapped) ? 1u : 0u,
                sampler ? sampler->GetExternalIndex() : 0,
                static_cast<unsigned long long>(sampler ? sampler->GetLifetimeId() : 0),
                sampler ? static_cast<Int>(sampler->GetMinFilter()) : -1,
                sampler ? static_cast<Int>(sampler->GetMipmapMode()) : -1);
            ++textureLines;
        }
        if (textureLines == 0) {
            Log("BUG004_VISIBLE_TEXTURE schema=%u seq=%llu sampler_uniforms=none", kSchema,
                static_cast<unsigned long long>(sequence));
        }
    }

    inline void BeginBackendDraw(const char* route, GLenum mode, GLsizei count, GLsizei instances,
                                 Bool indexed, GLsizei subdraws) {
        g_pending = {};
        if (!MG_State::pGLContext) return;
        const auto& selected = MG_State::pGLContext->GetCurrentProgram();
        const auto& drawProgram = MG_State::pGLContext->GetProgramForDraw();
        ProgramRecord* record = ClassifyProgram(drawProgram, route, mode, count);
        Bool weather = false;
        if (record) {
            const Uint64 now = NowMs();
            Uint64 observed = record->lastSampleMs.load(std::memory_order_relaxed);
            while (observed == 0 || (now >= observed && now - observed >= kSampleIntervalMs)) {
                if (record->lastSampleMs.compare_exchange_weak(observed, now, std::memory_order_acq_rel)) {
                    weather = true;
                    break;
                }
            }
        }
        const Bool quad = g_compatQuad.active && g_compatQuad.capture;
        if (!weather && !quad) {
            g_compatQuad = {};
            return;
        }

        if (weather) {
            const Uint32 event = g_weatherEvents.fetch_add(1, std::memory_order_relaxed);
            if (event >= kWeatherEventLimit) {
                if (!g_weatherCapLogged.exchange(true)) {
                    Log("BUG004_EVENT_CAP schema=%u limit=%u further_route_samples=suppressed",
                        kSchema, kWeatherEventLimit);
                }
                weather = false;
                if (!quad) return;
            }
        }

        const Uint64 sequence = g_sequence.fetch_add(1, std::memory_order_relaxed);
        const GLenum sourceMode = g_compatQuad.active ? g_compatQuad.mode : mode;
        const GLsizei sourceCount = g_compatQuad.active ? g_compatQuad.count : count;
        g_pending = {true, sequence, weather, quad, 0, sourceMode, sourceCount};
        const Uint selectedExternal = selected ? selected->GetExternalIndex() : 0;
        const Uint64 selectedLifetime = selected ? selected->GetLifetimeId() : 0;
        const Uint drawExternal = drawProgram ? drawProgram->GetExternalIndex() : 0;
        const Uint64 drawLifetime = drawProgram ? drawProgram->GetLifetimeId() : 0;
        const Uint fbo = MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw).GetBoundObject()
            ? MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw).GetBoundObject()->GetExternalIndex()
            : 0;
        if (weather) {
            Log("BUG004_USE_TO_DRAW schema=%u seq=%llu use_seq=%llu selected_program=%u selected_lifetime=%llu "
                "draw_program=%u draw_lifetime=%llu same=%u route=%s",
                kSchema, static_cast<unsigned long long>(sequence), static_cast<unsigned long long>(g_lastUse.sequence),
                selectedExternal, static_cast<unsigned long long>(selectedLifetime), drawExternal,
                static_cast<unsigned long long>(drawLifetime), selected == drawProgram ? 1u : 0u, route);
            Log("BUG004_DRAW_SAMPLE schema=%u seq=%llu stage=frontend_to_backend route=%s source_route=%s "
                "source_mode=0x%x submitted_mode=0x%x source_count=%d submitted_count=%d indexed=%u "
                "instances=%d subdraws=%d fbo=%u reason=periodic_5s primitive_filter=none",
                kSchema, static_cast<unsigned long long>(sequence), route,
                g_compatQuad.active ? g_compatQuad.route : route, sourceMode, mode, sourceCount, count,
                indexed ? 1u : 0u, instances, subdraws, fbo);
        }
        if (quad) {
            Log("BUG004_COMPAT_TO_DRAW schema=%u seq=%llu use_seq=%llu selected_program=%u "
                "selected_lifetime=%llu draw_program=%u draw_lifetime=%llu same=%u route=%s",
                kSchema, static_cast<unsigned long long>(sequence), static_cast<unsigned long long>(g_lastUse.sequence),
                selectedExternal, static_cast<unsigned long long>(selectedLifetime), drawExternal,
                static_cast<unsigned long long>(drawLifetime), selected == drawProgram ? 1u : 0u, route);
            Log("BUG004_COMPAT_ROUTE schema=%u seq=%llu stage=frontend_to_backend route=%s source_route=%s "
                "source_mode=0x%x submitted_mode=0x%x source_count=%d submitted_count=%d indexed=%u "
                "instances=%d outcome=submitted_to_backend",
                kSchema, static_cast<unsigned long long>(sequence), route,
                g_compatQuad.active ? g_compatQuad.route : route, sourceMode, mode, sourceCount, count,
                indexed ? 1u : 0u, instances);
        }

        if (weather && record->weatherEvents.fetch_add(1, std::memory_order_relaxed) < 32) {
            LogVisibleState(sequence, drawProgram);
        }
        g_compatQuad = {};
    }

    inline void EndBackendDraw() {
        g_pending = {};
    }

    class ScopedBackendDraw {
    public:
        ScopedBackendDraw(const char* route, GLenum mode, GLsizei count, GLsizei instances,
                          Bool indexed, GLsizei subdraws) {
            BeginBackendDraw(route, mode, count, instances, indexed, subdraws);
        }
        ~ScopedBackendDraw() { EndBackendDraw(); }
        ScopedBackendDraw(const ScopedBackendDraw&) = delete;
        ScopedBackendDraw& operator=(const ScopedBackendDraw&) = delete;
    };

    inline void NativeSubmit(const char* route, GLenum mode, GLsizei count, GLsizei instances,
                             Bool indexed, GLsizei subdraw, Uint backendProgram) {
        if (!g_pending.active || g_pending.nativeLines >= kNativeLinesPerDraw) return;
        ++g_pending.nativeLines;
        if (g_pending.weather) {
            Log("BUG004_NATIVE_SUBMIT schema=%u seq=%llu stage=native route=%s source_mode=0x%x native_mode=0x%x "
                "source_count=%d native_count=%d indexed=%u instances=%d subdraw=%d backend_program=%u",
                kSchema, static_cast<unsigned long long>(g_pending.sequence), route, g_pending.sourceMode, mode,
                g_pending.sourceCount, count, indexed ? 1u : 0u, instances, subdraw, backendProgram);
        }
        if (g_pending.quad) {
            Log("BUG004_COMPAT_NATIVE schema=%u seq=%llu stage=native route=%s source_mode=0x%x native_mode=0x%x "
                "source_count=%d native_count=%d indexed=%u instances=%d subdraw=%d backend_program=%u "
                "outcome=native_submit",
                kSchema, static_cast<unsigned long long>(g_pending.sequence), route, g_pending.sourceMode, mode,
                g_pending.sourceCount, count, indexed ? 1u : 0u, instances, subdraw, backendProgram);
        }
    }

    inline void EmitInitMarker() {
        Log("BUG004_ROUTE_CENSUS_ACTIVE schema=%u base=OPT-LAB-V3-022 weather_filter=none "
            "primitive_filter=none observation_only=1 "
            "base_source_id=c63202c01d21b897a0802578f383ed2c5f46cdf09fcf86ccf95b6146d3d0d91f "
            "target=com.zomdroid.mglpz2 "
            "render_fix=none program_limit=%zu weather_event_limit=%u quad_signature_limit=%zu "
            "quad_program_reserve=%zu quad_event_limit=%u "
            "native_queries=none glFinish=none glReadPixels=none",
            kSchema, kProgramLimit, kWeatherEventLimit, kQuadSignatureLimit,
            kQuadProgramReserveLimit, kQuadDetailedEventLimit);
    }
} // namespace MobileGL::MG_Util::BUGWeatherQuadDiag

#else

namespace MobileGL::MG_Util::BUGWeatherQuadDiag {
    template <typename... Args> inline void OnUseProgram(Args&&...) {}
    template <typename... Args> inline void ObserveCompatQuad(Args&&...) {}
    template <typename... Args> inline void ObserveRejectedPrimitive(Args&&...) {}
    template <typename... Args> inline void NativeSubmit(Args&&...) {}
    inline void EmitInitMarker() {}
    class ScopedBackendDraw {
    public:
        template <typename... Args>
        explicit ScopedBackendDraw(Args&&...) {}
    };
    class ScopedCompatQuad {
    public:
        template <typename... Args>
        explicit ScopedCompatQuad(Args&&...) {}
    };
} // namespace MobileGL::MG_Util::BUGWeatherQuadDiag

#endif
