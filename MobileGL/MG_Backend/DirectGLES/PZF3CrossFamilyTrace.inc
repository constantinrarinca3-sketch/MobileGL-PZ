// MobilePZ PZF3 - bounded, read-only cross-family shader/draw route tracer.
// Included inside namespace MobileGL::MG_Backend::DirectGLES after PrepareForDraw.

namespace {
    enum class PZF3Family : Uint8 {
        Skinned = 0,
        StaticModel,
        WorldChunk,
        Textured,
        Other,
        Count,
    };

    constexpr SizeT kPZF3FamilyCount = static_cast<SizeT>(PZF3Family::Count);
    constexpr SizeT kPZF3MaxPrograms = 192;
    constexpr Uint64 kPZF3MaxDrawHeaders = 1200;
    constexpr Uint64 kPZF3MaxDetailedCaptures = 80;
    constexpr SizeT kPZF3MaxSourceBytes = 16u * 1024u;
    constexpr SizeT kPZF3SourceChunkBytes = 560;

    struct PZF3Features {
        Bool matrixPalette = false;
        Bool boneEffect = false;
        Bool transform = false;
        Bool finalScale = false;
        Bool chunkDepth = false;
        Bool zDepth = false;
        Bool useTexture = false;
        Bool diffuse = false;
        Bool mvp = false;
        Bool sampler = false;
        Bool alpha = false;
    };

    struct PZF3ProgramRecord {
        const MG_State::GLState::ProgramObject* identity = nullptr;
        Uint64 lifetime = 0;
        Uint32 link = 0;
        Uint external = 0;
        PZF3Family family = PZF3Family::Other;
        PZF3Features features{};
        Uint64 draws = 0;
        Bool inventoryLogged = false;
        Bool detailedLogged = false;
    };

    static Array<PZF3ProgramRecord, kPZF3MaxPrograms> g_pzf3Programs{};
    static SizeT g_pzf3ProgramCount = 0;
    static Uint64 g_pzf3ProgramOverflow = 0;
    static const MG_State::GLState::ProgramObject* g_pzf3LastProgram = nullptr;
    static SizeT g_pzf3LastProgramSlot = kPZF3MaxPrograms;
    static Uint64 g_pzf3DrawSerial = 0;
    static Uint64 g_pzf3DrawHeaders = 0;
    static Uint64 g_pzf3DetailedCaptures = 0;
    static Uint64 g_pzf3PresentSerial = 0;
    static Array<Uint64, kPZF3FamilyCount> g_pzf3FamilyPrograms{};
    static Array<Uint64, kPZF3FamilyCount> g_pzf3FamilyDraws{};
    static Array<Uint64, kPZF3FamilyCount> g_pzf3FamilyDetails{};
    static Array<Uint64, kPZF3FamilyCount> g_pzf3FamilySourceDumps{};

    static SizeT PZF3FamilyIndex(PZF3Family family) {
        return static_cast<SizeT>(family);
    }

    static const char* PZF3FamilyName(PZF3Family family) {
        switch (family) {
        case PZF3Family::Skinned: return "SKINNED";
        case PZF3Family::StaticModel: return "STATIC_MODEL";
        case PZF3Family::WorldChunk: return "WORLD_CHUNK";
        case PZF3Family::Textured: return "TEXTURED";
        default: return "OTHER";
        }
    }

    static const char* PZF3StageName(ShaderStage stage) {
        switch (stage) {
        case ShaderStage::Vertex: return "VS";
        case ShaderStage::TessControl: return "TCS";
        case ShaderStage::TessEval: return "TES";
        case ShaderStage::Geometry: return "GS";
        case ShaderStage::Fragment: return "FS";
        case ShaderStage::Compute: return "CS";
        default: return "UNKNOWN";
        }
    }

    static const char* PZF3DriverStageName(GLenum stage) {
        switch (stage) {
        case GL_VERTEX_SHADER: return "VS";
        case GL_FRAGMENT_SHADER: return "FS";
        case GL_GEOMETRY_SHADER: return "GS";
        case GL_TESS_CONTROL_SHADER: return "TCS";
        case GL_TESS_EVALUATION_SHADER: return "TES";
        case GL_COMPUTE_SHADER: return "CS";
        default: return "UNKNOWN";
        }
    }

    static Bool PZF3Contains(const String& source, const char* token) {
        return token && source.find(token) != String::npos;
    }

    static Uint64 PZF3HashBytes(const void* data, SizeT size) {
        if (!data || size == 0) return 0;
        const auto* bytes = static_cast<const Uint8*>(data);
        Uint64 hash = 1469598103934665603ull;
        for (SizeT i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }
        return hash;
    }

    static Uint64 PZF3HashText(const String& source) {
        return PZF3HashBytes(source.data(), source.size());
    }

    static String PZF3FirstLine(const String& input) {
        if (input.empty()) return "<empty>";
        const SizeT newline = input.find_first_of("\r\n");
        const SizeT end = std::min<SizeT>(newline == String::npos ? input.size() : newline, 240);
        String line = input.substr(0, end);
        for (char& c : line) {
            if (static_cast<unsigned char>(c) < 0x20) c = ' ';
        }
        return line;
    }

    static void PZF3AccumulateFeatures(PZF3Features& f, const String& text) {
        f.matrixPalette = f.matrixPalette || PZF3Contains(text, "MatrixPalette");
        f.boneEffect = f.boneEffect || PZF3Contains(text, "boneEffect");
        f.transform = f.transform || PZF3Contains(text, "transform");
        f.finalScale = f.finalScale || PZF3Contains(text, "FinalScale");
        f.chunkDepth = f.chunkDepth || PZF3Contains(text, "chunkDepth");
        f.zDepth = f.zDepth || PZF3Contains(text, "zDepth");
        f.useTexture = f.useTexture || PZF3Contains(text, "useTexture");
        f.diffuse = f.diffuse || PZF3Contains(text, "DIFFUSE");
        f.mvp = f.mvp || PZF3Contains(text, "ModelViewProjection") || PZF3Contains(text, "MVPMatrix");
        f.alpha = f.alpha || PZF3Contains(text, "Alpha") || PZF3Contains(text, "alpha");
    }

    static PZF3Features PZF3InspectFeatures(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        PZF3Features f;
        if (!program) return f;
        for (const auto& shader : program->GetAttachedShaders()) {
            if (shader) PZF3AccumulateFeatures(f, shader->GetShaderSource());
        }
        const Uint uniformCount = program->GetUniformCount();
        for (Uint i = 0; i < uniformCount; ++i) {
            const String& name = program->GetActiveUniformName(i);
            PZF3AccumulateFeatures(f, name);
            const GLenum type = program->GetActiveUniformType(i);
            if (SamplerUniformTextureTarget(type) != TextureTarget::Unknown) f.sampler = true;
        }
        return f;
    }

    static PZF3Family PZF3Classify(const PZF3Features& f) {
        if (f.matrixPalette || f.boneEffect) return PZF3Family::Skinned;
        if (f.chunkDepth || f.zDepth || (f.useTexture && f.diffuse)) return PZF3Family::WorldChunk;
        if (f.transform && (f.finalScale || f.sampler || f.mvp || f.alpha)) return PZF3Family::StaticModel;
        if (f.sampler) return PZF3Family::Textured;
        return PZF3Family::Other;
    }

    static Bool PZF3IsPowerOfTwo(Uint64 value) {
        return value != 0 && (value & (value - 1)) == 0;
    }

    static PZF3ProgramRecord* PZF3ObserveProgram(
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        if (!program) return nullptr;
        const auto* identity = program.get();
        const Uint64 lifetime = program->GetLifetimeId();
        const Uint32 link = program->GetLinkVersion();
        if (g_pzf3LastProgram == identity && g_pzf3LastProgramSlot < g_pzf3ProgramCount) {
            auto& record = g_pzf3Programs[g_pzf3LastProgramSlot];
            if (record.lifetime == lifetime && record.link == link) return &record;
        }
        for (SizeT i = 0; i < g_pzf3ProgramCount; ++i) {
            auto& record = g_pzf3Programs[i];
            if (record.identity == identity && record.lifetime == lifetime && record.link == link) {
                g_pzf3LastProgram = identity;
                g_pzf3LastProgramSlot = i;
                return &record;
            }
        }
        if (g_pzf3ProgramCount >= kPZF3MaxPrograms) {
            ++g_pzf3ProgramOverflow;
            return nullptr;
        }
        const SizeT slot = g_pzf3ProgramCount++;
        auto& record = g_pzf3Programs[slot];
        record.identity = identity;
        record.lifetime = lifetime;
        record.link = link;
        record.external = program->GetExternalIndex();
        record.features = PZF3InspectFeatures(program);
        record.family = PZF3Classify(record.features);
        ++g_pzf3FamilyPrograms[PZF3FamilyIndex(record.family)];
        g_pzf3LastProgram = identity;
        g_pzf3LastProgramSlot = slot;
        return &record;
    }

