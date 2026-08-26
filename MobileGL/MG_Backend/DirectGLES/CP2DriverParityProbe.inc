// MobilePZ V1.04 / CP2R5: one-shot, read-only parity capture for the SKINNED
// shader family established by VTX0 and CP1.  CP2R4's source-semantic fallback
// was accidentally placed behind the exact CP1 vertex-layout gate, so it could
// not observe a semantically matching SKINNED program whose live VAO differed.
// CP2R5 inventories programs before that layout gate and captures the first draw
// whose vertex source contains the exact CP1 skinning semantics.  The capture is
// emitted before the native draw, so the evidence survives a crash immediately
// after the draw.  No GL state or rendering argument is changed.

namespace {
    enum class CP2State : Uint8 { Dormant, Armed, Done };

    static constexpr Uint64 kCP2MaxWaitBoundaries = 36000;
    static constexpr SizeT kCP2MaxDriverReadBytes = 64u * 1024u;
    static constexpr SizeT kCP2SourceChunkBytes = 640;
    static constexpr SizeT kCP2MaxSourceChunks = 64;
    static constexpr Uint32 kCP2TargetAttributeMask = 0x1f;
    static constexpr SizeT kCP2MaxLayoutPrograms = 256;

    struct CP2BackendUniformLayout {
        Bool found = false;
        String selectedName;
        GLuint index = GL_INVALID_INDEX;
        GLint offset = -1;
        GLint blockIndex = -1;
        GLint type = 0;
        GLint size = 0;
        GLint arrayStride = 0;
        GLint matrixStride = 0;
        GLint rowMajor = 0;
    };

    struct CP2DriverBufferSnapshot {
        Bool readable = false;
        GLint buffer = 0;
        GLint64 start = 0;
        GLint64 range = 0;
        Vector<Uint8> bytes;
    };

    struct CP2LayoutProgramCandidate {
        const MG_State::GLState::ProgramObject* identity = nullptr;
        Uint64 lifetime = 0;
        Uint link = 0;
        Uint external = 0;
        Uint64 draws = 0;
        Bool uniformSurface = false;
        Bool sourceSemantic = false;
        Uint64 sourceHash = 0;
        SizeT sourceBytes = 0;
    };

    static CP2State g_cp2State = CP2State::Dormant;
    static Uint64 g_cp2BoundarySerial = 0;
    static Uint64 g_cp2WaitBoundaries = 0;
    static Uint64 g_cp2SeenDraws = 0;
    static Uint64 g_cp2RejectLayout = 0;
    static Uint64 g_cp2RejectUniformSurface = 0;
    static Uint64 g_cp2RejectPrimitive = 0;
    static Uint64 g_cp2RejectIndexType = 0;
    static Uint64 g_cp2RejectBaseVertex = 0;
    static Uint64 g_cp2RejectMissingBinding = 0;
    static Uint64 g_cp2RejectEnabledMask = 0;
    static Uint64 g_cp2RejectAttribute = 0;
    static Uint64 g_cp2RejectSharedVbo = 0;
    static Bool g_cp2EarlyArmChecked = false;
    static Bool g_cp2NearCoarseLogged = false;
    static Bool g_cp2NearLayoutLogged = false;
    static Bool g_cp2NearUniformLogged = false;
    static Bool g_cp2StartupLogged = false;
    static Uint g_cp2ScratchBuffer = 0;
    static Array<CP2LayoutProgramCandidate, kCP2MaxLayoutPrograms> g_cp2LayoutPrograms{};
    static SizeT g_cp2LayoutProgramCount = 0;
    static Uint64 g_cp2LayoutProgramOverflow = 0;
    static const MG_State::GLState::ProgramObject* g_cp2LastLayoutProgram = nullptr;
    static SizeT g_cp2LastLayoutProgramSlot = kCP2MaxLayoutPrograms;

    static void CP2LogLayoutProgramSummary(const char* reason);

    static Uint64 CP2HashBytes(const void* bytes, SizeT size) {
        if (!bytes || size == 0) return 0;
        const auto* p = static_cast<const Uint8*>(bytes);
        Uint64 hash = 1469598103934665603ull;
        for (SizeT i = 0; i < size; ++i) {
            hash ^= p[i];
            hash *= 1099511628211ull;
        }
        return hash;
    }

    static Bool CP2Contains(const String& source, const char* needle) {
        return needle && source.find(needle) != String::npos;
    }

    static void CP2LogStartupOnce() {
        if (g_cp2StartupLogged) return;
        g_cp2StartupLogged = true;
        MGLOG_I("MOBILEPZ_CP2_READY schema=7 revision=CP2R5 base=P15+P21A+DISPLAY route=OPENGL_CORE "
                "target=SKINNED_VERTEX_SOURCE_SEMANTIC auto_arm=1 wait_boundaries=%llu "
                "one_shot=1 capture_before_draw=1 checkpoints=1 "
                "all_draw_program_census=256 source_semantic_precedes_layout=1 "
                "rendering_mutation=0 error_drain=0",
                static_cast<unsigned long long>(kCP2MaxWaitBoundaries));
    }

    static void CP2ResetCounters() {
        g_cp2WaitBoundaries = 0;
        g_cp2SeenDraws = 0;
        g_cp2RejectLayout = 0;
        g_cp2RejectUniformSurface = 0;
        g_cp2RejectPrimitive = 0;
        g_cp2RejectIndexType = 0;
        g_cp2RejectBaseVertex = 0;
        g_cp2RejectMissingBinding = 0;
        g_cp2RejectEnabledMask = 0;
        g_cp2RejectAttribute = 0;
        g_cp2RejectSharedVbo = 0;
        g_cp2NearCoarseLogged = false;
        g_cp2NearLayoutLogged = false;
        g_cp2NearUniformLogged = false;
        g_cp2LayoutPrograms = {};
        g_cp2LayoutProgramCount = 0;
        g_cp2LayoutProgramOverflow = 0;
        g_cp2LastLayoutProgram = nullptr;
        g_cp2LastLayoutProgramSlot = kCP2MaxLayoutPrograms;
    }

    static void CP2LogCheckpoint(const char* reason) {
        if (g_cp2State != CP2State::Armed) return;
        MGLOG_I("MOBILEPZ_CP2_CHECKPOINT reason=%s wait_boundaries=%llu seen_draws=%llu "
                "reject_layout=%llu reject_uniform_surface=%llu reject_primitive=%llu "
                "reject_index_type=%llu reject_base_vertex=%llu reject_missing_binding=%llu "
                "reject_enabled_mask=%llu reject_attribute=%llu reject_shared_vbo=%llu "
                "layout_programs=%zu layout_program_overflow=%llu rendering_mutation=0",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(g_cp2WaitBoundaries),
                static_cast<unsigned long long>(g_cp2SeenDraws),
                static_cast<unsigned long long>(g_cp2RejectLayout),
                static_cast<unsigned long long>(g_cp2RejectUniformSurface),
                static_cast<unsigned long long>(g_cp2RejectPrimitive),
                static_cast<unsigned long long>(g_cp2RejectIndexType),
                static_cast<unsigned long long>(g_cp2RejectBaseVertex),
                static_cast<unsigned long long>(g_cp2RejectMissingBinding),
                static_cast<unsigned long long>(g_cp2RejectEnabledMask),
                static_cast<unsigned long long>(g_cp2RejectAttribute),
                static_cast<unsigned long long>(g_cp2RejectSharedVbo),
                g_cp2LayoutProgramCount,
                static_cast<unsigned long long>(g_cp2LayoutProgramOverflow));
    }