    static void PZF3LogSourceChunks(const char* side, PZF3Family family, Uint program,
                                    const char* stage, Uint shader, const String& source) {
        const SizeT loggedBytes = std::min(source.size(), kPZF3MaxSourceBytes);
        const SizeT chunks = (loggedBytes + kPZF3SourceChunkBytes - 1) / kPZF3SourceChunkBytes;
        for (SizeT chunkIndex = 0; chunkIndex < chunks; ++chunkIndex) {
            String chunk = source.substr(chunkIndex * kPZF3SourceChunkBytes, kPZF3SourceChunkBytes);
            for (char& c : chunk) {
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                else if (static_cast<unsigned char>(c) < 0x20) c = '?';
            }
            MGLOG_I("MOBILEPZ_PZF3_SHADER_SOURCE side=%s family=%s program=%u stage=%s shader=%u "
                    "chunk=%zu/%zu text=%s", side, PZF3FamilyName(family), program,
                    stage ? stage : "UNKNOWN", shader, chunkIndex + 1, chunks, chunk.c_str());
        }
        if (source.size() > loggedBytes) {
            MGLOG_I("MOBILEPZ_PZF3_SHADER_SOURCE side=%s family=%s program=%u stage=%s shader=%u "
                    "truncated=1 original_bytes=%zu logged_bytes=%zu", side, PZF3FamilyName(family),
                    program, stage ? stage : "UNKNOWN", shader, source.size(), loggedBytes);
        }
    }

    static Bool PZF3ReserveSourceDump(PZF3Family family) {
        static constexpr Array<Uint64, kPZF3FamilyCount> limits = {3, 3, 3, 1, 1};
        const SizeT index = PZF3FamilyIndex(family);
        if (g_pzf3FamilySourceDumps[index] >= limits[index]) return false;
        ++g_pzf3FamilySourceDumps[index];
        return true;
    }

    static String PZF3DriverShaderSource(GLuint shader) {
        if (!shader || !g_GLESFuncs.glGetShaderiv || !g_GLESFuncs.glGetShaderSource) return {};
        GLint length = 0;
        g_GLESFuncs.glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &length);
        length = std::clamp<GLint>(length, 0, 256 * 1024);
        if (length <= 1) return {};
        Vector<GLchar> bytes(static_cast<SizeT>(length) + 1, '\0');
        GLsizei written = 0;
        g_GLESFuncs.glGetShaderSource(shader, length, &written, bytes.data());
        if (written <= 0) return {};
        return String(bytes.data(), static_cast<SizeT>(written));
    }

    static String PZF3DriverShaderInfo(GLuint shader) {
        if (!shader || !g_GLESFuncs.glGetShaderiv || !g_GLESFuncs.glGetShaderInfoLog) return {};
        GLint length = 0;
        g_GLESFuncs.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        length = std::clamp<GLint>(length, 0, 16 * 1024);
        if (length <= 1) return {};
        Vector<GLchar> bytes(static_cast<SizeT>(length) + 1, '\0');
        GLsizei written = 0;
        g_GLESFuncs.glGetShaderInfoLog(shader, length, &written, bytes.data());
        return written > 0 ? String(bytes.data(), static_cast<SizeT>(written)) : String{};
    }

    static String PZF3DriverProgramInfo(GLuint program) {
        if (!program || !g_GLESFuncs.glGetProgramiv || !g_GLESFuncs.glGetProgramInfoLog) return {};
        GLint length = 0;
        g_GLESFuncs.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        length = std::clamp<GLint>(length, 0, 16 * 1024);
        if (length <= 1) return {};
        Vector<GLchar> bytes(static_cast<SizeT>(length) + 1, '\0');
        GLsizei written = 0;
        g_GLESFuncs.glGetProgramInfoLog(program, length, &written, bytes.data());
        return written > 0 ? String(bytes.data(), static_cast<SizeT>(written)) : String{};
    }

    static void PZF3LogUniformInventory(
        const PZF3ProgramRecord& record,
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        String chunk;
        Uint chunkOrdinal = 0;
        const Uint count = std::min<Uint>(program->GetUniformCount(), 96);
        auto flush = [&]() {
            if (chunk.empty()) return;
            MGLOG_I("MOBILEPZ_PZF3_UNIFORMS family=%s program=%u chunk=%u text=%s",
                    PZF3FamilyName(record.family), record.external, chunkOrdinal++, chunk.c_str());
            chunk.clear();
        };
        for (Uint i = 0; i < count; ++i) {
            const String& name = program->GetActiveUniformName(i);
            const Int location = program->GetUniformLocation(name);
            const Uint offset = location >= 0
                ? program->GetUniformOffset(static_cast<Uint>(location))
                : MG_State::GLState::ProgramObject::kInvalidUniformOffset;
            char entry[320] = {};
            std::snprintf(entry, sizeof(entry), "%s%s@%d:0x%x[%d]:off=%u",
                          chunk.empty() ? "" : ";", name.c_str(), location,
                          program->GetActiveUniformType(i), program->GetActiveUniformArraySize(i), offset);
            if (chunk.size() + std::strlen(entry) > 1300) flush();
            chunk += entry;
        }
        flush();
        if (program->GetUniformCount() > count) {
            MGLOG_I("MOBILEPZ_PZF3_UNIFORMS family=%s program=%u truncated=1 active=%u logged=%u",
                    PZF3FamilyName(record.family), record.external,
                    program->GetUniformCount(), count);
        }
    }

    static void PZF3LogAttributeInventory(
        const PZF3ProgramRecord& record,
        const SharedPtr<MG_State::GLState::ProgramObject>& program) {
        const Uint32 mask = program->GetActiveAttributeLocationMask();
        String text;
        for (Uint location = 0; location < 32; ++location) {
            if ((mask & (Uint32{1} << location)) == 0) continue;
            char entry[256] = {};
            std::snprintf(entry, sizeof(entry), "%s%u=%s:0x%x",
                          text.empty() ? "" : ";", location,
                          program->GetAttribName(location).c_str(), program->GetAttribType(location));
            if (text.size() + std::strlen(entry) > 1300) break;
            text += entry;
        }
        MGLOG_I("MOBILEPZ_PZF3_ATTRIBUTES family=%s program=%u active_mask=0x%x text=%s",
                PZF3FamilyName(record.family), record.external, mask,
                text.empty() ? "<none>" : text.c_str());
    }

    static void PZF3LogProgramInventory(
        PZF3ProgramRecord& record,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        PrgramImpl::BackendProgramObjectImpl* backend) {
        if (record.inventoryLogged) return;
        record.inventoryLogged = true;
        const GLuint backendId = backend ? backend->GetBackendProgramId() : 0;
        GLint driverCurrent = 0;
        if (g_GLESFuncs.glGetIntegerv) g_GLESFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &driverCurrent);
        GLint driverLinked = 0;
        if (backendId && g_GLESFuncs.glGetProgramiv) {
            g_GLESFuncs.glGetProgramiv(backendId, GL_LINK_STATUS, &driverLinked);
        }
        const String driverInfo = PZF3DriverProgramInfo(backendId);
        MGLOG_I("MOBILEPZ_PZF3_PROGRAM family=%s front_program=%u lifetime=%llu link=%u "
                "front_linked=%d front_spirv=%d uniforms=%u attrib_mask=0x%x "
                "ubo_size=%u ubo_version=%u ubo_hash=%016llx backend_program=%u "
                "backend_usable=%d backend_link=%u backend_ubo=%u backend_ubo_size=%d "
                "backend_uploaded_version=%u driver_current=%d driver_linked=%d driver_info=%s",
                PZF3FamilyName(record.family), record.external,
                static_cast<unsigned long long>(record.lifetime), record.link,
                program->GetLinkStatus() ? 1 : 0, program->GetSpirvStatus() ? 1 : 0,
                program->GetUniformCount(), program->GetActiveAttributeLocationMask(),
                program->GetUBOSize(), program->GetUBOContentVersion(),
                static_cast<unsigned long long>(PZF3HashBytes(program->GetUBOData(), program->GetUBOSize())),
                backendId, backend && backend->IsBackendProgramUsable() ? 1 : 0,
                backend ? backend->GetSyncedLinkVersion() : 0,
                backend ? backend->GetBackendGlobalUBOId() : 0,
                backend ? backend->GetGlobalUboBackendBlockSize() : -1,
                backend ? backend->GetLastUploadedGlobalUboVersion() : 0,
                driverCurrent, driverLinked, PZF3FirstLine(driverInfo).c_str());
        MGLOG_I("MOBILEPZ_PZF3_FEATURES family=%s program=%u MatrixPalette=%d boneEffect=%d "
                "transform=%d FinalScale=%d chunkDepth=%d zDepth=%d useTexture=%d "
                "DIFFUSE=%d MVP=%d sampler=%d alpha=%d classifier=heuristic",
                PZF3FamilyName(record.family), record.external,
                record.features.matrixPalette ? 1 : 0, record.features.boneEffect ? 1 : 0,
                record.features.transform ? 1 : 0, record.features.finalScale ? 1 : 0,
                record.features.chunkDepth ? 1 : 0, record.features.zDepth ? 1 : 0,
                record.features.useTexture ? 1 : 0, record.features.diffuse ? 1 : 0,
                record.features.mvp ? 1 : 0, record.features.sampler ? 1 : 0,
                record.features.alpha ? 1 : 0);

        PZF3LogUniformInventory(record, program);
        PZF3LogAttributeInventory(record, program);
        const Bool dumpSources = PZF3ReserveSourceDump(record.family);
        for (const auto& shader : program->GetAttachedShaders()) {
            if (!shader) continue;
            const String& source = shader->GetShaderSource();
            const Bool compiled = shader->GetCompileStatus();
            MGLOG_I("MOBILEPZ_PZF3_FRONT_SOURCE family=%s program=%u shader=%u stage=%s "
                    "compiled=%d bytes=%zu hash=%016llx info=%s",
                    PZF3FamilyName(record.family), record.external, shader->GetExternalIndex(),
                    PZF3StageName(shader->GetShaderStage()), compiled ? 1 : 0, source.size(),
                    static_cast<unsigned long long>(PZF3HashText(source)),
                    PZF3FirstLine(shader->GetInfoLog()).c_str());
            if (dumpSources || !compiled) {
                PZF3LogSourceChunks("FRONT", record.family, record.external,
                                    PZF3StageName(shader->GetShaderStage()),
                                    shader->GetExternalIndex(), source);
            }
        }

        if (!backendId || !g_GLESFuncs.glGetProgramiv || !g_GLESFuncs.glGetAttachedShaders) {
            MGLOG_I("MOBILEPZ_PZF3_DRIVER_SOURCE family=%s program=%u status=unavailable backend_program=%u",
                    PZF3FamilyName(record.family), record.external, backendId);
            return;
        }
        GLint attached = 0;
        g_GLESFuncs.glGetProgramiv(backendId, GL_ATTACHED_SHADERS, &attached);
        attached = std::clamp<GLint>(attached, 0, 8);
        Array<GLuint, 8> shaders{};
        GLsizei written = 0;
        g_GLESFuncs.glGetAttachedShaders(backendId, attached, &written, shaders.data());
        for (GLsizei i = 0; i < written; ++i) {
            const GLuint shader = shaders[static_cast<SizeT>(i)];
            GLint stage = 0;
            GLint compiled = 0;
            if (g_GLESFuncs.glGetShaderiv) {
                g_GLESFuncs.glGetShaderiv(shader, GL_SHADER_TYPE, &stage);
                g_GLESFuncs.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            }
            const String source = PZF3DriverShaderSource(shader);
            const String info = PZF3DriverShaderInfo(shader);
            PZF3Features translatedFeatures;
            PZF3AccumulateFeatures(translatedFeatures, source);
            MGLOG_I("MOBILEPZ_PZF3_DRIVER_SOURCE family=%s program=%u backend_program=%u "
                    "shader=%u stage=%s compiled=%d bytes=%zu hash=%016llx "
                    "MatrixPalette=%d boneEffect=%d transform=%d FinalScale=%d "
                    "chunkDepth=%d zDepth=%d useTexture=%d DIFFUSE=%d info=%s",
                    PZF3FamilyName(record.family), record.external, backendId, shader,
                    PZF3DriverStageName(static_cast<GLenum>(stage)), compiled,
                    source.size(), static_cast<unsigned long long>(PZF3HashText(source)),
                    translatedFeatures.matrixPalette ? 1 : 0, translatedFeatures.boneEffect ? 1 : 0,
                    translatedFeatures.transform ? 1 : 0, translatedFeatures.finalScale ? 1 : 0,
                    translatedFeatures.chunkDepth ? 1 : 0, translatedFeatures.zDepth ? 1 : 0,
                    translatedFeatures.useTexture ? 1 : 0, translatedFeatures.diffuse ? 1 : 0,
                    PZF3FirstLine(info).c_str());
            if (dumpSources || !compiled) {
                PZF3LogSourceChunks("DRIVER", record.family, record.external,
                                    PZF3DriverStageName(static_cast<GLenum>(stage)), shader, source);
            }
        }
    }
} // namespace

namespace {
    struct PZF3BackendUniformLayout {
        Bool found = false;
        String selectedName;
        GLuint index = GL_INVALID_INDEX;
        GLint type = 0;
        GLint size = 0;
        GLint block = -1;
        GLint offset = -1;
        GLint arrayStride = 0;
        GLint matrixStride = 0;
        GLint rowMajor = 0;
    };

    static PZF3BackendUniformLayout PZF3ReflectBackendUniform(
        GLuint backendProgram, const String& frontendName) {
        PZF3BackendUniformLayout result;
        if (!backendProgram || !g_GLESFuncs.glGetUniformIndices ||
            !g_GLESFuncs.glGetActiveUniformsiv) return result;
        String baseName = frontendName;
        if (baseName.size() > 3 && baseName.compare(baseName.size() - 3, 3, "[0]") == 0) {
            baseName.resize(baseName.size() - 3);
        }
        const String blockName = "MGL_GLOBAL_UBO." + frontendName;
        const String blockBaseName = "MGL_GLOBAL_UBO." + baseName;
        const Array<const String*, 4> names = {&blockName, &blockBaseName, &frontendName, &baseName};
        for (const String* candidate : names) {
            const GLchar* raw = candidate->c_str();
            GLuint index = GL_INVALID_INDEX;
            g_GLESFuncs.glGetUniformIndices(backendProgram, 1, &raw, &index);
            if (index == GL_INVALID_INDEX) continue;
            result.found = true;
            result.selectedName = *candidate;
            result.index = index;
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_TYPE, &result.type);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_SIZE, &result.size);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_BLOCK_INDEX, &result.block);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_OFFSET, &result.offset);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_ARRAY_STRIDE,
                                               &result.arrayStride);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_MATRIX_STRIDE,
                                               &result.matrixStride);
            g_GLESFuncs.glGetActiveUniformsiv(backendProgram, 1, &index, GL_UNIFORM_IS_ROW_MAJOR,
                                               &result.rowMajor);
            break;
        }
        return result;
    }

    static Bool PZF3IsFloatUniform(GLenum type) {
        switch (type) {
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT3x2:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x2:
        case GL_FLOAT_MAT4x3:
            return true;
        default:
            return false;
        }
    }

    static SizeT PZF3FrontendUniformSpan(
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        Int location, Int arraySize) {
        if (!program || location < 0 || !program->IsValidUniformLocation(location)) return 0;
        const SizeT elementSpan = program->GetUniformStorageSpanInBytes(static_cast<Uint>(location));
        if (arraySize <= 1) return elementSpan;
        const Int lastLocation = location + arraySize - 1;
        if (!program->IsValidUniformLocation(lastLocation) ||
            !program->UniformLocationsAliasSameUniform(location, lastLocation)) return elementSpan;
        const Uint firstOffset = program->GetUniformOffset(static_cast<Uint>(location));
        const Uint lastOffset = program->GetUniformOffset(static_cast<Uint>(lastLocation));
        if (firstOffset == MG_State::GLState::ProgramObject::kInvalidUniformOffset ||
            lastOffset == MG_State::GLState::ProgramObject::kInvalidUniformOffset ||
            lastOffset < firstOffset) return elementSpan;
        return static_cast<SizeT>(lastOffset - firstOffset) + elementSpan;
    }

    static void PZF3LogImportantUniform(
        Uint64 serial, PZF3Family family,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        GLuint backendProgram, const char* requestedName) {
        if (!program || !requestedName) return;
        const String name = requestedName;
        const Int location = program->GetUniformLocation(name);
        if (location < 0 || !program->IsValidUniformLocation(location)) return;
        const Int activeIndex = program->GetActiveUniformIndex(name);
        const Int arraySize = activeIndex >= 0
            ? program->GetActiveUniformArraySize(static_cast<Uint>(activeIndex)) : 1;
        const GLenum type = program->GetUniformType(static_cast<Uint>(location));
        const Uint offset = program->GetUniformOffset(static_cast<Uint>(location));
        const SizeT span = PZF3FrontendUniformSpan(program, location, arraySize);
        const Uint uboSize = program->GetUBOSize();
        const auto* ubo = static_cast<const Uint8*>(program->GetUBOData());
        const Bool readable = ubo && offset != MG_State::GLState::ProgramObject::kInvalidUniformOffset &&
            static_cast<SizeT>(offset) <= uboSize && span <= static_cast<SizeT>(uboSize) - offset;
        const auto* bytes = readable ? ubo + offset : nullptr;
        const Uint64 hash = PZF3HashBytes(bytes, readable ? span : 0);

        Array<Float, 4> preview{};
        Bool finite = true;
        Float absMax = 0.0f;
        Uint nonZero = 0;
        const SizeT floatWords = readable && PZF3IsFloatUniform(type) ? span / sizeof(Float) : 0;
        const SizeT scannedWords = std::min<SizeT>(floatWords, 4096);
        for (SizeT i = 0; i < scannedWords; ++i) {
            Float value = 0.0f;
            std::memcpy(&value, bytes + i * sizeof(Float), sizeof(Float));
            if (i < preview.size()) preview[i] = value;
            finite = finite && std::isfinite(value);
            absMax = std::max(absMax, std::abs(value));
            if (value != 0.0f) ++nonZero;
        }

        const PZF3BackendUniformLayout backend = PZF3ReflectBackendUniform(backendProgram, name);
        const Bool layoutMatch = backend.found && backend.type == static_cast<GLint>(type) &&
            backend.size == arraySize && backend.offset >= 0 &&
            offset != MG_State::GLState::ProgramObject::kInvalidUniformOffset &&
            static_cast<Uint>(backend.offset) == offset && backend.rowMajor == 0;
        MGLOG_I("MOBILEPZ_PZF3_UNIFORM_STATE serial=%llu family=%s program=%u name=%s "
                "location=%d type=0x%x array=%d offset=%u span=%zu readable=%d written=%d "
                "hash=%016llx finite=%d float_words=%zu nonzero=%u absmax=%g "
                "first4=%g,%g,%g,%g backend_name=%s backend_found=%d backend_index=%u "
                "backend_type=0x%x backend_size=%d backend_block=%d backend_offset=%d "
                "backend_array_stride=%d backend_matrix_stride=%d row_major=%d layout_match=%d",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family),
                program->GetExternalIndex(), name.c_str(), location, type, arraySize, offset, span,
                readable ? 1 : 0, program->IsUniformWrittenAtLocation(static_cast<Uint>(location)) ? 1 : 0,
                static_cast<unsigned long long>(hash), finite ? 1 : 0, scannedWords, nonZero, absMax,
                preview[0], preview[1], preview[2], preview[3], backend.selectedName.c_str(),
                backend.found ? 1 : 0, backend.index, backend.type, backend.size, backend.block,
                backend.offset, backend.arrayStride, backend.matrixStride, backend.rowMajor,
                layoutMatch ? 1 : 0);
    }

    static void PZF3LogImportantUniforms(
        Uint64 serial, PZF3Family family,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        GLuint backendProgram) {
        static constexpr const char* names[] = {
            "ModelViewProjection", "MVPMatrix", "transform", "MatrixPalette[0]",
            "FinalScale", "u_color", "Alpha", "alpha", "useTexture", "DIFFUSE",
            "TintColour", "AmbientColour", "NightValue", "NightVisionGoggles",
            "DesaturationVal", "SearchMode", "BlurFactor"
        };
        for (const char* name : names) {
            PZF3LogImportantUniform(serial, family, program, backendProgram, name);
        }
    }

    static void PZF3LogSamplers(
        Uint64 serial, PZF3Family family,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        PrgramImpl::BackendProgramObjectImpl* backend) {
        if (!program || !backend) return;
        const GLuint backendId = backend->GetBackendProgramId();
        Uint ordinal = 0;
        for (const auto& binding : backend->GetSamplerUniformBindings()) {
            if (ordinal >= 12) break;
            const Uint location = binding.frontendLocation;
            if (!program->IsValidUniformLocation(static_cast<Int>(location))) continue;
            const String& name = program->GetUniformName(location);
            const Int frontUnit = program->GetUniformSamplerOrImageUnitIndex(location);
            GLint driverUnit = -1;
            if (backendId && binding.backendLocation >= 0 && g_GLESFuncs.glGetUniformiv) {
                g_GLESFuncs.glGetUniformiv(backendId, binding.backendLocation, &driverUnit);
            }
            const TextureTarget target = SamplerUniformTextureTarget(binding.uniformType);
            SharedPtr<MG_State::GLState::ITextureObject> texture;
            if (frontUnit >= 0 &&
                frontUnit < MG_State::GLState::TextureState::MAX_TEXTURE_IMAGE_UNITS &&
                target != TextureTarget::Unknown) {
                texture = MG_State::pGLContext->GetTextureUnitObject(frontUnit)
                              .GetBindingSlot(target).GetBoundObject();
            }
            Uint nativeTexture = 0;
            if (texture) {
                auto* backendTextureSlot = TextureImpl::g_backendTextureObjects.Find(texture.get());
                if (backendTextureSlot && *backendTextureSlot) {
                    nativeTexture = (*backendTextureSlot)->GetBackendTextureId();
                }
            }
            const auto size = texture ? texture->GetBaseSize() : IntVec3{};
            const auto levels = texture ? texture->GetLevelRange() : UintVec2{};
            MGLOG_I("MOBILEPZ_PZF3_SAMPLER serial=%llu family=%s program=%u ordinal=%u "
                    "name=%s type=0x%x front_location=%u backend_location=%d "
                    "front_unit=%d driver_unit=%d unit_match=%d target=%d "
                    "front_texture=%u texture_lifetime=%llu native_texture=%u format=%d "
                    "size=%d,%d,%d complete=%d levels=%u,%u content_version=%llu samples=%d",
                    static_cast<unsigned long long>(serial), PZF3FamilyName(family),
                    program->GetExternalIndex(), ordinal, name.c_str(), binding.uniformType,
                    location, binding.backendLocation, frontUnit, driverUnit,
                    frontUnit == driverUnit ? 1 : 0, static_cast<Int>(target),
                    texture ? texture->GetExternalIndex() : 0,
                    static_cast<unsigned long long>(texture ? texture->GetLifetimeId() : 0),
                    nativeTexture, static_cast<Int>(texture ? texture->GetFormat() : TextureInternalFormat::Unknown),
                    size.x(), size.y(), size.z(), texture && texture->IsComplete() ? 1 : 0,
                    levels.x(), levels.y(),
                    static_cast<unsigned long long>(texture ? texture->GetContentVersion() : 0),
                    texture ? texture->GetSamples() : 0);
            ++ordinal;
        }
        MGLOG_I("MOBILEPZ_PZF3_SAMPLER_SUMMARY serial=%llu family=%s program=%u "
                "bindings=%zu logged=%u",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family),
                program->GetExternalIndex(), backend->GetSamplerUniformBindings().size(), ordinal);
    }

    static Uint32 PZF3EnabledAttributeMask(
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao) {
        if (!vao) return 0;
        Uint32 mask = 0;
        const auto& attributes = vao->GetAllAttributes();
        for (SizeT i = 0; i < attributes.size() && i < 32; ++i) {
            if (attributes[i].Enabled) mask |= Uint32{1} << i;
        }
        return mask;
    }

    static void PZF3LogVertexRoute(
        Uint64 serial, PZF3Family family,
        const SharedPtr<MG_State::GLState::VertexArrayObject>& vao) {
        const auto& ebo = vao ? vao->GetIndexBufferBindingSlot().GetBoundObject()
                              : SharedPtr<MG_State::GLState::BufferObject>{};
        auto* eboResource = ebo ? BufferImpl::GetBufferResource(ebo.get()) : nullptr;
        MGLOG_I("MOBILEPZ_PZF3_VERTEX_ROUTE serial=%llu family=%s front_vao=%u "
                "vao_lifetime=%llu enabled_mask=0x%x front_ebo=%u ebo_lifetime=%llu "
                "native_ebo=%u ebo_size=%zu",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family),
                vao ? vao->GetExternalIndex() : 0,
                static_cast<unsigned long long>(vao ? vao->GetLifetimeId() : 0),
                PZF3EnabledAttributeMask(vao), ebo ? ebo->GetExternalIndex() : 0,
                static_cast<unsigned long long>(ebo ? ebo->GetLifetimeId() : 0),
                eboResource ? eboResource->id : 0, ebo ? ebo->GetSize() : 0);
        if (!vao) return;
        const auto& attributes = vao->GetAllAttributes();
        Uint logged = 0;
        for (SizeT i = 0; i < attributes.size() && logged < 16; ++i) {
            const auto& attribute = attributes[i];
            if (!attribute.Enabled) continue;
            auto* resource = attribute.Buffer ? BufferImpl::GetBufferResource(attribute.Buffer.get()) : nullptr;
            MGLOG_I("MOBILEPZ_PZF3_ATTRIBUTE serial=%llu family=%s location=%zu enabled=1 "
                    "size=%d type=0x%x normalized=%d integer=%d stride=%d offset=%zu divisor=%u "
                    "front_vbo=%u vbo_lifetime=%llu native_vbo=%u vbo_size=%zu",
                    static_cast<unsigned long long>(serial), PZF3FamilyName(family), i,
                    attribute.Size, static_cast<Uint>(attribute.Type), attribute.Normalized ? 1 : 0,
                    attribute.IsInteger ? 1 : 0, attribute.Stride, attribute.Offset, attribute.Divisor,
                    attribute.Buffer ? attribute.Buffer->GetExternalIndex() : 0,
                    static_cast<unsigned long long>(attribute.Buffer ? attribute.Buffer->GetLifetimeId() : 0),
                    resource ? resource->id : 0, attribute.Buffer ? attribute.Buffer->GetSize() : 0);
            ++logged;
        }
    }

    static void PZF3LogDriverState(Uint64 serial, PZF3Family family) {
        GLint currentProgram = 0, currentVao = 0, arrayBuffer = 0, elementBuffer = 0;
        GLint drawFbo = 0, viewport[4] = {}, scissor[4] = {};
        GLint depthFunc = 0, cullMode = 0, frontFace = 0;
        GLint blendSrcRgb = 0, blendDstRgb = 0, blendSrcAlpha = 0, blendDstAlpha = 0;
        GLint blendEqRgb = 0, blendEqAlpha = 0;
        GLboolean depthMask = GL_FALSE;
        GLboolean colorMask[4] = {};
        if (g_GLESFuncs.glGetIntegerv) {
            g_GLESFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
            g_GLESFuncs.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVao);
            g_GLESFuncs.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
            g_GLESFuncs.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);
            g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
            g_GLESFuncs.glGetIntegerv(GL_VIEWPORT, viewport);
            g_GLESFuncs.glGetIntegerv(GL_SCISSOR_BOX, scissor);
            g_GLESFuncs.glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
            g_GLESFuncs.glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
            g_GLESFuncs.glGetIntegerv(GL_FRONT_FACE, &frontFace);
            g_GLESFuncs.glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
            g_GLESFuncs.glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
            g_GLESFuncs.glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
            g_GLESFuncs.glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
            g_GLESFuncs.glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRgb);
            g_GLESFuncs.glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqAlpha);
        }
        if (g_GLESFuncs.glGetBooleanv) {
            g_GLESFuncs.glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            g_GLESFuncs.glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        }
        const GLenum fboStatus = g_GLESFuncs.glCheckFramebufferStatus
            ? g_GLESFuncs.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) : 0;
        const auto enabled = [](GLenum cap) -> Int {
            return g_GLESFuncs.glIsEnabled && g_GLESFuncs.glIsEnabled(cap) == GL_TRUE ? 1 : 0;
        };
        MGLOG_I("MOBILEPZ_PZF3_DRIVER_STATE serial=%llu family=%s current_program=%d "
                "vao=%d array_buffer=%d element_buffer=%d draw_fbo=%d fbo_status=0x%x "
                "viewport=%d,%d,%d,%d scissor_test=%d scissor=%d,%d,%d,%d "
                "depth_test=%d depth_func=0x%x depth_mask=%d cull=%d cull_mode=0x%x "
                "front_face=0x%x blend=%d blend_rgb=0x%x,0x%x blend_alpha=0x%x,0x%x "
                "blend_eq=0x%x,0x%x color_mask=%d,%d,%d,%d raster_discard=%d",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family), currentProgram,
                currentVao, arrayBuffer, elementBuffer, drawFbo, fboStatus,
                viewport[0], viewport[1], viewport[2], viewport[3], enabled(GL_SCISSOR_TEST),
                scissor[0], scissor[1], scissor[2], scissor[3], enabled(GL_DEPTH_TEST), depthFunc,
                depthMask == GL_TRUE ? 1 : 0, enabled(GL_CULL_FACE), cullMode, frontFace,
                enabled(GL_BLEND), blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha,
                blendEqRgb, blendEqAlpha, colorMask[0] == GL_TRUE ? 1 : 0,
                colorMask[1] == GL_TRUE ? 1 : 0, colorMask[2] == GL_TRUE ? 1 : 0,
                colorMask[3] == GL_TRUE ? 1 : 0, enabled(GL_RASTERIZER_DISCARD));
    }

    static Bool PZF3ReserveDetailed(PZF3ProgramRecord& record) {
        if (record.detailedLogged || g_pzf3DetailedCaptures >= kPZF3MaxDetailedCaptures) return false;
        static constexpr Array<Uint64, kPZF3FamilyCount> limits = {24, 24, 24, 4, 4};
        const SizeT index = PZF3FamilyIndex(record.family);
        if (g_pzf3FamilyDetails[index] >= limits[index]) return false;
        record.detailedLogged = true;
        ++g_pzf3DetailedCaptures;
        ++g_pzf3FamilyDetails[index];
        return true;
    }

    static void PZF3LogDetailedDraw(
        Uint64 serial, PZF3ProgramRecord& record,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        PrgramImpl::BackendProgramObjectImpl* backend) {
        PZF3LogDriverState(serial, record.family);
        PZF3LogVertexRoute(serial, record.family, MG_State::pGLContext->GetBoundVertexArray());
        const GLuint backendId = backend ? backend->GetBackendProgramId() : 0;
        PZF3LogImportantUniforms(serial, record.family, program, backendId);
        PZF3LogSamplers(serial, record.family, program, backend);
        MGLOG_I("MOBILEPZ_PZF3_DETAIL_DONE serial=%llu family=%s program=%u "
                "rendering_mutation=0 error_drain=0",
                static_cast<unsigned long long>(serial), PZF3FamilyName(record.family), record.external);
    }