    static void CP2TryArm(const char* source) {
        CP2LogStartupOnce();
        if (g_cp2State != CP2State::Dormant) return;
        g_cp2State = CP2State::Armed;
        CP2ResetCounters();
        MGLOG_I("MOBILEPZ_CP2_ARMED boundary=%llu source=%s target=first_semantic_skinned_draw "
                "auto_arm=1 trigger_file_required=0 rendering_mutation=0",
                static_cast<unsigned long long>(g_cp2BoundarySerial),
                source ? source : "unknown");
    }

    static void CP2FinishIncomplete(const char* reason) {
        if (g_cp2State != CP2State::Armed) return;
        CP2LogCheckpoint(reason);
        CP2LogLayoutProgramSummary(reason);
        MGLOG_I("MOBILEPZ_CP2_RESULT verdict=CP2_CAPTURE_INCOMPLETE reason=%s "
                "wait_boundaries=%llu seen_draws=%llu reject_layout=%llu "
                "reject_uniform_surface=%llu rendering_mutation=0",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(g_cp2WaitBoundaries),
                static_cast<unsigned long long>(g_cp2SeenDraws),
                static_cast<unsigned long long>(g_cp2RejectLayout),
                static_cast<unsigned long long>(g_cp2RejectUniformSurface));
        g_cp2State = CP2State::Done;
    }

    static void CP2OnPresentBoundary() {
        ++g_cp2BoundarySerial;
        if (g_cp2State == CP2State::Armed) {
            ++g_cp2WaitBoundaries;
            if (g_cp2WaitBoundaries == 60 || g_cp2WaitBoundaries == 600 ||
                g_cp2WaitBoundaries == 3600 || g_cp2WaitBoundaries == 18000) {
                CP2LogCheckpoint("present_milestone");
            }
            if (g_cp2WaitBoundaries >= kCP2MaxWaitBoundaries) {
                CP2FinishIncomplete("wait_timeout");
            }
            return;
        }
        CP2TryArm("present");
    }

    static void CP2OnContextDestroyed() {
        CP2FinishIncomplete("context_destroyed_before_target");
        if (g_cp2ScratchBuffer != 0 && g_GLESFuncs.glDeleteBuffers) {
            g_GLESFuncs.glDeleteBuffers(1, &g_cp2ScratchBuffer);
            g_cp2ScratchBuffer = 0;
        }
    }

    static Bool CP2ExpectedAttribute(const MG_State::GLState::VertexAttribute& attribute,
                                     GLint size, SizeT offset) {
        return attribute.Enabled && attribute.Size == size && attribute.Type == DataType::Float32 &&
            !attribute.IsInteger && attribute.Stride == 64 && attribute.Offset == offset &&
            attribute.Buffer != nullptr;
    }

    static Uint32 CP2EnabledAttributeMask(
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao) {
        if (!vao) return 0;
        const auto& attributes = vao->GetAllAttributes();
        Uint32 enabledMask = 0;
        for (SizeT i = 0; i < attributes.size() && i < 32; ++i) {
            if (attributes[i].Enabled) enabledMask |= Uint32{1} << i;
        }
        return enabledMask;
    }

    static const char* CP2LayoutRejectReason(
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao,
        const SharedPtr<MG_State::GLState::BufferObject>& ebo) {
        if (!vao) return "missing_vao";
        if (!ebo) return "missing_ebo";
        const auto& attributes = vao->GetAllAttributes();
        if (attributes.size() < 5) return "attribute_vector_short";
        if (CP2EnabledAttributeMask(vao) != kCP2TargetAttributeMask) return "enabled_mask";
        if (!CP2ExpectedAttribute(attributes[0], 3, 0)) return "attribute_0";
        if (!CP2ExpectedAttribute(attributes[1], 3, 12)) return "attribute_1";
        if (!CP2ExpectedAttribute(attributes[2], 4, 24)) return "attribute_2";
        if (!CP2ExpectedAttribute(attributes[3], 4, 40)) return "attribute_3";
        if (!CP2ExpectedAttribute(attributes[4], 2, 56)) return "attribute_4";
        const auto* shared = attributes[0].Buffer.get();
        for (Uint i = 1; i < 5; ++i) {
            if (attributes[i].Buffer.get() != shared) return "different_vbo";
        }
        return nullptr;
    }

    static void CP2LogNearLayout(
        const char* reason, GLenum mode, GLsizei count, GLenum indexType, GLint baseVertex,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao,
        const SharedPtr<MG_State::GLState::BufferObject>& ebo) {
        const Bool coarse = reason &&
            (std::strcmp(reason, "missing_vao") == 0 ||
             std::strcmp(reason, "missing_ebo") == 0 ||
             std::strcmp(reason, "enabled_mask") == 0);
        Bool& logged = coarse ? g_cp2NearCoarseLogged : g_cp2NearLayoutLogged;
        if (logged) return;
        logged = true;
        MGLOG_I("MOBILEPZ_CP2_NEAR_LAYOUT reason=%s mode=0x%x count=%d index_type=0x%x "
                "base_vertex=%d front_program=%u front_vao=%u front_ebo=%u enabled_mask=0x%x",
                reason ? reason : "unknown", mode, count, indexType, baseVertex,
                program ? program->GetExternalIndex() : 0, vao ? vao->GetExternalIndex() : 0,
                ebo ? ebo->GetExternalIndex() : 0, CP2EnabledAttributeMask(vao));
        if (!vao) return;
        const auto& attributes = vao->GetAllAttributes();
        for (SizeT i = 0; i < attributes.size() && i < 5; ++i) {
            const auto& attribute = attributes[i];
            MGLOG_I("MOBILEPZ_CP2_NEAR_ATTRIBUTE location=%zu enabled=%d size=%d float32=%d "
                    "integer=%d stride=%zu offset=%zu front_buffer=%u",
                    i, attribute.Enabled ? 1 : 0, attribute.Size,
                    attribute.Type == DataType::Float32 ? 1 : 0,
                    attribute.IsInteger ? 1 : 0, static_cast<SizeT>(attribute.Stride),
                    static_cast<SizeT>(attribute.Offset),
                    attribute.Buffer ? attribute.Buffer->GetExternalIndex() : 0);
        }
    }