#ifdef MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
    struct PZF7UniformSnapshot {
        Bool valid = false;
        Bool readable = false;
        Bool finite = true;
        Int location = -1;
        Uint offset = MG_State::GLState::ProgramObject::kInvalidUniformOffset;
        SizeT span = 0;
        Uint64 hash = 0;
        Uint nonZeroWords = 0;
        Float absMax = 0.0f;
    };

    static ::MobilePZ::PZF7::Family PZF7FamilyFromPZF3(PZF3Family family) {
        if (family == PZF3Family::Skinned) return ::MobilePZ::PZF7::Family::Skinned;
        if (family == PZF3Family::StaticModel) return ::MobilePZ::PZF7::Family::StaticModel;
        return ::MobilePZ::PZF7::Family::Other;
    }

    static PZF7UniformSnapshot PZF7ReadUniformSnapshot(
        const SharedPtr<MG_State::GLState::ProgramObject>& program, const char* name) {
        PZF7UniformSnapshot snapshot;
        if (!program || !name) return snapshot;
        snapshot.location = program->GetUniformLocation(name);
        if (snapshot.location < 0 || !program->IsValidUniformLocation(snapshot.location)) return snapshot;
        snapshot.valid = true;
        snapshot.offset = program->GetUniformOffset(static_cast<Uint>(snapshot.location));
        snapshot.span = std::min<SizeT>(
            program->GetUniformStorageSpanInBytes(static_cast<Uint>(snapshot.location)),
            16 * sizeof(Float));
        const SizeT uboSize = program->GetUBOSize();
        const auto* bytes = static_cast<const Uint8*>(program->GetUBOData());
        if (!bytes || snapshot.offset == MG_State::GLState::ProgramObject::kInvalidUniformOffset ||
            snapshot.span == 0 || static_cast<SizeT>(snapshot.offset) + snapshot.span > uboSize) {
            return snapshot;
        }
        snapshot.readable = true;
        const void* value = bytes + snapshot.offset;
        snapshot.hash = ::MobilePZ::PZF7::HashBytes(value, snapshot.span);
        for (SizeT byte = 0; byte + sizeof(Float) <= snapshot.span; byte += sizeof(Float)) {
            Float word = 0.0f;
            std::memcpy(&word, bytes + snapshot.offset + byte, sizeof(word));
            if (!std::isfinite(word)) snapshot.finite = false;
            if (word != 0.0f) ++snapshot.nonZeroWords;
            if (std::isfinite(word)) snapshot.absMax = std::max(snapshot.absMax, std::fabs(word));
        }
        return snapshot;
    }

    static Bool PZF7CallMatchesShadow(const ::MobilePZ::PZF7::MatrixCall& call,
                                      const PZF7UniformSnapshot& shadow) {
        return call.valid && call.effectiveInputHash != 0 && shadow.readable &&
               call.effectiveInputHash == shadow.hash;
    }

    static const char* PZF7CallName(const ::MobilePZ::PZF7::MatrixCall& call) {
        return call.valid && call.uniformName[0] ? call.uniformName.data() : "none";
    }

    static const char* PZF7CallOutcome(const ::MobilePZ::PZF7::MatrixCall& call) {
        return call.valid ? ::MobilePZ::PZF7::MatrixOutcomeName(call.outcome) : "none";
    }

    static const char* PZF7CallRole(const ::MobilePZ::PZF7::MatrixCall& call) {
        return call.valid ? ::MobilePZ::PZF7::MatrixRoleName(call.role) : "none";
    }

    static void PZF7CapturePreparedModelDraw(
        Uint64 serial, const PZF3ProgramRecord& record,
        const SharedPtr<MG_State::GLState::ProgramObject>& program,
        GLsizei count, GLsizei instances) {
        if (count <= 0 || instances <= 0) return;
        const auto family = PZF7FamilyFromPZF3(record.family);
        const auto decision = ::MobilePZ::PZF7::gTraceState.CaptureModelDraw(
            family, record.external, record.lifetime, g_pzf3PresentSerial);
        if (!decision.selected || !program) return;

        const SizeT uboSize = program->GetUBOSize();
        const Uint32 uboVersion = program->GetUBOContentVersion();
        const Uint64 shadowHash =
            ::MobilePZ::PZF7::HashBytes(program->GetUBOData(), uboSize);
        const auto& binding = decision.binding;
        const Bool bindingIdentity = binding.valid && binding.program == record.external &&
                                     binding.lifetime == record.lifetime;
        const Bool versionMatch = bindingIdentity && binding.submittedKnown &&
                                  uboVersion == binding.submittedVersion;
        const Bool sizeMatch = bindingIdentity && binding.submittedKnown &&
                               uboSize == binding.submittedSize;
        const Bool bindShadowMatch = bindingIdentity && binding.frontendVersion == uboVersion &&
                                     binding.frontendSize == uboSize &&
                                     binding.frontendHash == shadowHash;
        const Bool fullPayloadMatch = versionMatch && sizeMatch && bindShadowMatch &&
                                      binding.submittedHash == shadowHash;

        MGLOG_I("MOBILEPZ_PZF7_DRAW_CAPTURE capture=%u generation=%u serial=%llu "
                "present=%llu family=%s program=%u lifetime=%llu ubo_version=%u ubo_size=%zu "
                "shadow_hash=%016llx binding_seen=%d binding_identity=%d bind_sequence=%llu "
                "binding_generation=%u "
                "transfer_path=%s fresh_submission=%d bind_front_version=%u "
                "bind_front_size=%zu bind_front_hash=%016llx submitted_known=%d "
                "submitted_version=%u submitted_size=%zu submitted_hash=%016llx "
                "version_match=%d size_match=%d bind_shadow_match=%d full_payload_match=%d "
                "ring_generation=%u frame_serial=%llu ring_offset=%zu "
                "rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
                decision.capture, g_backendContextGeneration,
                static_cast<unsigned long long>(serial),
                static_cast<unsigned long long>(g_pzf3PresentSerial),
                ::MobilePZ::PZF7::FamilyName(family), record.external,
                static_cast<unsigned long long>(record.lifetime), uboVersion, uboSize,
                static_cast<unsigned long long>(shadowHash), binding.valid ? 1 : 0,
                bindingIdentity ? 1 : 0,
                static_cast<unsigned long long>(binding.sequence),
                binding.contextGeneration,
                ::MobilePZ::PZF7::UboPathName(binding.path),
                binding.freshSubmission ? 1 : 0, binding.frontendVersion,
                binding.frontendSize, static_cast<unsigned long long>(binding.frontendHash),
                binding.submittedKnown ? 1 : 0, binding.submittedVersion,
                binding.submittedSize, static_cast<unsigned long long>(binding.submittedHash),
                versionMatch ? 1 : 0, sizeMatch ? 1 : 0, bindShadowMatch ? 1 : 0,
                fullPayloadMatch ? 1 : 0, binding.ringGeneration,
                static_cast<unsigned long long>(binding.frameSerial), binding.ringOffset);

        const PZF7UniformSnapshot mvp = PZF7ReadUniformSnapshot(program, "ModelViewProjection");
        const PZF7UniformSnapshot mvpFallback =
            mvp.valid ? PZF7UniformSnapshot{} : PZF7ReadUniformSnapshot(program, "MVPMatrix");
        const PZF7UniformSnapshot& selectedMvp = mvp.valid ? mvp : mvpFallback;
        const PZF7UniformSnapshot palette = PZF7ReadUniformSnapshot(program, "MatrixPalette[0]");
        const PZF7UniformSnapshot transform = PZF7ReadUniformSnapshot(program, "transform");
        MGLOG_I("MOBILEPZ_PZF7_DRAW_UNIFORMS capture=%u family=%s program=%u "
                "mvp_valid=%d mvp_readable=%d mvp_location=%d mvp_offset=%u mvp_span=%zu "
                "mvp_hash=%016llx mvp_finite=%d mvp_nonzero_words=%u mvp_abs_max=%g "
                "palette_valid=%d palette_readable=%d palette_location=%d palette_offset=%u "
                "palette_span=%zu palette_hash=%016llx palette_finite=%d "
                "palette_nonzero_words=%u palette_abs_max=%g "
                "transform_valid=%d transform_readable=%d transform_location=%d "
                "transform_offset=%u transform_span=%zu transform_hash=%016llx "
                "transform_finite=%d transform_nonzero_words=%u transform_abs_max=%g",
                decision.capture, ::MobilePZ::PZF7::FamilyName(family), record.external,
                selectedMvp.valid ? 1 : 0, selectedMvp.readable ? 1 : 0,
                selectedMvp.location, selectedMvp.offset, selectedMvp.span,
                static_cast<unsigned long long>(selectedMvp.hash), selectedMvp.finite ? 1 : 0,
                selectedMvp.nonZeroWords, selectedMvp.absMax, palette.valid ? 1 : 0,
                palette.readable ? 1 : 0, palette.location, palette.offset, palette.span,
                static_cast<unsigned long long>(palette.hash), palette.finite ? 1 : 0,
                palette.nonZeroWords, palette.absMax, transform.valid ? 1 : 0,
                transform.readable ? 1 : 0, transform.location, transform.offset,
                transform.span, static_cast<unsigned long long>(transform.hash),
                transform.finite ? 1 : 0, transform.nonZeroWords, transform.absMax);

        const auto& any = decision.latestAny;
        const auto& latestMvp = decision.latestMvp;
        const auto& latestPalette = decision.latestPalette;
        const auto& latestTransform = decision.latestTransform;
        const auto& locationZero = decision.latestLocationZero;
        MGLOG_I("MOBILEPZ_PZF7_DRAW_CALLS capture=%u family=%s program=%u "
                "any_seq=%llu any_location=%d any_name=%s any_type=0x%x any_opaque=%d "
                "any_role=%s any_outcome=%s "
                "mvp_seq=%llu mvp_input_hash=%016llx mvp_shadow_match=%d mvp_outcome=%s "
                "palette_seq=%llu palette_input_hash=%016llx palette_shadow_match=%d "
                "palette_outcome=%s transform_seq=%llu transform_input_hash=%016llx "
                "transform_shadow_match=%d transform_outcome=%s "
                "location0_seq=%llu location0_name=%s location0_type=0x%x "
                "location0_opaque=%d location0_outcome=%s",
                decision.capture, ::MobilePZ::PZF7::FamilyName(family), record.external,
                static_cast<unsigned long long>(any.sequence), any.location, PZF7CallName(any),
                any.type, any.opaque ? 1 : 0, PZF7CallRole(any), PZF7CallOutcome(any),
                static_cast<unsigned long long>(latestMvp.sequence),
                static_cast<unsigned long long>(latestMvp.effectiveInputHash),
                PZF7CallMatchesShadow(latestMvp, selectedMvp) ? 1 : 0,
                PZF7CallOutcome(latestMvp),
                static_cast<unsigned long long>(latestPalette.sequence),
                static_cast<unsigned long long>(latestPalette.effectiveInputHash),
                PZF7CallMatchesShadow(latestPalette, palette) ? 1 : 0,
                PZF7CallOutcome(latestPalette),
                static_cast<unsigned long long>(latestTransform.sequence),
                static_cast<unsigned long long>(latestTransform.effectiveInputHash),
                PZF7CallMatchesShadow(latestTransform, transform) ? 1 : 0,
                PZF7CallOutcome(latestTransform),
                static_cast<unsigned long long>(locationZero.sequence),
                PZF7CallName(locationZero), locationZero.type, locationZero.opaque ? 1 : 0,
                PZF7CallOutcome(locationZero));
    }

    static void PZF7LogSummary(const char* reason) {
        const auto summary = ::MobilePZ::PZF7::gTraceState.GetSummary();
        MGLOG_I("MOBILEPZ_PZF7_SUMMARY reason=%s context_ready=%d context_generation=%u "
                "context_ready_events=%llu context_destroyed_events=%llu presents=%llu "
                "matrix_calls=%llu model_matrix_calls=%llu matrix_location0=%llu "
                "matrix_location0_opaque=%llu model_matrix_location0_opaque=%llu "
                "matrix_mvp=%llu matrix_palette=%llu "
                "matrix_transform=%llu outcome_invalid_location=%llu "
                "outcome_rejected_opaque=%llu outcome_buffered=%llu "
                "outcome_shadow_changed=%llu outcome_shadow_unchanged=%llu matrix_logs=%llu "
                "program_overflow=%llu ubo_bindings=%llu ubo_ring_bindings=%llu "
                "ubo_buffer_sub_data_bindings=%llu ubo_fresh_submissions=%llu "
                "eligible_model_draws=%llu reject_same_present=%llu "
                "reject_family_limit=%llu reject_global_limit=%llu captures=%llu "
                "skinned_captures=%llu static_captures=%llu payload_match=%llu "
                "payload_mismatch=%llu payload_unknown=%llu payload_version_mismatch=%llu "
                "captures_with_mvp_call=%llu captures_with_palette_call=%llu "
                "captures_with_transform_call=%llu captures_with_opaque_location0=%llu "
                "max_captures_per_context=%u max_captures_per_family=%u "
                "rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
                reason ? reason : "unknown", summary.contextReady ? 1 : 0,
                summary.contextGeneration,
                static_cast<unsigned long long>(::MobilePZ::PZF7::gTraceState.ContextReadyEvents()),
                static_cast<unsigned long long>(::MobilePZ::PZF7::gTraceState.ContextDestroyedEvents()),
                static_cast<unsigned long long>(summary.presents),
                static_cast<unsigned long long>(summary.matrixCalls),
                static_cast<unsigned long long>(summary.modelMatrixCalls),
                static_cast<unsigned long long>(summary.matrixLocationZero),
                static_cast<unsigned long long>(summary.matrixLocationZeroOpaque),
                static_cast<unsigned long long>(summary.modelMatrixLocationZeroOpaque),
                static_cast<unsigned long long>(summary.matrixMvp),
                static_cast<unsigned long long>(summary.matrixPalette),
                static_cast<unsigned long long>(summary.matrixTransform),
                static_cast<unsigned long long>(summary.outcomeInvalidLocation),
                static_cast<unsigned long long>(summary.outcomeRejectedOpaque),
                static_cast<unsigned long long>(summary.outcomeBuffered),
                static_cast<unsigned long long>(summary.outcomeShadowChanged),
                static_cast<unsigned long long>(summary.outcomeShadowUnchanged),
                static_cast<unsigned long long>(summary.matrixLogs),
                static_cast<unsigned long long>(summary.programOverflow),
                static_cast<unsigned long long>(summary.uboBindings),
                static_cast<unsigned long long>(summary.uboRingBindings),
                static_cast<unsigned long long>(summary.uboBufferSubDataBindings),
                static_cast<unsigned long long>(summary.uboFreshSubmissions),
                static_cast<unsigned long long>(summary.eligibleModelDraws),
                static_cast<unsigned long long>(summary.rejectSamePresent),
                static_cast<unsigned long long>(summary.rejectFamilyLimit),
                static_cast<unsigned long long>(summary.rejectGlobalLimit),
                static_cast<unsigned long long>(summary.captures),
                static_cast<unsigned long long>(summary.skinnedCaptures),
                static_cast<unsigned long long>(summary.staticCaptures),
                static_cast<unsigned long long>(summary.payloadMatch),
                static_cast<unsigned long long>(summary.payloadMismatch),
                static_cast<unsigned long long>(summary.payloadUnknown),
                static_cast<unsigned long long>(summary.payloadVersionMismatch),
                static_cast<unsigned long long>(summary.capturesWithMvpCall),
                static_cast<unsigned long long>(summary.capturesWithPaletteCall),
                static_cast<unsigned long long>(summary.capturesWithTransformCall),
                static_cast<unsigned long long>(summary.capturesWithOpaqueLocationZero),
                ::MobilePZ::PZF7::TraceState::kMaxCaptures,
                ::MobilePZ::PZF7::TraceState::kMaxCapturesPerFamily);
    }

    static void PZF7OnContextReady(Uint contextGeneration) {
        if (!::MobilePZ::PZF7::gTraceState.OnContextReady(contextGeneration)) return;
        MGLOG_I("MOBILEPZ_PZF7_CONTEXT_READY generation=%u captures_reset=1 "
                "program_records_reset=1 rendering_output_mutation=0 additional_gl_calls=0",
                contextGeneration);
    }

    static void PZF7OnPresentBoundary() {
        ::MobilePZ::PZF7::gTraceState.OnPresentBoundary();
        if (g_pzf3PresentSerial == 1 || g_pzf3PresentSerial == 60 ||
            g_pzf3PresentSerial == 300 || g_pzf3PresentSerial == 600 ||
            g_pzf3PresentSerial == 1800 || g_pzf3PresentSerial == 3600) {
            PZF7LogSummary("present_milestone");
        }
    }

    static void PZF7OnContextDestroyed() {
        PZF7LogSummary("context_destroyed");
        ::MobilePZ::PZF7::gTraceState.OnContextDestroyed();
        MGLOG_I("MOBILEPZ_PZF7_CONTEXT_NOT_READY generation=%u context_ready=0 "
                "rendering_output_mutation=0 additional_gl_calls=0", g_backendContextGeneration);
    }