    static Bool CP2MatchesUniformSurface(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        if (!program) return false;
        const Int mvp = program->GetUniformLocation("ModelViewProjection");
        const Int palette = program->GetUniformLocation("MatrixPalette[0]");
        const Int finalScale = program->GetUniformLocation("FinalScale");
        const Int color = program->GetUniformLocation("u_color");
        const Int texture = program->GetUniformLocation("Texture");
        if (mvp < 0 || palette < 0 || finalScale < 0 || color < 0 || texture < 0) return false;
        if (program->GetUniformType(static_cast<Uint>(mvp)) != GL_FLOAT_MAT4 ||
            program->GetUniformType(static_cast<Uint>(palette)) != GL_FLOAT_MAT4 ||
            program->GetUniformType(static_cast<Uint>(finalScale)) != GL_FLOAT ||
            program->GetUniformType(static_cast<Uint>(color)) != GL_FLOAT_VEC4) {
            return false;
        }
        const Int paletteIndex = program->GetActiveUniformIndex("MatrixPalette[0]");
        return paletteIndex >= 0 &&
            program->GetActiveUniformArraySize(static_cast<Uint>(paletteIndex)) >= 60;
    }

    static void CP2LogNearUniformSurface(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        if (g_cp2NearUniformLogged) return;
        g_cp2NearUniformLogged = true;
        if (!program) {
            MGLOG_I("MOBILEPZ_CP2_NEAR_UNIFORM status=missing_program");
            return;
        }
        const Int mvp = program->GetUniformLocation("ModelViewProjection");
        const Int palette = program->GetUniformLocation("MatrixPalette[0]");
        const Int finalScale = program->GetUniformLocation("FinalScale");
        const Int color = program->GetUniformLocation("u_color");
        const Int texture = program->GetUniformLocation("Texture");
        const Int paletteIndex = program->GetActiveUniformIndex("MatrixPalette[0]");
        MGLOG_I("MOBILEPZ_CP2_NEAR_UNIFORM front_program=%u lifetime=%llu link=%u "
                "mvp_loc=%d mvp_type=0x%x palette_loc=%d palette_type=0x%x palette_size=%d "
                "final_scale_loc=%d final_scale_type=0x%x color_loc=%d color_type=0x%x "
                "texture_loc=%d texture_type=0x%x",
                program->GetExternalIndex(),
                static_cast<unsigned long long>(program->GetLifetimeId()),
                program->GetLinkVersion(), mvp,
                mvp >= 0 ? program->GetUniformType(static_cast<Uint>(mvp)) : 0,
                palette, palette >= 0 ? program->GetUniformType(static_cast<Uint>(palette)) : 0,
                paletteIndex >= 0
                    ? program->GetActiveUniformArraySize(static_cast<Uint>(paletteIndex)) : 0,
                finalScale,
                finalScale >= 0 ? program->GetUniformType(static_cast<Uint>(finalScale)) : 0,
                color, color >= 0 ? program->GetUniformType(static_cast<Uint>(color)) : 0,
                texture, texture >= 0 ? program->GetUniformType(static_cast<Uint>(texture)) : 0);
    }

    static void CP2LogSourceChunks(const char* side, const String& source) {
        const SizeT chunkCount = std::min<SizeT>(
            (source.size() + kCP2SourceChunkBytes - 1) / kCP2SourceChunkBytes,
            kCP2MaxSourceChunks);
        for (SizeT chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
            const SizeT begin = chunkIndex * kCP2SourceChunkBytes;
            String chunk = source.substr(begin, kCP2SourceChunkBytes);
            for (char& c : chunk) {
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                else if (static_cast<unsigned char>(c) < 32) c = '?';
            }
            MGLOG_I("MOBILEPZ_CP2_SHADER_SOURCE side=%s chunk=%zu/%zu text=%s",
                    side, chunkIndex + 1, chunkCount, chunk.c_str());
        }
        if (source.size() > kCP2MaxSourceChunks * kCP2SourceChunkBytes) {
            MGLOG_I("MOBILEPZ_CP2_SHADER_SOURCE side=%s truncated=1 original_bytes=%zu logged_bytes=%zu",
                    side, source.size(), kCP2MaxSourceChunks * kCP2SourceChunkBytes);
        }
    }

    static String CP2FrontendVertexSource(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        if (!program) return {};
        for (const auto& shader : program->GetAttachedShaders()) {
            if (shader && shader->GetShaderStage() == ShaderStage::Vertex) {
                return shader->GetShaderSource();
            }
        }
        return {};
    }

    static Bool CP2HasCP1VertexSemantic(const String& source) {
        return CP2Contains(source, "ModelViewProjection") &&
            CP2Contains(source, "MatrixPalette") &&
            CP2Contains(source, "FinalScale") &&
            CP2Contains(source, "boneEffect") &&
            CP2Contains(source, "gl_Position");
    }

    static void CP2LogTargetLayout(
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao,
        const SharedPtr<MG_State::GLState::BufferObject>& ebo) {
        MGLOG_I("MOBILEPZ_CP2_TARGET_LAYOUT front_program=%u front_vao=%u front_ebo=%u "
                "enabled_mask=0x%x attribute_count=%zu",
                program ? program->GetExternalIndex() : 0,
                vao ? vao->GetExternalIndex() : 0,
                ebo ? ebo->GetExternalIndex() : 0,
                CP2EnabledAttributeMask(vao),
                vao ? vao->GetAllAttributes().size() : 0);
        if (!vao) return;
        const auto& attributes = vao->GetAllAttributes();
        for (SizeT i = 0; i < attributes.size() && i < 8; ++i) {
            const auto& attribute = attributes[i];
            MGLOG_I("MOBILEPZ_CP2_TARGET_ATTRIBUTE location=%zu enabled=%d size=%d type=0x%x "
                    "integer=%d normalized=%d stride=%zu offset=%zu front_buffer=%u",
                    i, attribute.Enabled ? 1 : 0, attribute.Size,
                    static_cast<Uint>(attribute.Type), attribute.IsInteger ? 1 : 0,
                    attribute.Normalized ? 1 : 0,
                    static_cast<SizeT>(attribute.Stride),
                    static_cast<SizeT>(attribute.Offset),
                    attribute.Buffer ? attribute.Buffer->GetExternalIndex() : 0);
        }
    }