#endif

#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
#include "PZF4VisibilityProbe.inc"
#endif
#ifdef MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
#include "PZF12NullRenderTarget.inc"
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
#include "PZF13NullTextureProducer.inc"
#endif
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
#include "PZF14ClearToDetachTrace.inc"
#endif

    class PZF3DrawProbe {
    public:
        PZF3DrawProbe(const char* entry, GLenum mode, GLsizei count, GLenum indexType,
                      const void* indices, GLsizei instances = 1, GLint baseVertex = 0,
                      GLuint baseInstance = 0)
            : m_entry(entry ? entry : "unknown") {
            if (!MG_State::pGLContext) return;
            const auto& program = MG_State::pGLContext->GetProgramForDraw();
            PZF3ProgramRecord* record = PZF3ObserveProgram(program);
            if (!record) return;
            m_serial = ++g_pzf3DrawSerial;
            ++record->draws;
            ++g_pzf3FamilyDraws[PZF3FamilyIndex(record->family)];
            m_family = record->family;
            m_program = record->external;
            auto* backend = GetCurrentBackendProgram();
#ifdef MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
            PZF7CapturePreparedModelDraw(m_serial, *record, program, count, instances);
#endif
#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
            PZF5BeforeDraw(m_serial, *record, program, backend);
#endif
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
            PZF8BeforeDraw(m_serial, *record, program, backend, count, instances);
#endif
#ifdef MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
            PZF12BeforeDraw(m_serial, *record);
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
#ifndef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
            PZF13BeforeFramebufferWrite(m_entry, ::MobilePZ::PZF13::WriteKind::Draw);
#endif
            PZF13BeforeModelDraw(m_serial, *record, program, backend);
#endif
            PZF3LogProgramInventory(*record, program, backend);

            const Bool header = record->draws <= 2 ||
                (record->draws >= 64 && PZF3IsPowerOfTwo(record->draws));
            if (header && g_pzf3DrawHeaders < kPZF3MaxDrawHeaders) {
                ++g_pzf3DrawHeaders;
                m_logPost = true;
                GLint driverCurrent = 0;
                if (g_GLESFuncs.glGetIntegerv) g_GLESFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &driverCurrent);
                MGLOG_I("MOBILEPZ_PZF3_DRAW_PRE serial=%llu family=%s family_classifier=heuristic "
                        "program_draw=%llu entry=%s mode=0x%x count=%d indexed=%d index_type=0x%x "
                        "index_or_first=%zu instances=%d base_vertex=%d base_instance=%u "
                        "front_program=%u lifetime=%llu link=%u front_linked=%d front_spirv=%d "
                        "backend_program=%u backend_usable=%d backend_link=%u driver_current=%d "
                        "ubo_version=%u backend_uploaded_version=%u upload_version_match=%d",
                        static_cast<unsigned long long>(m_serial), PZF3FamilyName(record->family),
                        static_cast<unsigned long long>(record->draws), m_entry, mode, count,
                        indexType != 0 ? 1 : 0, indexType, reinterpret_cast<SizeT>(indices),
                        instances, baseVertex, baseInstance, record->external,
                        static_cast<unsigned long long>(record->lifetime), record->link,
                        program->GetLinkStatus() ? 1 : 0, program->GetSpirvStatus() ? 1 : 0,
                        backend ? backend->GetBackendProgramId() : 0,
                        backend && backend->IsBackendProgramUsable() ? 1 : 0,
                        backend ? backend->GetSyncedLinkVersion() : 0, driverCurrent,
                        program->GetUBOContentVersion(),
                        backend ? backend->GetLastUploadedGlobalUboVersion() : 0,
                        backend && backend->GetLastUploadedGlobalUboVersion() == program->GetUBOContentVersion()
                            ? 1 : 0);
            }
            if (PZF3ReserveDetailed(*record)) {
                PZF3LogDetailedDraw(m_serial, *record, program, backend);
            }
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
            m_pzf4Token = PZF4BeginDraw(m_serial, *record, program, backend, mode, count,
                                        indexType, indices, instances, baseVertex, baseInstance);
#endif
#if defined(MOBILEPZ_PZF6_DRAW_DELTA) && !defined(MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA)
            m_pzf6Token = PZF6BeginDraw(m_serial, *record);
#endif
        }

        ~PZF3DrawProbe() {
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
            PZF4EndDraw(m_pzf4Token);
#endif
#if defined(MOBILEPZ_PZF6_DRAW_DELTA) && !defined(MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA)
            PZF6EndDraw(m_pzf6Token);
#endif
#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
            PZF5AfterDraw(m_serial, m_family, m_program);
#endif
            if (!m_logPost) return;
            MGLOG_I("MOBILEPZ_PZF3_DRAW_POST serial=%llu family=%s program=%u entry=%s "
                    "driver_call_returned=1 error_drain=0",
                    static_cast<unsigned long long>(m_serial), PZF3FamilyName(m_family),
                    m_program, m_entry);
        }

    private:
        const char* m_entry = "unknown";
        Uint64 m_serial = 0;
        PZF3Family m_family = PZF3Family::Other;
        Uint m_program = 0;
        Bool m_logPost = false;
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
        Int m_pzf4Token = -1;