    static CP2LayoutProgramCandidate* CP2ObserveLayoutProgram(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        if (!program) return nullptr;
        const auto* identity = program.get();
        const Uint64 lifetime = program->GetLifetimeId();
        const Uint link = program->GetLinkVersion();
        if (g_cp2LastLayoutProgram == identity &&
            g_cp2LastLayoutProgramSlot < g_cp2LayoutProgramCount) {
            auto& candidate = g_cp2LayoutPrograms[g_cp2LastLayoutProgramSlot];
            if (candidate.lifetime == lifetime && candidate.link == link) {
                ++candidate.draws;
                return &candidate;
            }
        }
        for (SizeT i = 0; i < g_cp2LayoutProgramCount; ++i) {
            auto& candidate = g_cp2LayoutPrograms[i];
            if (candidate.identity == identity && candidate.lifetime == lifetime &&
                candidate.link == link) {
                ++candidate.draws;
                g_cp2LastLayoutProgram = identity;
                g_cp2LastLayoutProgramSlot = i;
                return &candidate;
            }
        }
        if (g_cp2LayoutProgramCount >= kCP2MaxLayoutPrograms) {
            ++g_cp2LayoutProgramOverflow;
            return nullptr;
        }
        const SizeT slot = g_cp2LayoutProgramCount++;
        auto& candidate = g_cp2LayoutPrograms[slot];
        candidate.identity = identity;
        candidate.lifetime = lifetime;
        candidate.link = link;
        candidate.external = program->GetExternalIndex();
        candidate.draws = 1;
        candidate.uniformSurface = CP2MatchesUniformSurface(program);
        const String source = CP2FrontendVertexSource(program);
        candidate.sourceSemantic = CP2HasCP1VertexSemantic(source);
        candidate.sourceHash = CP2HashBytes(source.data(), source.size());
        candidate.sourceBytes = source.size();
        g_cp2LastLayoutProgram = identity;
        g_cp2LastLayoutProgramSlot = slot;

        const Int mvp = program->GetUniformLocation("ModelViewProjection");
        const Int palette = program->GetUniformLocation("MatrixPalette[0]");
        const Int finalScale = program->GetUniformLocation("FinalScale");
        const Int color = program->GetUniformLocation("u_color");
        const Int texture = program->GetUniformLocation("Texture");
        const Int paletteIndex = program->GetActiveUniformIndex("MatrixPalette[0]");
        MGLOG_I("MOBILEPZ_CP2_LAYOUT_PROGRAM slot=%zu front_program=%u lifetime=%llu link=%u "
                "active_uniforms=%u mvp_loc=%d palette_loc=%d palette_size=%d "
                "final_scale_loc=%d color_loc=%d texture_loc=%d uniform_surface=%d "
                "source_semantic=%d source_bytes=%zu source_hash=%llx",
                slot, candidate.external, static_cast<unsigned long long>(lifetime), link,
                program->GetUniformCount(), mvp, palette,
                paletteIndex >= 0
                    ? program->GetActiveUniformArraySize(static_cast<Uint>(paletteIndex)) : 0,
                finalScale, color, texture, candidate.uniformSurface ? 1 : 0,
                candidate.sourceSemantic ? 1 : 0, candidate.sourceBytes,
                static_cast<unsigned long long>(candidate.sourceHash));
        return &candidate;
    }

    static void CP2LogLayoutProgramSummary(const char* reason) {
        for (SizeT i = 0; i < g_cp2LayoutProgramCount; ++i) {
            const auto& candidate = g_cp2LayoutPrograms[i];
            MGLOG_I("MOBILEPZ_CP2_LAYOUT_PROGRAM_SUMMARY reason=%s slot=%zu front_program=%u "
                    "lifetime=%llu link=%u draws=%llu uniform_surface=%d source_semantic=%d "
                    "source_bytes=%zu source_hash=%llx",
                    reason ? reason : "unknown", i, candidate.external,
                    static_cast<unsigned long long>(candidate.lifetime), candidate.link,
                    static_cast<unsigned long long>(candidate.draws),
                    candidate.uniformSurface ? 1 : 0, candidate.sourceSemantic ? 1 : 0,
                    candidate.sourceBytes,
                    static_cast<unsigned long long>(candidate.sourceHash));
        }
        MGLOG_I("MOBILEPZ_CP2_LAYOUT_PROGRAM_SUMMARY_END reason=%s programs=%zu overflow=%llu",
                reason ? reason : "unknown", g_cp2LayoutProgramCount,
                static_cast<unsigned long long>(g_cp2LayoutProgramOverflow));
    }

    static String CP2DriverVertexSource(GLuint backendProgram, Bool& complete) {
        complete = false;
        if (backendProgram == 0 || !g_GLESFuncs.glGetProgramiv ||
            !g_GLESFuncs.glGetAttachedShaders || !g_GLESFuncs.glGetShaderiv ||
            !g_GLESFuncs.glGetShaderSource) {
            return {};
        }
        GLint attachedCount = 0;
        g_GLESFuncs.glGetProgramiv(backendProgram, GL_ATTACHED_SHADERS, &attachedCount);
        if (attachedCount <= 0 || attachedCount > 16) return {};
        Vector<GLuint> shaders(static_cast<SizeT>(attachedCount));
        GLsizei actualCount = 0;
        g_GLESFuncs.glGetAttachedShaders(backendProgram, attachedCount, &actualCount, shaders.data());
        actualCount = std::clamp<GLsizei>(actualCount, 0, attachedCount);
        for (GLsizei i = 0; i < actualCount; ++i) {
            GLint type = 0;
            GLint sourceLength = 0;
            g_GLESFuncs.glGetShaderiv(shaders[static_cast<SizeT>(i)], GL_SHADER_TYPE, &type);
            if (type != GL_VERTEX_SHADER) continue;
            g_GLESFuncs.glGetShaderiv(shaders[static_cast<SizeT>(i)], GL_SHADER_SOURCE_LENGTH,
                                      &sourceLength);
            if (sourceLength <= 1 || sourceLength > 1024 * 1024) return {};
            Vector<GLchar> bytes(static_cast<SizeT>(sourceLength) + 1, '\0');
            GLsizei written = 0;
            g_GLESFuncs.glGetShaderSource(shaders[static_cast<SizeT>(i)], sourceLength,
                                          &written, bytes.data());
            written = std::clamp<GLsizei>(written, 0, sourceLength);
            complete = written > 0;
            return complete ? String(bytes.data(), static_cast<SizeT>(written)) : String{};
        }
        return {};
    }

    static Bool CP2EnsureScratchBuffer() {
        if (g_cp2ScratchBuffer != 0) return true;
        if (!g_GLESFuncs.glGenBuffers || !g_GLESFuncs.glBindBuffer ||
            !g_GLESFuncs.glBufferData || !g_GLESFuncs.glGetIntegerv) {
            return false;
        }
        GLint previousWrite = 0;
        g_GLESFuncs.glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &previousWrite);
        g_GLESFuncs.glGenBuffers(1, &g_cp2ScratchBuffer);
        if (g_cp2ScratchBuffer == 0) return false;
        g_GLESFuncs.glBindBuffer(GL_COPY_WRITE_BUFFER, g_cp2ScratchBuffer);
        g_GLESFuncs.glBufferData(GL_COPY_WRITE_BUFFER,
                                 static_cast<GLsizeiptr>(kCP2MaxDriverReadBytes),
                                 nullptr, GL_STREAM_READ);
        g_GLESFuncs.glBindBuffer(GL_COPY_WRITE_BUFFER, static_cast<GLuint>(previousWrite));
        return true;
    }

    static CP2DriverBufferSnapshot CP2ReadBoundGlobalUbo(SizeT bytesToRead) {
        CP2DriverBufferSnapshot result;
        if (bytesToRead == 0 || bytesToRead > kCP2MaxDriverReadBytes ||
            !g_GLESFuncs.glGetIntegeri_v || !g_GLESFuncs.glGetInteger64i_v ||
            !g_GLESFuncs.glGetIntegerv || !g_GLESFuncs.glCopyBufferSubData ||
            !g_GLESFuncs.glMapBufferRange || !g_GLESFuncs.glUnmapBuffer ||
            !CP2EnsureScratchBuffer()) {
            return result;
        }
        g_GLESFuncs.glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &result.buffer);
        g_GLESFuncs.glGetInteger64i_v(GL_UNIFORM_BUFFER_START, 0, &result.start);
        g_GLESFuncs.glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, 0, &result.range);
        if (result.buffer <= 0 || result.start < 0 || result.range < 0 ||
            static_cast<Uint64>(result.range) < bytesToRead) {
            return result;
        }

        GLint previousRead = 0;
        GLint previousWrite = 0;
        g_GLESFuncs.glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &previousRead);
        g_GLESFuncs.glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &previousWrite);
        g_GLESFuncs.glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(result.buffer));
        g_GLESFuncs.glBindBuffer(GL_COPY_WRITE_BUFFER, g_cp2ScratchBuffer);
        g_GLESFuncs.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                        static_cast<GLintptr>(result.start), 0,
                                        static_cast<GLsizeiptr>(bytesToRead));
        const void* mapped = g_GLESFuncs.glMapBufferRange(
            GL_COPY_WRITE_BUFFER, 0, static_cast<GLsizeiptr>(bytesToRead), GL_MAP_READ_BIT);
        if (mapped) {
            result.bytes.resize(bytesToRead);
            std::memcpy(result.bytes.data(), mapped, bytesToRead);
            result.readable = true;
            g_GLESFuncs.glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        }
        g_GLESFuncs.glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousRead));
        g_GLESFuncs.glBindBuffer(GL_COPY_WRITE_BUFFER, static_cast<GLuint>(previousWrite));
        return result;
    }

    static CP2BackendUniformLayout CP2ReflectBackendUniform(
        GLuint backendProgram, const char* const* names, SizeT nameCount) {
        CP2BackendUniformLayout layout;
        if (!g_GLESFuncs.glGetUniformIndices || !g_GLESFuncs.glGetActiveUniformsiv) return layout;
        for (SizeT i = 0; i < nameCount; ++i) {
            const GLchar* name = names[i];
            GLuint index = GL_INVALID_INDEX;
            g_GLESFuncs.glGetUniformIndices(backendProgram, 1, &name, &index);
            if (index == GL_INVALID_INDEX) continue;
            layout.found = true;
            layout.selectedName = names[i];
            layout.index = index;
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_OFFSET, &layout.offset);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_BLOCK_INDEX, &layout.blockIndex);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_TYPE, &layout.type);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_SIZE, &layout.size);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_ARRAY_STRIDE, &layout.arrayStride);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_MATRIX_STRIDE, &layout.matrixStride);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_IS_ROW_MAJOR, &layout.rowMajor);
            break;
        }
        return layout;
    }

    static void CP2LogFrontendInventory(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        MGLOG_I("MOBILEPZ_CP2_FRONT_UNIFORM_SUMMARY program=%u active=%u max_location=%u "
                "ubo_size=%u ubo_version=%u",
                program->GetExternalIndex(), program->GetUniformCount(),
                program->GetMaxUniformLocation(), program->GetUBOSize(),
                program->GetUBOContentVersion());
        for (Uint index = 0; index < program->GetUniformCount(); ++index) {
            const String& name = program->GetActiveUniformName(index);
            const Int location = program->GetUniformLocation(name);
            const Uint offset = location >= 0
                ? program->GetUniformOffset(static_cast<Uint>(location))
                : MG_State::GLState::ProgramObject::kInvalidUniformOffset;
            MGLOG_I("MOBILEPZ_CP2_FRONT_UNIFORM index=%u name=%s type=0x%x size=%d "
                    "location=%d shadow_offset=%u opaque=%d",
                    index, name.c_str(), program->GetActiveUniformType(index),
                    program->GetActiveUniformArraySize(index), location, offset,
                    location >= 0 && program->IsUniformOpaqueAtLocation(static_cast<Uint>(location)) ? 1 : 0);
        }
    }

    static Bool CP2LogDriverInventory(GLuint backendProgram) {
        if (!g_GLESFuncs.glGetProgramiv || !g_GLESFuncs.glGetActiveUniform ||
            !g_GLESFuncs.glGetActiveUniformsiv || !g_GLESFuncs.glGetUniformLocation) {
            MGLOG_I("MOBILEPZ_CP2_DRIVER_UNIFORM_SUMMARY status=query_unavailable");
            return false;
        }
        GLint count = 0;
        GLint maxLength = 0;
        g_GLESFuncs.glGetProgramiv(backendProgram, GL_ACTIVE_UNIFORMS, &count);
        g_GLESFuncs.glGetProgramiv(backendProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
        count = std::clamp<GLint>(count, 0, 4096);
        maxLength = std::clamp<GLint>(maxLength, 1, 4096);
        MGLOG_I("MOBILEPZ_CP2_DRIVER_UNIFORM_SUMMARY backend_program=%u active=%d max_name=%d",
                backendProgram, count, maxLength);
        Vector<GLchar> name(static_cast<SizeT>(maxLength) + 1, '\0');
        for (GLint i = 0; i < count; ++i) {
            GLsizei written = 0;
            GLint size = 0;
            GLenum type = 0;
            std::fill(name.begin(), name.end(), '\0');
            g_GLESFuncs.glGetActiveUniform(backendProgram, static_cast<GLuint>(i), maxLength,
                                           &written, &size, &type, name.data());
            const GLuint index = static_cast<GLuint>(i);
            GLint block = -1, offset = -1, arrayStride = -1, matrixStride = -1, rowMajor = -1;
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_BLOCK_INDEX, &block);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_OFFSET, &offset);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_ARRAY_STRIDE, &arrayStride);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_MATRIX_STRIDE, &matrixStride);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index,
                                               GL_UNIFORM_IS_ROW_MAJOR, &rowMajor);
            const GLint location = g_GLESFuncs.glGetUniformLocation(backendProgram, name.data());
            MGLOG_I("MOBILEPZ_CP2_DRIVER_UNIFORM index=%d name=%s type=0x%x size=%d "
                    "location=%d block=%d offset=%d array_stride=%d matrix_stride=%d row_major=%d",
                    i, name.data(), type, size, location, block, offset,
                    arrayStride, matrixStride, rowMajor);
        }
        return true;
    }

    static Bool CP2CompareMember(
        const char* label, const char* frontendName, const char* frontendFallback,
        const char* const* backendNames, SizeT backendNameCount,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        GLuint backendProgram, const CP2DriverBufferSnapshot& driver,
        Bool& captureComplete) {
        Int frontLocation = program->GetUniformLocation(frontendName);
        const char* selectedFrontName = frontendName;
        if (frontLocation < 0 && frontendFallback) {
            frontLocation = program->GetUniformLocation(frontendFallback);
            selectedFrontName = frontendFallback;
        }
        const Bool frontFound = frontLocation >= 0 && program->IsValidUniformLocation(frontLocation);
        const Int frontIndex = frontFound ? program->GetActiveUniformIndex(selectedFrontName) : -1;
        const GLint frontType = frontFound
            ? static_cast<GLint>(program->GetUniformType(static_cast<Uint>(frontLocation))) : 0;
        const GLint frontArraySize = frontIndex >= 0
            ? program->GetActiveUniformArraySize(static_cast<Uint>(frontIndex)) : 0;
        const Uint frontOffset = frontFound
            ? program->GetUniformOffset(static_cast<Uint>(frontLocation))
            : MG_State::GLState::ProgramObject::kInvalidUniformOffset;
        const SizeT frontElementSpan = frontFound
            ? program->GetUniformStorageSpanInBytes(static_cast<Uint>(frontLocation)) : 0;
        GLint frontArrayStride = 0;
        if (frontArraySize > 1 && program->IsValidUniformLocation(frontLocation + 1)) {
            const Uint nextOffset = program->GetUniformOffset(static_cast<Uint>(frontLocation + 1));
            if (frontOffset != MG_State::GLState::ProgramObject::kInvalidUniformOffset &&
                nextOffset != MG_State::GLState::ProgramObject::kInvalidUniformOffset &&
                nextOffset > frontOffset) {
                frontArrayStride = static_cast<GLint>(nextOffset - frontOffset);
            }
        }
        const GLint frontMatrixStride = frontType == GL_FLOAT_MAT4 ? 16 : 0;
        SizeT frontSpan = frontElementSpan;
        if (frontArraySize > 1 && frontArrayStride > 0) {
            frontSpan = static_cast<SizeT>(frontArrayStride) *
                static_cast<SizeT>(frontArraySize - 1) + frontElementSpan;
        }

        const CP2BackendUniformLayout backend = CP2ReflectBackendUniform(
            backendProgram, backendNames, backendNameCount);
        SizeT backendElementSpan = 0;
        if (backend.type == GL_FLOAT_MAT4 && backend.matrixStride > 0) {
            backendElementSpan = static_cast<SizeT>(backend.matrixStride) * 4;
        } else if (backend.type == GL_FLOAT) {
            backendElementSpan = sizeof(GLfloat);
        } else if (backend.type == GL_FLOAT_VEC4) {
            backendElementSpan = sizeof(GLfloat) * 4;
        }
        SizeT backendSpan = backendElementSpan;
        if (backend.size > 1 && backend.arrayStride > 0) {
            backendSpan = static_cast<SizeT>(backend.arrayStride) *
                static_cast<SizeT>(backend.size - 1) + backendElementSpan;
        }

        const Bool layoutMatch = frontFound && backend.found &&
            frontOffset != MG_State::GLState::ProgramObject::kInvalidUniformOffset &&
            backend.offset >= 0 && static_cast<SizeT>(frontOffset) == static_cast<SizeT>(backend.offset) &&
            frontType == backend.type && frontArraySize == backend.size &&
            (frontArraySize <= 1 || frontArrayStride == backend.arrayStride) &&
            (frontType != GL_FLOAT_MAT4 || backend.matrixStride == frontMatrixStride) &&
            backend.rowMajor == 0 && frontSpan == backendSpan;

        const auto* frontBytes = static_cast<const Uint8*>(program->GetUBOData());
        const SizeT frontUboSize = program->GetUBOSize();
        Bool contentReadable = driver.readable && frontBytes && frontSpan > 0 && backendSpan > 0 &&
            frontOffset != MG_State::GLState::ProgramObject::kInvalidUniformOffset &&
            backend.offset >= 0 && static_cast<SizeT>(frontOffset) <= frontUboSize &&
            frontSpan <= frontUboSize - static_cast<SizeT>(frontOffset) &&
            static_cast<SizeT>(backend.offset) <= driver.bytes.size() &&
            backendSpan <= driver.bytes.size() - static_cast<SizeT>(backend.offset);
        const SizeT comparedBytes = contentReadable ? std::min(frontSpan, backendSpan) : 0;
        const Uint8* frontMember = comparedBytes
            ? frontBytes + static_cast<SizeT>(frontOffset) : nullptr;
        const Uint8* driverMember = comparedBytes
            ? driver.bytes.data() + static_cast<SizeT>(backend.offset) : nullptr;
        const Bool contentMatch = comparedBytes > 0 &&
            std::memcmp(frontMember, driverMember, comparedBytes) == 0;
        if (!contentReadable) captureComplete = false;

        MGLOG_I("MOBILEPZ_CP2_MEMBER label=%s front_name=%s front_found=%d front_location=%d "
                "front_type=0x%x front_offset=%u front_size=%d front_array_stride=%d "
                "front_matrix_stride=%d front_span=%zu backend_name=%s backend_found=%d "
                "backend_index=%u backend_block=%d backend_type=0x%x backend_offset=%d "
                "backend_size=%d backend_array_stride=%d backend_matrix_stride=%d "
                "backend_row_major=%d backend_span=%zu layout_match=%d content_readable=%d "
                "content_match=%d compared_bytes=%zu front_hash=%llx driver_hash=%llx",
                label, selectedFrontName, frontFound ? 1 : 0, frontLocation, frontType,
                frontOffset, frontArraySize, frontArrayStride, frontMatrixStride, frontSpan,
                backend.selectedName.c_str(), backend.found ? 1 : 0, backend.index,
                backend.blockIndex, backend.type, backend.offset, backend.size,
                backend.arrayStride, backend.matrixStride, backend.rowMajor, backendSpan,
                layoutMatch ? 1 : 0, contentReadable ? 1 : 0, contentMatch ? 1 : 0,
                comparedBytes,
                static_cast<unsigned long long>(CP2HashBytes(frontMember, comparedBytes)),
                static_cast<unsigned long long>(CP2HashBytes(driverMember, comparedBytes)));
        return layoutMatch && contentMatch;
    }

    static Bool CP2CompareSampler(
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        GLuint backendProgram, Bool& captureComplete) {
        const Int frontLocation = program->GetUniformLocation("Texture");
        const Int frontUnit = frontLocation >= 0
            ? program->GetUniformSamplerOrImageUnitIndex(static_cast<Uint>(frontLocation)) : -1;
        GLint backendLocation = -1;
        GLint backendUnit = -1;
        if (g_GLESFuncs.glGetUniformLocation && g_GLESFuncs.glGetUniformiv) {
            backendLocation = g_GLESFuncs.glGetUniformLocation(backendProgram, "Texture");
            if (backendLocation >= 0) {
                g_GLESFuncs.glGetUniformiv(backendProgram, backendLocation, &backendUnit);
            }
        }
        const Bool complete = frontLocation >= 0 && backendLocation >= 0;
        const Bool match = complete && frontUnit == backendUnit;
        if (!complete) captureComplete = false;
        MGLOG_I("MOBILEPZ_CP2_SAMPLER name=Texture front_location=%d front_unit=%d "
                "backend_location=%d backend_unit=%d match=%d",
                frontLocation, frontUnit, backendLocation, backendUnit, match ? 1 : 0);
        return match;
    }

    static Bool CP2CompareShaderInputs(
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        GLuint backendProgram, Bool& captureComplete) {
        if (!g_GLESFuncs.glGetAttribLocation) {
            captureComplete = false;
            MGLOG_I("MOBILEPZ_CP2_SHADER_INPUT_SUMMARY status=query_unavailable");
            return false;
        }
        Bool allMatch = true;
        for (Uint location = 0; location < 5; ++location) {
            const String& name = program->GetAttribName(location);
            const GLenum frontType = program->GetAttribType(location);
            const GLint driverLocation = name.empty()
                ? -1 : g_GLESFuncs.glGetAttribLocation(backendProgram, name.c_str());
            const Bool match = !name.empty() && driverLocation == static_cast<GLint>(location);
            allMatch = allMatch && match;
            MGLOG_I("MOBILEPZ_CP2_SHADER_INPUT location=%u name=%s front_type=0x%x "
                    "driver_location=%d match=%d",
                    location, name.c_str(), frontType, driverLocation, match ? 1 : 0);
        }
        return allMatch;
    }

    static void CP2Capture(
        const char* entry, GLenum mode, GLsizei count, GLenum indexType,
        const void* indices, const char* selector,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao,
        const SharedPtr<MG_State::GLState::BufferObject>& ebo) {
        auto* backend = GetCurrentBackendProgram();
        const GLuint backendId = backend ? backend->GetBackendProgramId() : 0;
        GLint driverCurrentProgram = 0;
        if (g_GLESFuncs.glGetIntegerv) {
            g_GLESFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &driverCurrentProgram);
        }
        const auto& attributes = vao->GetAllAttributes();
        const auto& vbo = attributes[0].Buffer;
        auto* vboResource = vbo ? BufferImpl::GetBufferResource(vbo.get()) : nullptr;
        auto* eboResource = ebo ? BufferImpl::GetBufferResource(ebo.get()) : nullptr;

        MGLOG_I("MOBILEPZ_CP2_TARGET entry=%s mode=0x%x count=%d index_type=0x%x "
                "index_offset=%zu front_program=%u front_lifetime=%llu front_link=%u "
                "backend_program=%u driver_current=%d front_vao=%u front_vbo=%u native_vbo=%u "
                "front_ebo=%u native_ebo=%u attrs=0x%x stride=64 selector=%s "
                "rendering_mutation=0",
                entry ? entry : "unknown", mode, count, indexType,
                reinterpret_cast<SizeT>(indices), program->GetExternalIndex(),
                static_cast<unsigned long long>(program->GetLifetimeId()),
                program->GetLinkVersion(), backendId, driverCurrentProgram,
                vao->GetExternalIndex(), vbo ? vbo->GetExternalIndex() : 0,
                vboResource ? vboResource->id : 0, ebo ? ebo->GetExternalIndex() : 0,
                eboResource ? eboResource->id : 0, kCP2TargetAttributeMask,
                selector ? selector : "unknown");

        Bool captureComplete = backend != nullptr && backendId != 0 &&
            driverCurrentProgram == static_cast<GLint>(backendId);
        Bool uniformMismatch = false;
        Bool shaderMappingMismatch = false;

        CP2LogFrontendInventory(program);
        if (!CP2LogDriverInventory(backendId)) captureComplete = false;

        const SizeT uboSize = program->GetUBOSize();
        CP2DriverBufferSnapshot driverUbo = CP2ReadBoundGlobalUbo(uboSize);
        const auto* frontUbo = static_cast<const Uint8*>(program->GetUBOData());
        const Bool wholeUboMatch = driverUbo.readable && frontUbo &&
            driverUbo.bytes.size() == uboSize &&
            std::memcmp(frontUbo, driverUbo.bytes.data(), uboSize) == 0;
        if (!driverUbo.readable) captureComplete = false;
        MGLOG_I("MOBILEPZ_CP2_UBO_SUMMARY front_size=%zu backend_block_size=%d "
                "driver_buffer=%d driver_start=%lld driver_range=%lld readable=%d "
                "whole_content_match=%d front_hash=%llx driver_hash=%llx content_version=%u",
                uboSize, backend ? backend->GetGlobalUboBackendBlockSize() : -1,
                driverUbo.buffer, static_cast<long long>(driverUbo.start),
                static_cast<long long>(driverUbo.range), driverUbo.readable ? 1 : 0,
                wholeUboMatch ? 1 : 0,
                static_cast<unsigned long long>(CP2HashBytes(frontUbo, uboSize)),
                static_cast<unsigned long long>(CP2HashBytes(
                    driverUbo.bytes.data(), driverUbo.bytes.size())),
                program->GetUBOContentVersion());
        if (driverUbo.readable && !wholeUboMatch) uniformMismatch = true;

        static constexpr const char* mvpBackendNames[] = {
            "MGL_GLOBAL_UBO.ModelViewProjection", "ModelViewProjection"
        };
        static constexpr const char* paletteBackendNames[] = {
            "MGL_GLOBAL_UBO.MatrixPalette[0]", "MGL_GLOBAL_UBO.MatrixPalette",
            "MatrixPalette[0]", "MatrixPalette"
        };
        static constexpr const char* finalScaleBackendNames[] = {
            "MGL_GLOBAL_UBO.FinalScale", "FinalScale"
        };
        static constexpr const char* colorBackendNames[] = {
            "MGL_GLOBAL_UBO.u_color", "u_color"
        };
        if (!CP2CompareMember("ModelViewProjection", "ModelViewProjection", nullptr,
                              mvpBackendNames, std::size(mvpBackendNames), program,
                              backendId, driverUbo, captureComplete)) {
            uniformMismatch = true;
        }
        if (!CP2CompareMember("MatrixPalette", "MatrixPalette[0]", "MatrixPalette",
                              paletteBackendNames, std::size(paletteBackendNames), program,
                              backendId, driverUbo, captureComplete)) {
            uniformMismatch = true;
        }
        if (!CP2CompareMember("FinalScale", "FinalScale", nullptr,
                              finalScaleBackendNames, std::size(finalScaleBackendNames), program,
                              backendId, driverUbo, captureComplete)) {
            uniformMismatch = true;
        }
        if (!CP2CompareMember("u_color", "u_color", nullptr,
                              colorBackendNames, std::size(colorBackendNames), program,
                              backendId, driverUbo, captureComplete)) {
            uniformMismatch = true;
        }
        if (!CP2CompareSampler(program, backendId, captureComplete)) uniformMismatch = true;

        if (!CP2CompareShaderInputs(program, backendId, captureComplete)) {
            shaderMappingMismatch = true;
        }
        const String frontendVertexSource = CP2FrontendVertexSource(program);
        Bool driverSourceComplete = false;
        const String driverVertexSource = CP2DriverVertexSource(backendId, driverSourceComplete);
        const Bool frontendSourceComplete = !frontendVertexSource.empty();
        if (!frontendSourceComplete || !driverSourceComplete) captureComplete = false;
        const Bool frontendSemantic =
            CP2Contains(frontendVertexSource, "ModelViewProjection") &&
            CP2Contains(frontendVertexSource, "MatrixPalette") &&
            CP2Contains(frontendVertexSource, "FinalScale") &&
            CP2Contains(frontendVertexSource, "gl_Position");
        const Bool driverSemantic =
            CP2Contains(driverVertexSource, "ModelViewProjection") &&
            CP2Contains(driverVertexSource, "MatrixPalette") &&
            CP2Contains(driverVertexSource, "FinalScale") &&
            CP2Contains(driverVertexSource, "gl_Position") &&
            CP2Contains(driverVertexSource, "MGL_GLOBAL_UBO");
        if (driverSourceComplete && !driverSemantic) shaderMappingMismatch = true;
        MGLOG_I("MOBILEPZ_CP2_SHADER_SUMMARY front_bytes=%zu front_hash=%llx front_complete=%d "
                "front_semantic=%d driver_bytes=%zu driver_hash=%llx driver_complete=%d "
                "driver_semantic=%d front_has_boneEffect=%d driver_has_matrix_index=%d",
                frontendVertexSource.size(),
                static_cast<unsigned long long>(CP2HashBytes(
                    frontendVertexSource.data(), frontendVertexSource.size())),
                frontendSourceComplete ? 1 : 0, frontendSemantic ? 1 : 0,
                driverVertexSource.size(),
                static_cast<unsigned long long>(CP2HashBytes(
                    driverVertexSource.data(), driverVertexSource.size())),
                driverSourceComplete ? 1 : 0, driverSemantic ? 1 : 0,
                CP2Contains(frontendVertexSource, "boneEffect") ? 1 : 0,
                CP2Contains(driverVertexSource, "MatrixPalette[") ? 1 : 0);
        CP2LogSourceChunks("frontend_vertex", frontendVertexSource);
        CP2LogSourceChunks("driver_vertex", driverVertexSource);

        const char* verdict = "CP2_FRONTEND_DRIVER_PARITY_CONFIRMED";
        const char* reason = "uniform_bytes_layout_sampler_and_shader_mapping_match";
        if (!captureComplete) {
            verdict = "CP2_CAPTURE_INCOMPLETE";
            reason = "required_native_query_or_readback_unavailable";
        } else if (uniformMismatch) {
            verdict = "CP2_DRIVER_UNIFORM_MISMATCH";
            reason = "uniform_layout_or_bound_value_diverged";
        } else if (shaderMappingMismatch) {
            verdict = "CP2_DRIVER_SHADER_MAPPING_MISMATCH";
            reason = "attribute_or_translated_vertex_semantic_diverged";
        }
        MGLOG_I("MOBILEPZ_CP2_RESULT verdict=%s reason=%s front_program=%u "
                "backend_program=%u count=%d uniform_mismatch=%d shader_mapping_mismatch=%d "
                "capture_complete=%d wait_boundaries=%llu seen_draws=%llu rendering_mutation=0",
                verdict, reason, program->GetExternalIndex(), backendId, count,
                uniformMismatch ? 1 : 0, shaderMappingMismatch ? 1 : 0,
                captureComplete ? 1 : 0,
                static_cast<unsigned long long>(g_cp2WaitBoundaries),
                static_cast<unsigned long long>(g_cp2SeenDraws));
        g_cp2State = CP2State::Done;
    }

    static void CP2RecordDraw(const char* entry, GLenum mode, GLsizei count,
                              GLenum indexType, const void* indices, GLint baseVertex) {
        if (g_cp2State == CP2State::Dormant && !g_cp2EarlyArmChecked) {
            g_cp2EarlyArmChecked = true;
            CP2TryArm("draw");
        }
        if (g_cp2State != CP2State::Armed) return;
        ++g_cp2SeenDraws;
        if (mode != GL_TRIANGLES || count <= 0) {
            ++g_cp2RejectPrimitive;
            ++g_cp2RejectLayout;
            return;
        }
        const auto& program = MG_State::pGLContext->GetProgramForDraw();
        const auto& vao = MG_State::pGLContext->GetBoundVertexArray();
        const auto ebo = vao ? vao->GetIndexBufferBindingSlot().GetBoundObject() : nullptr;

        // CP2R5 correction: identify the CP1 shader semantically before applying
        // the exact historical VAO/index gate.  Program numbers and live VAO
        // layouts are not stable across fresh worlds or instrumentation timing.
        CP2LayoutProgramCandidate* candidate = CP2ObserveLayoutProgram(program);
        if (candidate && candidate->sourceSemantic) {
            if (!vao || !ebo || vao->GetAllAttributes().empty()) {
                ++g_cp2RejectMissingBinding;
                ++g_cp2RejectLayout;
                return;
            }
            CP2LogTargetLayout(program, vao, ebo);
            CP2Capture(entry, mode, count, indexType, indices,
                       "cp1_vertex_source_semantic_pre_layout",
                       program, vao, ebo);
            return;
        }

        if (indexType != GL_UNSIGNED_INT) {
            ++g_cp2RejectIndexType;
            ++g_cp2RejectLayout;
            return;
        }
        if (baseVertex != 0) {
            ++g_cp2RejectBaseVertex;
            ++g_cp2RejectLayout;
            return;
        }
        const char* layoutReject = CP2LayoutRejectReason(vao, ebo);
        if (layoutReject) {
            ++g_cp2RejectLayout;
            if (std::strcmp(layoutReject, "missing_vao") == 0 ||
                std::strcmp(layoutReject, "missing_ebo") == 0) {
                ++g_cp2RejectMissingBinding;
            } else if (std::strcmp(layoutReject, "enabled_mask") == 0) {
                ++g_cp2RejectEnabledMask;
            } else if (std::strcmp(layoutReject, "different_vbo") == 0) {
                ++g_cp2RejectSharedVbo;
            } else {
                ++g_cp2RejectAttribute;
            }
            CP2LogNearLayout(layoutReject, mode, count, indexType, baseVertex,
                             program, vao, ebo);
            return;
        }
        const Bool uniformSurface = candidate && candidate->uniformSurface;
        if (!uniformSurface) {
            ++g_cp2RejectUniformSurface;
            CP2LogNearUniformSurface(program);
            return;
        }
        CP2LogTargetLayout(program, vao, ebo);
        CP2Capture(entry, mode, count, indexType, indices,
                   "exact_cp1_layout_and_uniform_surface",
                   program, vao, ebo);
    }
} // namespace