#endif
#if defined(MOBILEPZ_PZF6_DRAW_DELTA) && !defined(MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA)
        PZF6DrawDeltaToken m_pzf6Token;
#endif
    };

    static void PZF3LogSummary(const char* reason) {
        MGLOG_I("MOBILEPZ_PZF3_SUMMARY reason=%s presents=%llu draws=%llu programs=%zu "
                "program_overflow=%llu draw_headers=%llu detailed=%llu "
                "skinned_programs=%llu skinned_draws=%llu static_programs=%llu static_draws=%llu "
                "world_programs=%llu world_draws=%llu textured_programs=%llu textured_draws=%llu "
                "other_programs=%llu other_draws=%llu",
                reason ? reason : "unknown", static_cast<unsigned long long>(g_pzf3PresentSerial),
                static_cast<unsigned long long>(g_pzf3DrawSerial), g_pzf3ProgramCount,
                static_cast<unsigned long long>(g_pzf3ProgramOverflow),
                static_cast<unsigned long long>(g_pzf3DrawHeaders),
                static_cast<unsigned long long>(g_pzf3DetailedCaptures),
                static_cast<unsigned long long>(g_pzf3FamilyPrograms[0]),
                static_cast<unsigned long long>(g_pzf3FamilyDraws[0]),
                static_cast<unsigned long long>(g_pzf3FamilyPrograms[1]),
                static_cast<unsigned long long>(g_pzf3FamilyDraws[1]),
                static_cast<unsigned long long>(g_pzf3FamilyPrograms[2]),
                static_cast<unsigned long long>(g_pzf3FamilyDraws[2]),
                static_cast<unsigned long long>(g_pzf3FamilyPrograms[3]),
                static_cast<unsigned long long>(g_pzf3FamilyDraws[3]),
                static_cast<unsigned long long>(g_pzf3FamilyPrograms[4]),
                static_cast<unsigned long long>(g_pzf3FamilyDraws[4]));
    }

    static void PZF3OnPresentBoundary() {
        ++g_pzf3PresentSerial;
#ifdef MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
        PZF7OnPresentBoundary();
#endif
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
        PZF4OnPresentBoundary();
#endif
#ifdef MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
        PZF12OnPresentBoundary();
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
        PZF13OnPresentBoundary();
#endif
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
        PZF14OnPresentBoundary();
#endif
        if (g_pzf3PresentSerial == 1 || g_pzf3PresentSerial == 60 ||
            g_pzf3PresentSerial == 300 || g_pzf3PresentSerial == 600 ||
            g_pzf3PresentSerial == 1800 || g_pzf3PresentSerial == 3600) {
            PZF3LogSummary("present_milestone");
        }
    }

    static void PZF3OnContextReady(Uint contextGeneration) {
#ifdef MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
        PZF7OnContextReady(contextGeneration);
#endif
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
        PZF4OnContextReady(contextGeneration);
#else
        (void)contextGeneration;
#endif
#ifdef MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
        PZF12OnContextReady(contextGeneration);
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
        PZF13OnContextReady(contextGeneration);
#endif
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
        PZF14OnContextReady(contextGeneration);
#endif
    }

    static void PZF3OnContextDestroyed() {
#ifdef MOBILEPZ_PZF4_SAMPLE_SURVIVAL_TRACE
        PZF4OnContextDestroyed();
#endif
#ifdef MOBILEPZ_PZF7_UNIFORM_TO_NATIVE
        PZF7OnContextDestroyed();
#endif
#ifdef MOBILEPZ_PZF12_NULL_RT_IDENTITY_REPAIR
        PZF12OnContextDestroyed();
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
        PZF13OnContextDestroyed();
#endif
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
        PZF14OnContextDestroyed();
#endif
        PZF3LogSummary("context_destroyed");
    }
} // namespace
