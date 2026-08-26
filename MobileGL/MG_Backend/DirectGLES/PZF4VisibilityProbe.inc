// MobilePZ PZF4 - bounded draw identity, texture-shadow alpha and fragment-survival probe.
// Included by PZF3CrossFamilyTrace.inc inside its anonymous namespace. PZF4 deliberately
// reuses PZF3's already-device-proven program classifier and never changes render state.

constexpr SizeT kPZF4QuerySlots = 32;
constexpr SizeT kPZF4MaxVertexSamples = 8192;
constexpr SizeT kPZF4MaxIndexScans = 65536;
constexpr SizeT kPZF4MaxTextureByteSamples = 65536;
constexpr SizeT kPZF4MaxAlphaSamples = 4096;

struct PZF4IndexStats {
    Bool readable = false;
    Bool indexed = false;
    Bool defined = false;
    Uint64 hash = 0;
    SizeT byteOffset = 0;
    SizeT byteCount = 0;
    SizeT scanned = 0;
    Uint restartCount = 0;
    Int64 minVertex = 0;
    Int64 maxVertex = 0;
    Vector<Int64> vertices;
};

struct PZF4AttributeStats {
    Bool found = false;
    Bool readable = false;
    Bool finite = true;
    Uint location = 0;
    String name;
    GLenum type = 0;
    Int components = 0;
    Int stride = 0;
    SizeT offset = 0;
    Uint samples = 0;
    Uint outOfBounds = 0;
    Uint nonFinite = 0;
    Uint64 hash = 0;
    Array<Float, 4> lo{};
    Array<Float, 4> hi{};
};

struct PZF4DrawData {
    Uint64 drawId = 0;
    Uint64 geometryHash = 0;
    Uint64 textureHash = 0;
    Uint64 uboHash = 0;
    GLint drawFbo = 0;
    GLint outputType = GL_NONE;
    GLint outputName = 0;
    GLint outputLevel = -1;
    GLint outputLayer = -1;
};

struct PZF4QueryRecord {
    GLuint id = 0;
    Bool ended = false;
    Uint contextGeneration = 0;
    Uint64 drawId = 0;
    Uint64 serial = 0;
    Uint64 signature = 0;
    Uint64 geometryHash = 0;
    Uint64 textureHash = 0;
    Uint64 beginPresent = 0;
    PZF3Family family = PZF3Family::Other;
    Uint program = 0;
    GLint drawFbo = 0;
    GLint outputType = GL_NONE;
    GLint outputName = 0;
};

static Array<PZF4QueryRecord, kPZF4QuerySlots> g_pzf4Queries{};
static SizeT g_pzf4PollCursor = 0;
static Uint g_pzf4PendingQueries = 0;
static Uint64 g_pzf4QueryStarted = 0;
static Uint64 g_pzf4QueryUnsupported = 0;
static Uint64 g_pzf4QueryConflict = 0;
static Uint64 g_pzf4QuerySlotFull = 0;
static Uint64 g_pzf4QueryResults = 0;
static Uint64 g_pzf4SamplesPassed = 0;
static Uint64 g_pzf4ZeroSamples = 0;
static Array<Uint64, kPZF3FamilyCount> g_pzf4FamilyResults{};
static Array<Uint64, kPZF3FamilyCount> g_pzf4FamilyPassed{};
static ::MobilePZ::PZF4R1::LifecycleState g_pzf4Lifecycle;

static_assert(::MobilePZ::PZF4R1::LifecycleState::kFamilyCount == kPZF3FamilyCount);

#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
constexpr Uint kPZF5MaxCapturesPerContext = 16;
constexpr GLint kPZF5ReadRows = 24;
constexpr GLint kPZF5MaxReadWidth = 8192;

struct PZF5LineageState {
    Bool valid = false;
    Uint contextGeneration = 0;
    GLint fbo = 0;
    GLint texture = 0;
    GLint level = -1;
    GLint layer = -1;
    Array<GLint, 4> viewport{};
    Uint64 lastProducerSerial = 0;
    PZF3Family lastProducerFamily = PZF3Family::Other;
    Uint lastProducerProgram = 0;
    Uint64 skinnedWrites = 0;
    Uint64 staticWrites = 0;
};

static PZF5LineageState g_pzf5Lineage;
static Uint g_pzf5ContextCaptures = 0;
static Uint64 g_pzf5LastCapturePresent = std::numeric_limits<Uint64>::max();
static Uint64 g_pzf5ConsumerChecks = 0;
static Uint64 g_pzf5LineageMatches = 0;
static Uint64 g_pzf5ReadbackAttempts = 0;
static Uint64 g_pzf5ReadbackCaptures = 0;
static Uint64 g_pzf5ReadbackUnsupported = 0;
static Uint64 g_pzf5ReadbackIncomplete = 0;
static Uint64 g_pzf5ReadbackEmpty = 0;
static Uint64 g_pzf5CapturedPixels = 0;
static Uint64 g_pzf5CapturedZeroRgba = 0;
static Uint64 g_pzf5CapturedRgbNonzero = 0;
static Uint64 g_pzf5CapturedAlphaNonzero = 0;
static Uint64 g_pzf5CapturedNonTransparentColour = 0;

#ifdef MOBILEPZ_PZF6_DRAW_DELTA
constexpr Uint kPZF6MaxPairsPerContext = 16;
constexpr Uint kPZF6MaxPairsPerFamily = 8;
constexpr GLint kPZF6ReadRows = 48;
constexpr GLint kPZF6MaxReadWidth = 8192;

struct PZF6DrawDeltaToken {
    Bool active = false;
    Uint capture = 0;
    Uint contextGeneration = 0;
    Uint rowPhase = 0;
    Uint64 serial = 0;
    PZF3Family family = PZF3Family::Other;
    Uint program = 0;
    GLint fbo = 0;
    GLint texture = 0;
    GLint level = -1;
    GLint layer = -1;
    Array<GLint, 4> viewport{};
    GLint readX = 0;
    GLint readWidth = 0;
    Vector<GLint> rowY;
    Vector<Uint8> before;
    Uint beforeSentinelRows = 0;
    GLenum beforeFboStatus = 0;
};

static Uint g_pzf6ContextPairs = 0;
static Array<Uint, 2> g_pzf6ContextFamilyPairs{};
static Uint64 g_pzf6LastPairPresent = std::numeric_limits<Uint64>::max();
static Uint64 g_pzf6EligibleDraws = 0;
static Uint64 g_pzf6BeginAttempts = 0;
static Uint64 g_pzf6CompletedPairs = 0;
static Uint64 g_pzf6ChangedPairs = 0;
static Uint64 g_pzf6NoChangePairs = 0;
static Uint64 g_pzf6ReadbackUnsupported = 0;
static Uint64 g_pzf6ReadbackIncomplete = 0;
static Uint64 g_pzf6ReadbackEmpty = 0;
static Uint64 g_pzf6TopologyChanged = 0;
static Uint64 g_pzf6Pixels = 0;
static Uint64 g_pzf6ChangedPixels = 0;
static Uint64 g_pzf6RgbChangedPixels = 0;
static Uint64 g_pzf6AlphaChangedPixels = 0;
static Uint64 g_pzf6NonTransparentGain = 0;
static Uint64 g_pzf6NonTransparentLoss = 0;
static Array<Uint64, 2> g_pzf6FamilyCompleted{};
#endif
#endif

static Uint64 PZF4Mix(Uint64 hash, Uint64 value) {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31;
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    return hash;
}

static Uint64 PZF4HashSampledBytes(const Uint8* bytes, SizeT size, SizeT limit) {
    if (!bytes || size == 0 || limit == 0) return 0;
    const SizeT samples = std::min(size, limit);
    const SizeT step = std::max<SizeT>(1, size / samples);
    Uint64 hash = 1469598103934665603ull;
    SizeT sampled = 0;
    for (SizeT offset = 0; offset < size && sampled < samples; offset += step, ++sampled) {
        hash ^= bytes[offset];
        hash *= 1099511628211ull;
    }
    hash = PZF4Mix(hash, size);
    return PZF4Mix(hash, sampled);
}

static Bool PZF4NameContainsAsciiNoCase(const String& value, const char* needle) {
    if (!needle || needle[0] == '\0') return false;
    const SizeT needleSize = std::strlen(needle);
    if (value.size() < needleSize) return false;
    for (SizeT start = 0; start + needleSize <= value.size(); ++start) {
        Bool match = true;
        for (SizeT i = 0; i < needleSize; ++i) {
            const char a = value[start + i] >= 'A' && value[start + i] <= 'Z'
                ? static_cast<char>(value[start + i] - 'A' + 'a') : value[start + i];
            const char b = needle[i] >= 'A' && needle[i] <= 'Z'
                ? static_cast<char>(needle[i] - 'A' + 'a') : needle[i];
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static Bool PZF4IsUvName(const String& name) {
    return PZF4NameContainsAsciiNoCase(name, "uv") ||
           PZF4NameContainsAsciiNoCase(name, "texcoord");
}

static Bool PZF4IsPositionName(const String& name) {
    return PZF4NameContainsAsciiNoCase(name, "vertex") ||
           PZF4NameContainsAsciiNoCase(name, "vpos") ||
           PZF4NameContainsAsciiNoCase(name, "position");
}

static SharedPtr<MG_State::GLState::ITextureObject> PZF4ResolveSamplerTexture(
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    const PrgramImpl::BackendProgramObjectImpl::SamplerUniformBinding& binding) {
    if (!program || !MG_State::pGLContext ||
        !program->IsValidUniformLocation(static_cast<Int>(binding.frontendLocation))) return {};
    const Int unit = program->GetUniformSamplerOrImageUnitIndex(binding.frontendLocation);
    const TextureTarget target = SamplerUniformTextureTarget(binding.uniformType);
    if (unit < 0 || unit >= MG_State::GLState::TextureState::MAX_TEXTURE_IMAGE_UNITS ||
        target == TextureTarget::Unknown) return {};
    return MG_State::pGLContext->GetTextureUnitObject(unit).GetBindingSlot(target).GetBoundObject();
}

#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
static Bool PZF5FindSamplerLineage(
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend, GLint nativeTexture,
    Uint& samplerOrdinal, Uint& frontendTexture, Uint64& textureLifetime) {
    samplerOrdinal = 0;
    frontendTexture = 0;
    textureLifetime = 0;
    if (!program || !backend || nativeTexture <= 0) return false;
    Uint ordinal = 0;
    for (const auto& binding : backend->GetSamplerUniformBindings()) {
        if (ordinal >= 8) break;
        const auto texture = PZF4ResolveSamplerTexture(program, binding);
        Uint backendTexture = 0;
        if (texture) {
            auto* slot = TextureImpl::g_backendTextureObjects.Find(texture.get());
            if (slot && *slot) backendTexture = (*slot)->GetBackendTextureId();
        }
        if (static_cast<GLint>(backendTexture) == nativeTexture) {
            samplerOrdinal = ordinal;
            frontendTexture = texture ? texture->GetExternalIndex() : 0;
            textureLifetime = texture ? texture->GetLifetimeId() : 0;
            return true;
        }
        ++ordinal;
    }
    return false;
}

static Bool PZF5CanReadback() {
    return g_GLESFuncs.glGetIntegerv && g_GLESFuncs.glBindFramebuffer &&
           g_GLESFuncs.glCheckFramebufferStatus && g_GLESFuncs.glReadBuffer &&
           g_GLESFuncs.glBindBuffer && g_GLESFuncs.glPixelStorei &&
           g_GLESFuncs.glReadPixels;
}

static Bool PZF5ReadProducerTarget(
    Uint64 consumerSerial, const PZF3ProgramRecord& consumer,
    Uint samplerOrdinal, Uint frontendTexture, Uint64 textureLifetime) {
    ++g_pzf5ReadbackAttempts;
    if (!PZF5CanReadback()) {
        ++g_pzf5ReadbackUnsupported;
        MGLOG_I("MOBILEPZ_PZF5_RENDER_TARGET_READBACK capture=%llu generation=%u "
                "status=unsupported producer_fbo=%d producer_texture=%d "
                "consumer_serial=%llu consumer_family=%s consumer_program=%u "
                "blocking_readback=0 state_restored=1 rendering_output_mutation=0 error_drain=0",
                static_cast<unsigned long long>(g_pzf5ReadbackCaptures + 1),
                g_backendContextGeneration, g_pzf5Lineage.fbo, g_pzf5Lineage.texture,
                static_cast<unsigned long long>(consumerSerial),
                PZF3FamilyName(consumer.family), consumer.external);
        return false;
    }

    GLint x = std::max<GLint>(0, g_pzf5Lineage.viewport[0]);
    GLint y = std::max<GLint>(0, g_pzf5Lineage.viewport[1]);
    GLint width = g_pzf5Lineage.viewport[2] - (x - g_pzf5Lineage.viewport[0]);
    GLint height = g_pzf5Lineage.viewport[3] - (y - g_pzf5Lineage.viewport[1]);
    if (width > kPZF5MaxReadWidth) {
        x += (width - kPZF5MaxReadWidth) / 2;
        width = kPZF5MaxReadWidth;
    }
    if (width <= 0 || height <= 0 || width > std::numeric_limits<GLint>::max() / 4) {
        ++g_pzf5ReadbackEmpty;
        return false;
    }

    GLint previousReadFbo = 0;
    GLint previousPackBuffer = 0;
    GLint previousPackAlignment = 4;
    GLint previousPackRowLength = 0;
    GLint previousPackSkipRows = 0;
    GLint previousPackSkipPixels = 0;
    GLint previousReadBuffer = GL_BACK;
    g_GLESFuncs.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFbo);
    g_GLESFuncs.glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
    g_GLESFuncs.glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    g_GLESFuncs.glGetIntegerv(GL_PACK_ROW_LENGTH, &previousPackRowLength);
    g_GLESFuncs.glGetIntegerv(GL_PACK_SKIP_ROWS, &previousPackSkipRows);
    g_GLESFuncs.glGetIntegerv(GL_PACK_SKIP_PIXELS, &previousPackSkipPixels);
    g_GLESFuncs.glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);

    g_GLESFuncs.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(g_pzf5Lineage.fbo));
    GLint producerReadBuffer = GL_COLOR_ATTACHMENT0;
    g_GLESFuncs.glGetIntegerv(GL_READ_BUFFER, &producerReadBuffer);
    const GLenum framebufferStatus = g_GLESFuncs.glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    ::MobilePZ::PZF5::ReadbackStats stats;
    Uint sentinelRows = 0;
    const GLint rows = std::min(kPZF5ReadRows, height);
    if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE) {
        g_GLESFuncs.glReadBuffer(GL_COLOR_ATTACHMENT0);
        g_GLESFuncs.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        g_GLESFuncs.glPixelStorei(GL_PACK_ALIGNMENT, 1);
        g_GLESFuncs.glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        Vector<Uint8> row(static_cast<SizeT>(width) * 4u);
        for (GLint rowIndex = 0; rowIndex < rows; ++rowIndex) {
            const GLint readY = rows == 1 ? y : y + rowIndex * (height - 1) / (rows - 1);
            std::fill(row.begin(), row.end(), static_cast<Uint8>(0xa5));
            g_GLESFuncs.glReadPixels(x, readY, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, row.data());
            const Bool unchanged = std::all_of(row.begin(), row.end(), [](Uint8 value) {
                return value == static_cast<Uint8>(0xa5);
            });
            if (unchanged) {
                ++sentinelRows;
            } else {
                stats.Observe(row.data(), static_cast<SizeT>(width));
            }
        }
    } else {
        ++g_pzf5ReadbackIncomplete;
    }

    g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_PIXELS, previousPackSkipPixels);
    g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_ROWS, previousPackSkipRows);
    g_GLESFuncs.glPixelStorei(GL_PACK_ROW_LENGTH, previousPackRowLength);
    g_GLESFuncs.glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    g_GLESFuncs.glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
    g_GLESFuncs.glReadBuffer(static_cast<GLenum>(producerReadBuffer));
    g_GLESFuncs.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFbo));
    g_GLESFuncs.glReadBuffer(static_cast<GLenum>(previousReadBuffer));

    if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE && stats.pixels == 0) {
        ++g_pzf5ReadbackEmpty;
    }
    if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE && stats.pixels != 0) {
        ++g_pzf5ReadbackCaptures;
        g_pzf5CapturedPixels += stats.pixels;
        g_pzf5CapturedZeroRgba += stats.zeroRgba;
        g_pzf5CapturedRgbNonzero += stats.rgbNonzero;
        g_pzf5CapturedAlphaNonzero += stats.alphaMid + stats.alphaFull;
        g_pzf5CapturedNonTransparentColour += stats.nonTransparentColour;
    }

    const double meanR = stats.pixels ? static_cast<double>(stats.channelSum[0]) / stats.pixels : 0.0;
    const double meanG = stats.pixels ? static_cast<double>(stats.channelSum[1]) / stats.pixels : 0.0;
    const double meanB = stats.pixels ? static_cast<double>(stats.channelSum[2]) / stats.pixels : 0.0;
    const double meanA = stats.pixels ? static_cast<double>(stats.channelSum[3]) / stats.pixels : 0.0;
    MGLOG_I("MOBILEPZ_PZF5_RENDER_TARGET_READBACK capture=%llu generation=%u status=%s "
            "producer_serial=%llu producer_family=%s producer_program=%u producer_fbo=%d "
            "producer_texture=%d producer_level=%d producer_layer=%d "
            "producer_writes_skinned=%llu producer_writes_static=%llu "
            "consumer_serial=%llu consumer_family=%s consumer_program=%u "
            "consumer_sampler_ordinal=%u consumer_front_texture=%u consumer_texture_lifetime=%llu "
            "viewport=%d,%d,%d,%d rows=%d sentinel_rows=%u pixels=%llu zero_rgba=%llu "
            "rgb_nonzero=%llu alpha_zero=%llu alpha_mid=%llu alpha_full=%llu "
            "nontransparent_colour=%llu rgba_min=%u,%u,%u,%u rgba_max=%u,%u,%u,%u "
            "rgba_mean=%g,%g,%g,%g rgba_hash=%016llx fbo_status=0x%x "
            "blocking_readback=1 state_restored=1 rendering_output_mutation=0 error_drain=0",
            static_cast<unsigned long long>(g_pzf5ReadbackCaptures), g_backendContextGeneration,
            framebufferStatus == GL_FRAMEBUFFER_COMPLETE && stats.pixels ? "captured" : "no_data",
            static_cast<unsigned long long>(g_pzf5Lineage.lastProducerSerial),
            PZF3FamilyName(g_pzf5Lineage.lastProducerFamily), g_pzf5Lineage.lastProducerProgram,
            g_pzf5Lineage.fbo, g_pzf5Lineage.texture, g_pzf5Lineage.level, g_pzf5Lineage.layer,
            static_cast<unsigned long long>(g_pzf5Lineage.skinnedWrites),
            static_cast<unsigned long long>(g_pzf5Lineage.staticWrites),
            static_cast<unsigned long long>(consumerSerial), PZF3FamilyName(consumer.family),
            consumer.external, samplerOrdinal, frontendTexture,
            static_cast<unsigned long long>(textureLifetime), x, y, width, height, rows,
            sentinelRows, static_cast<unsigned long long>(stats.pixels),
            static_cast<unsigned long long>(stats.zeroRgba),
            static_cast<unsigned long long>(stats.rgbNonzero),
            static_cast<unsigned long long>(stats.alphaZero),
            static_cast<unsigned long long>(stats.alphaMid),
            static_cast<unsigned long long>(stats.alphaFull),
            static_cast<unsigned long long>(stats.nonTransparentColour),
            stats.pixels ? static_cast<Uint>(stats.channelMin[0]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMin[1]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMin[2]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMin[3]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMax[0]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMax[1]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMax[2]) : 0u,
            stats.pixels ? static_cast<Uint>(stats.channelMax[3]) : 0u,
            meanR, meanG, meanB, meanA, static_cast<unsigned long long>(stats.hash), framebufferStatus);
    return framebufferStatus == GL_FRAMEBUFFER_COMPLETE && stats.pixels != 0;
}

#ifdef MOBILEPZ_PZF6_DRAW_DELTA
static Int PZF6FamilySlot(PZF3Family family) {
    if (family == PZF3Family::Skinned) return 0;
    if (family == PZF3Family::StaticModel) return 1;
    return -1;
}

static Bool PZF6ReadRows(
    const PZF6DrawDeltaToken& token, Vector<Uint8>& rgba,
    Uint& sentinelRows, GLenum& framebufferStatus) {
    sentinelRows = 0;
    framebufferStatus = 0;
    rgba.clear();
    if (!PZF5CanReadback() || token.fbo <= 0 || token.readWidth <= 0 ||
        token.rowY.empty()) return false;

    GLint previousReadFbo = 0;
    GLint previousPackBuffer = 0;
    GLint previousPackAlignment = 4;
    GLint previousPackRowLength = 0;
    GLint previousPackSkipRows = 0;
    GLint previousPackSkipPixels = 0;
    GLint previousReadBuffer = GL_BACK;
    g_GLESFuncs.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFbo);
    g_GLESFuncs.glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
    g_GLESFuncs.glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    g_GLESFuncs.glGetIntegerv(GL_PACK_ROW_LENGTH, &previousPackRowLength);
    g_GLESFuncs.glGetIntegerv(GL_PACK_SKIP_ROWS, &previousPackSkipRows);
    g_GLESFuncs.glGetIntegerv(GL_PACK_SKIP_PIXELS, &previousPackSkipPixels);
    g_GLESFuncs.glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);

    g_GLESFuncs.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(token.fbo));
    GLint targetReadBuffer = GL_COLOR_ATTACHMENT0;
    g_GLESFuncs.glGetIntegerv(GL_READ_BUFFER, &targetReadBuffer);
    framebufferStatus = g_GLESFuncs.glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE) {
        g_GLESFuncs.glReadBuffer(GL_COLOR_ATTACHMENT0);
        g_GLESFuncs.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        g_GLESFuncs.glPixelStorei(GL_PACK_ALIGNMENT, 1);
        g_GLESFuncs.glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        const SizeT rowBytes = static_cast<SizeT>(token.readWidth) * 4u;
        rgba.resize(rowBytes * token.rowY.size());
        for (SizeT rowIndex = 0; rowIndex < token.rowY.size(); ++rowIndex) {
            Uint8* row = rgba.data() + rowIndex * rowBytes;
            std::fill(row, row + rowBytes, static_cast<Uint8>(0xa5));
            g_GLESFuncs.glReadPixels(token.readX, token.rowY[rowIndex], token.readWidth, 1,
                                     GL_RGBA, GL_UNSIGNED_BYTE, row);
            if (std::all_of(row, row + rowBytes, [](Uint8 value) {
                    return value == static_cast<Uint8>(0xa5);
                })) {
                ++sentinelRows;
            }
        }
    }

    g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_PIXELS, previousPackSkipPixels);
    g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_ROWS, previousPackSkipRows);
    g_GLESFuncs.glPixelStorei(GL_PACK_ROW_LENGTH, previousPackRowLength);
    g_GLESFuncs.glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    g_GLESFuncs.glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
    g_GLESFuncs.glReadBuffer(static_cast<GLenum>(targetReadBuffer));
    g_GLESFuncs.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFbo));
    g_GLESFuncs.glReadBuffer(static_cast<GLenum>(previousReadBuffer));

    return framebufferStatus == GL_FRAMEBUFFER_COMPLETE && !rgba.empty() &&
           sentinelRows == 0;
}

static PZF6DrawDeltaToken PZF6BeginDraw(
    Uint64 serial, const PZF3ProgramRecord& record) {
    PZF6DrawDeltaToken token;
    const Int familySlot = PZF6FamilySlot(record.family);
    if (familySlot < 0 ||
        !g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration) ||
        g_pzf6ContextPairs >= kPZF6MaxPairsPerContext ||
        g_pzf6ContextFamilyPairs[static_cast<SizeT>(familySlot)] >= kPZF6MaxPairsPerFamily ||
        g_pzf6LastPairPresent == g_pzf3PresentSerial) return token;
    ++g_pzf6EligibleDraws;

    if (!PZF5CanReadback() || !g_GLESFuncs.glGetFramebufferAttachmentParameteriv) {
        ++g_pzf6ReadbackUnsupported;
        g_pzf6LastPairPresent = g_pzf3PresentSerial;
        MGLOG_I("MOBILEPZ_PZF6_DRAW_DELTA capture=%u generation=%u status=unsupported "
                "serial=%llu family=%s program=%u present=%llu blocking_readback=0 "
                "same_regions=0 state_restored=1 rendering_output_mutation=0 error_drain=0",
                g_pzf6ContextPairs + 1, g_backendContextGeneration,
                static_cast<unsigned long long>(serial), PZF3FamilyName(record.family),
                record.external, static_cast<unsigned long long>(g_pzf3PresentSerial));
        return token;
    }

    GLint outputType = GL_NONE;
    Array<GLint, 4> viewport{};
    g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &token.fbo);
    if (token.fbo == 0) return token;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &outputType);
    if (outputType != GL_TEXTURE) return token;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &token.texture);
    if (token.texture == 0) return token;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &token.level);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &token.layer);
    g_GLESFuncs.glGetIntegerv(GL_VIEWPORT, viewport.data());

    token.viewport = viewport;
    token.readX = std::max<GLint>(0, viewport[0]);
    token.readWidth = viewport[2] - (token.readX - viewport[0]);
    const GLint readY = std::max<GLint>(0, viewport[1]);
    const GLint readHeight = viewport[3] - (readY - viewport[1]);
    if (token.readWidth > kPZF6MaxReadWidth) {
        token.readX += (token.readWidth - kPZF6MaxReadWidth) / 2;
        token.readWidth = kPZF6MaxReadWidth;
    }
    if (token.readWidth <= 0 || readHeight <= 0) {
        ++g_pzf6ReadbackEmpty;
        return {};
    }

    token.capture = g_pzf6ContextPairs + 1;
    token.contextGeneration = g_backendContextGeneration;
    token.rowPhase = g_pzf6ContextPairs;
    token.serial = serial;
    token.family = record.family;
    token.program = record.external;
    const GLint rows = std::min(kPZF6ReadRows, readHeight);
    token.rowY.reserve(static_cast<SizeT>(rows));
    for (GLint rowIndex = 0; rowIndex < rows; ++rowIndex) {
        const GLint stratumBegin = rowIndex * readHeight / rows;
        const GLint stratumEnd = (rowIndex + 1) * readHeight / rows;
        const GLint stratumHeight = std::max<GLint>(1, stratumEnd - stratumBegin);
        const GLint offset = static_cast<GLint>(
            (token.rowPhase * 17u + static_cast<Uint>(rowIndex) * 7u) %
            static_cast<Uint>(stratumHeight));
        token.rowY.push_back(readY + stratumBegin + offset);
    }

    ++g_pzf6BeginAttempts;
    if (!PZF6ReadRows(token, token.before, token.beforeSentinelRows,
                      token.beforeFboStatus)) {
        if (token.beforeFboStatus != GL_FRAMEBUFFER_COMPLETE) ++g_pzf6ReadbackIncomplete;
        else ++g_pzf6ReadbackEmpty;
        g_pzf6LastPairPresent = g_pzf3PresentSerial;
        MGLOG_I("MOBILEPZ_PZF6_DRAW_DELTA capture=%u generation=%u status=before_no_data "
                "serial=%llu family=%s program=%u present=%llu draw_fbo=%d texture=%d "
                "rows=%zu sentinel_rows=%u fbo_status=0x%x blocking_readback=1 "
                "same_regions=0 state_restored=1 rendering_output_mutation=0 error_drain=0",
                token.capture, g_backendContextGeneration,
                static_cast<unsigned long long>(serial), PZF3FamilyName(record.family),
                record.external, static_cast<unsigned long long>(g_pzf3PresentSerial),
                token.fbo, token.texture, token.rowY.size(), token.beforeSentinelRows,
                token.beforeFboStatus);
        return {};
    }

    token.active = true;
    ++g_pzf6ContextPairs;
    ++g_pzf6ContextFamilyPairs[static_cast<SizeT>(familySlot)];
    g_pzf6LastPairPresent = g_pzf3PresentSerial;
    return token;
}

static void PZF6EndDraw(PZF6DrawDeltaToken& token) {
    if (!token.active) return;
    token.active = false;
    GLint drawFbo = 0;
    GLint outputType = GL_NONE;
    GLint outputName = 0;
    GLint outputLevel = -1;
    GLint outputLayer = -1;
    if (token.contextGeneration != g_backendContextGeneration ||
        !g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration) ||
        !g_GLESFuncs.glGetIntegerv || !g_GLESFuncs.glGetFramebufferAttachmentParameteriv) {
        ++g_pzf6TopologyChanged;
        return;
    }
    g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &outputType);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &outputName);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &outputLevel);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &outputLayer);
    if (drawFbo != token.fbo || outputType != GL_TEXTURE || outputName != token.texture ||
        outputLevel != token.level || outputLayer != token.layer) {
        ++g_pzf6TopologyChanged;
        MGLOG_I("MOBILEPZ_PZF6_DRAW_DELTA capture=%u generation=%u status=topology_changed "
                "serial=%llu family=%s program=%u before_fbo=%d after_fbo=%d "
                "before_texture=%d after_texture=%d before_level_layer=%d,%d "
                "after_level_layer=%d,%d blocking_readback=1 same_regions=0 "
                "state_restored=1 rendering_output_mutation=0 error_drain=0",
                token.capture, g_backendContextGeneration,
                static_cast<unsigned long long>(token.serial), PZF3FamilyName(token.family),
                token.program, token.fbo, drawFbo, token.texture, outputName,
                token.level, token.layer, outputLevel, outputLayer);
        return;
    }

    Vector<Uint8> after;
    Uint afterSentinelRows = 0;
    GLenum afterFboStatus = 0;
    if (!PZF6ReadRows(token, after, afterSentinelRows, afterFboStatus) ||
        after.size() != token.before.size()) {
        if (afterFboStatus != GL_FRAMEBUFFER_COMPLETE) ++g_pzf6ReadbackIncomplete;
        else ++g_pzf6ReadbackEmpty;
        MGLOG_I("MOBILEPZ_PZF6_DRAW_DELTA capture=%u generation=%u status=after_no_data "
                "serial=%llu family=%s program=%u draw_fbo=%d texture=%d rows=%zu "
                "before_bytes=%zu after_bytes=%zu sentinel_rows_before=%u "
                "sentinel_rows_after=%u fbo_status_before=0x%x fbo_status_after=0x%x "
                "blocking_readback=1 same_regions=1 state_restored=1 "
                "rendering_output_mutation=0 error_drain=0",
                token.capture, g_backendContextGeneration,
                static_cast<unsigned long long>(token.serial), PZF3FamilyName(token.family),
                token.program, token.fbo, token.texture, token.rowY.size(),
                token.before.size(), after.size(), token.beforeSentinelRows,
                afterSentinelRows, token.beforeFboStatus, afterFboStatus);
        return;
    }

    ::MobilePZ::PZF5::ReadbackStats beforeStats;
    ::MobilePZ::PZF5::ReadbackStats afterStats;
    ::MobilePZ::PZF6::DrawDeltaStats delta;
    const SizeT pixels = token.before.size() / 4u;
    beforeStats.Observe(token.before.data(), pixels);
    afterStats.Observe(after.data(), pixels);
    delta.Observe(token.before.data(), after.data(), pixels);
    ++g_pzf6CompletedPairs;
    const Int familySlot = PZF6FamilySlot(token.family);
    if (familySlot >= 0) ++g_pzf6FamilyCompleted[static_cast<SizeT>(familySlot)];
    if (delta.changed != 0) ++g_pzf6ChangedPairs;
    else ++g_pzf6NoChangePairs;
    g_pzf6Pixels += delta.pixels;
    g_pzf6ChangedPixels += delta.changed;
    g_pzf6RgbChangedPixels += delta.rgbChanged;
    g_pzf6AlphaChangedPixels += delta.alphaChanged;
    g_pzf6NonTransparentGain += delta.nonTransparentGain;
    g_pzf6NonTransparentLoss += delta.nonTransparentLoss;

    Uint64 rowHash = 0x505a463600000001ull;
    for (GLint row : token.rowY) rowHash = PZF4Mix(rowHash, static_cast<Uint64>(row));
    const double changedPercent = delta.pixels
        ? 100.0 * static_cast<double>(delta.changed) / static_cast<double>(delta.pixels) : 0.0;
    const double meanAbsR = delta.pixels
        ? static_cast<double>(delta.channelAbsoluteDelta[0]) / delta.pixels : 0.0;
    const double meanAbsG = delta.pixels
        ? static_cast<double>(delta.channelAbsoluteDelta[1]) / delta.pixels : 0.0;
    const double meanAbsB = delta.pixels
        ? static_cast<double>(delta.channelAbsoluteDelta[2]) / delta.pixels : 0.0;
    const double meanAbsA = delta.pixels
        ? static_cast<double>(delta.channelAbsoluteDelta[3]) / delta.pixels : 0.0;
    MGLOG_I("MOBILEPZ_PZF6_DRAW_DELTA capture=%u generation=%u status=%s "
            "serial=%llu family=%s program=%u present=%llu draw_fbo=%d texture=%d "
            "level=%d layer=%d viewport=%d,%d,%d,%d read_x=%d read_width=%d "
            "rows=%zu row_phase=%u row_y_first=%d row_y_last=%d row_y_hash=%016llx "
            "pixels=%llu unchanged=%llu changed=%llu changed_percent=%g "
            "rgb_changed=%llu alpha_changed=%llu rgb_gain=%llu rgb_loss=%llu "
            "alpha_gain=%llu alpha_loss=%llu nontransparent_gain=%llu "
            "nontransparent_loss=%llu before_zero_rgba=%llu after_zero_rgba=%llu "
            "before_rgb_nonzero=%llu after_rgb_nonzero=%llu before_alpha_nonzero=%llu "
            "after_alpha_nonzero=%llu before_nontransparent=%llu after_nontransparent=%llu "
            "before_hash=%016llx after_hash=%016llx "
            "channel_signed_delta=%lld,%lld,%lld,%lld channel_abs_delta=%llu,%llu,%llu,%llu "
            "channel_mean_abs_delta=%g,%g,%g,%g sentinel_rows_before=%u "
            "sentinel_rows_after=%u fbo_status_before=0x%x fbo_status_after=0x%x "
            "blocking_readback=1 same_regions=1 target_verified=1 state_restored=1 "
            "rendering_output_mutation=0 error_drain=0",
            token.capture, g_backendContextGeneration,
            delta.changed != 0 ? "captured_changed" : "captured_no_change",
            static_cast<unsigned long long>(token.serial), PZF3FamilyName(token.family),
            token.program, static_cast<unsigned long long>(g_pzf3PresentSerial),
            token.fbo, token.texture, token.level, token.layer,
            token.viewport[0], token.viewport[1], token.viewport[2], token.viewport[3],
            token.readX, token.readWidth, token.rowY.size(), token.rowPhase,
            token.rowY.empty() ? 0 : token.rowY.front(),
            token.rowY.empty() ? 0 : token.rowY.back(),
            static_cast<unsigned long long>(rowHash),
            static_cast<unsigned long long>(delta.pixels),
            static_cast<unsigned long long>(delta.unchanged),
            static_cast<unsigned long long>(delta.changed), changedPercent,
            static_cast<unsigned long long>(delta.rgbChanged),
            static_cast<unsigned long long>(delta.alphaChanged),
            static_cast<unsigned long long>(delta.rgbGain),
            static_cast<unsigned long long>(delta.rgbLoss),
            static_cast<unsigned long long>(delta.alphaGain),
            static_cast<unsigned long long>(delta.alphaLoss),
            static_cast<unsigned long long>(delta.nonTransparentGain),
            static_cast<unsigned long long>(delta.nonTransparentLoss),
            static_cast<unsigned long long>(beforeStats.zeroRgba),
            static_cast<unsigned long long>(afterStats.zeroRgba),
            static_cast<unsigned long long>(beforeStats.rgbNonzero),
            static_cast<unsigned long long>(afterStats.rgbNonzero),
            static_cast<unsigned long long>(beforeStats.alphaMid + beforeStats.alphaFull),
            static_cast<unsigned long long>(afterStats.alphaMid + afterStats.alphaFull),
            static_cast<unsigned long long>(beforeStats.nonTransparentColour),
            static_cast<unsigned long long>(afterStats.nonTransparentColour),
            static_cast<unsigned long long>(beforeStats.hash),
            static_cast<unsigned long long>(afterStats.hash),
            static_cast<long long>(delta.channelSignedDelta[0]),
            static_cast<long long>(delta.channelSignedDelta[1]),
            static_cast<long long>(delta.channelSignedDelta[2]),
            static_cast<long long>(delta.channelSignedDelta[3]),
            static_cast<unsigned long long>(delta.channelAbsoluteDelta[0]),
            static_cast<unsigned long long>(delta.channelAbsoluteDelta[1]),
            static_cast<unsigned long long>(delta.channelAbsoluteDelta[2]),
            static_cast<unsigned long long>(delta.channelAbsoluteDelta[3]),
            meanAbsR, meanAbsG, meanAbsB, meanAbsA, token.beforeSentinelRows,
            afterSentinelRows, token.beforeFboStatus, afterFboStatus);
}

static void PZF6LogSummary(const char* reason) {
    MGLOG_I("MOBILEPZ_PZF6_SUMMARY reason=%s presents=%llu context_generation=%u "
            "context_pairs=%u context_family_pairs=%u,%u eligible_draws=%llu "
            "begin_attempts=%llu completed_pairs=%llu changed_pairs=%llu "
            "no_change_pairs=%llu unsupported=%llu incomplete=%llu empty=%llu "
            "topology_changed=%llu family_completed_skinned=%llu "
            "family_completed_static=%llu pixels=%llu changed_pixels=%llu "
            "rgb_changed_pixels=%llu alpha_changed_pixels=%llu "
            "nontransparent_gain=%llu nontransparent_loss=%llu "
            "max_pairs_per_context=%u rows_per_pair=%d blocking_readback=1 "
            "same_regions=1 state_restored=1 rendering_output_mutation=0 error_drain=0",
            reason ? reason : "unknown", static_cast<unsigned long long>(g_pzf3PresentSerial),
            g_backendContextGeneration, g_pzf6ContextPairs,
            g_pzf6ContextFamilyPairs[0], g_pzf6ContextFamilyPairs[1],
            static_cast<unsigned long long>(g_pzf6EligibleDraws),
            static_cast<unsigned long long>(g_pzf6BeginAttempts),
            static_cast<unsigned long long>(g_pzf6CompletedPairs),
            static_cast<unsigned long long>(g_pzf6ChangedPairs),
            static_cast<unsigned long long>(g_pzf6NoChangePairs),
            static_cast<unsigned long long>(g_pzf6ReadbackUnsupported),
            static_cast<unsigned long long>(g_pzf6ReadbackIncomplete),
            static_cast<unsigned long long>(g_pzf6ReadbackEmpty),
            static_cast<unsigned long long>(g_pzf6TopologyChanged),
            static_cast<unsigned long long>(g_pzf6FamilyCompleted[0]),
            static_cast<unsigned long long>(g_pzf6FamilyCompleted[1]),
            static_cast<unsigned long long>(g_pzf6Pixels),
            static_cast<unsigned long long>(g_pzf6ChangedPixels),
            static_cast<unsigned long long>(g_pzf6RgbChangedPixels),
            static_cast<unsigned long long>(g_pzf6AlphaChangedPixels),
            static_cast<unsigned long long>(g_pzf6NonTransparentGain),
            static_cast<unsigned long long>(g_pzf6NonTransparentLoss),
            kPZF6MaxPairsPerContext, kPZF6ReadRows);
}

static void PZF6OnContextReady(Uint contextGeneration) {
    g_pzf6ContextPairs = 0;
    g_pzf6ContextFamilyPairs = {};
    g_pzf6LastPairPresent = std::numeric_limits<Uint64>::max();
    MGLOG_I("MOBILEPZ_PZF6_CONTEXT_READY generation=%u pairs_reset=1 "
            "rendering_output_mutation=0", contextGeneration);
}

static void PZF6OnContextDestroyed() {
    g_pzf6ContextPairs = 0;
    g_pzf6ContextFamilyPairs = {};
    g_pzf6LastPairPresent = std::numeric_limits<Uint64>::max();
    MGLOG_I("MOBILEPZ_PZF6_CONTEXT_NOT_READY generation=%u rendering_output_mutation=0",
            g_backendContextGeneration);
}

#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
static Int PZF4AlphaChannel(
    const SharedPtr<MG_State::GLState::ITextureObject>& texture,
    SizeT components, Bool& constant, Uint8& constantValue);
#include "PZF8OpaqueModelBatch.inc"
#endif
#endif

static void PZF5BeforeDraw(
    Uint64 serial, const PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend) {
#ifdef MOBILEPZ_PZF6_DRAW_DELTA
    (void)serial;
    (void)record;
    (void)program;
    (void)backend;
    return;
#endif
    if (!g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration) ||
        !g_pzf5Lineage.valid || g_pzf5Lineage.contextGeneration != g_backendContextGeneration ||
        g_pzf5ContextCaptures >= kPZF5MaxCapturesPerContext ||
        g_pzf5Lineage.skinnedWrites + g_pzf5Lineage.staticWrites == 0 ||
        g_pzf5LastCapturePresent == g_pzf3PresentSerial) return;
    GLint currentDrawFbo = 0;
    if (!g_GLESFuncs.glGetIntegerv) return;
    g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFbo);
    if (currentDrawFbo == g_pzf5Lineage.fbo) return;
    ++g_pzf5ConsumerChecks;
    Uint samplerOrdinal = 0;
    Uint frontendTexture = 0;
    Uint64 textureLifetime = 0;
    if (!PZF5FindSamplerLineage(program, backend, g_pzf5Lineage.texture,
                                samplerOrdinal, frontendTexture, textureLifetime)) return;
    ++g_pzf5LineageMatches;
    PZF5ReadProducerTarget(serial, record, samplerOrdinal, frontendTexture, textureLifetime);
    ++g_pzf5ContextCaptures;
    g_pzf5LastCapturePresent = g_pzf3PresentSerial;
    g_pzf5Lineage.skinnedWrites = 0;
    g_pzf5Lineage.staticWrites = 0;
}

static void PZF5AfterDraw(Uint64 serial, PZF3Family family, Uint program) {
#ifdef MOBILEPZ_PZF6_DRAW_DELTA
    (void)serial;
    (void)family;
    (void)program;
    return;
#endif
    if (!g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration) ||
        g_pzf5ContextCaptures >= kPZF5MaxCapturesPerContext ||
        (family != PZF3Family::Skinned && family != PZF3Family::StaticModel) ||
        !g_GLESFuncs.glGetIntegerv || !g_GLESFuncs.glGetFramebufferAttachmentParameteriv) return;
    GLint drawFbo = 0;
    GLint outputType = GL_NONE;
    GLint outputName = 0;
    GLint outputLevel = -1;
    GLint outputLayer = -1;
    Array<GLint, 4> viewport{};
    g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    if (drawFbo == 0) return;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &outputType);
    if (outputType != GL_TEXTURE) return;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &outputName);
    if (outputName == 0) return;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &outputLevel);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &outputLayer);
    g_GLESFuncs.glGetIntegerv(GL_VIEWPORT, viewport.data());
    if (!g_pzf5Lineage.valid || g_pzf5Lineage.contextGeneration != g_backendContextGeneration ||
        g_pzf5Lineage.fbo != drawFbo || g_pzf5Lineage.texture != outputName) {
        g_pzf5Lineage = {};
        g_pzf5Lineage.valid = true;
        g_pzf5Lineage.contextGeneration = g_backendContextGeneration;
        g_pzf5Lineage.fbo = drawFbo;
        g_pzf5Lineage.texture = outputName;
    }
    g_pzf5Lineage.level = outputLevel;
    g_pzf5Lineage.layer = outputLayer;
    g_pzf5Lineage.viewport = viewport;
    g_pzf5Lineage.lastProducerSerial = serial;
    g_pzf5Lineage.lastProducerFamily = family;
    g_pzf5Lineage.lastProducerProgram = program;
    if (family == PZF3Family::Skinned) ++g_pzf5Lineage.skinnedWrites;
    if (family == PZF3Family::StaticModel) ++g_pzf5Lineage.staticWrites;
}

static void PZF5LogSummary(const char* reason) {
    MGLOG_I("MOBILEPZ_PZF5_SUMMARY reason=%s presents=%llu context_generation=%u "
            "context_captures=%u consumer_checks=%llu lineage_matches=%llu "
            "readback_attempts=%llu readback_captures=%llu readback_unsupported=%llu "
            "readback_incomplete=%llu readback_empty=%llu pixels=%llu zero_rgba=%llu "
            "rgb_nonzero=%llu alpha_nonzero=%llu nontransparent_colour=%llu "
            "max_captures_per_context=%u blocking_readback=1 state_restored=1 "
            "rendering_output_mutation=0 error_drain=0",
            reason ? reason : "unknown", static_cast<unsigned long long>(g_pzf3PresentSerial),
            g_backendContextGeneration, g_pzf5ContextCaptures,
            static_cast<unsigned long long>(g_pzf5ConsumerChecks),
            static_cast<unsigned long long>(g_pzf5LineageMatches),
            static_cast<unsigned long long>(g_pzf5ReadbackAttempts),
            static_cast<unsigned long long>(g_pzf5ReadbackCaptures),
            static_cast<unsigned long long>(g_pzf5ReadbackUnsupported),
            static_cast<unsigned long long>(g_pzf5ReadbackIncomplete),
            static_cast<unsigned long long>(g_pzf5ReadbackEmpty),
            static_cast<unsigned long long>(g_pzf5CapturedPixels),
            static_cast<unsigned long long>(g_pzf5CapturedZeroRgba),
            static_cast<unsigned long long>(g_pzf5CapturedRgbNonzero),
            static_cast<unsigned long long>(g_pzf5CapturedAlphaNonzero),
            static_cast<unsigned long long>(g_pzf5CapturedNonTransparentColour),
            kPZF5MaxCapturesPerContext);
}

static void PZF5OnContextReady(Uint contextGeneration) {
    g_pzf5Lineage = {};
    g_pzf5ContextCaptures = 0;
    g_pzf5LastCapturePresent = std::numeric_limits<Uint64>::max();
    MGLOG_I("MOBILEPZ_PZF5_CONTEXT_READY generation=%u captures_reset=1 "
            "rendering_output_mutation=0", contextGeneration);
#ifdef MOBILEPZ_PZF6_DRAW_DELTA
    PZF6OnContextReady(contextGeneration);
#endif
}

static void PZF5OnContextDestroyed() {
    g_pzf5Lineage = {};
    g_pzf5ContextCaptures = 0;
    g_pzf5LastCapturePresent = std::numeric_limits<Uint64>::max();
    MGLOG_I("MOBILEPZ_PZF5_CONTEXT_NOT_READY generation=%u rendering_output_mutation=0",
            g_backendContextGeneration);
#ifdef MOBILEPZ_PZF6_DRAW_DELTA
    PZF6OnContextDestroyed();
#endif
}
#endif

static Uint64 PZF4ComputeSignature(
    const PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend,
    GLenum mode, GLsizei count, GLenum indexType, const void* indices,
    GLsizei instances, GLint baseVertex, GLuint baseInstance) {
    Uint64 hash = 0x504a463400000001ull;
    hash = PZF4Mix(hash, record.lifetime);
    hash = PZF4Mix(hash, record.link);
    hash = PZF4Mix(hash, static_cast<Uint64>(mode));
    hash = PZF4Mix(hash, static_cast<Uint64>(static_cast<Uint32>(count)));
    hash = PZF4Mix(hash, static_cast<Uint64>(indexType));
    hash = PZF4Mix(hash, reinterpret_cast<SizeT>(indices));
    hash = PZF4Mix(hash, static_cast<Uint64>(static_cast<Uint32>(instances)));
    hash = PZF4Mix(hash, static_cast<Uint64>(static_cast<Int64>(baseVertex)));
    hash = PZF4Mix(hash, baseInstance);
    if (program) {
        hash = PZF4Mix(hash, program->GetUBOContentVersion());
    }
    const auto& vao = MG_State::pGLContext->GetBoundVertexArray();
    if (vao) {
        hash = PZF4Mix(hash, vao->GetLifetimeId());
        const auto& ebo = vao->GetIndexBufferBindingSlot().GetBoundObject();
        if (ebo) {
            hash = PZF4Mix(hash, ebo->GetLifetimeId());
            hash = PZF4Mix(hash, ebo->GetChangeSerial());
            hash = PZF4Mix(hash, ebo->GetSize());
        }
        const auto& attributes = vao->GetAllAttributes();
        for (SizeT i = 0; i < attributes.size(); ++i) {
            const auto& attribute = attributes[i];
            if (!attribute.Enabled) continue;
            hash = PZF4Mix(hash, i);
            hash = PZF4Mix(hash, static_cast<Uint64>(attribute.Type));
            hash = PZF4Mix(hash, static_cast<Uint64>(static_cast<Uint32>(attribute.Size)));
            hash = PZF4Mix(hash, static_cast<Uint64>(static_cast<Uint32>(attribute.Stride)));
            hash = PZF4Mix(hash, attribute.Offset);
            hash = PZF4Mix(hash, attribute.Divisor);
            if (attribute.Buffer) {
                hash = PZF4Mix(hash, attribute.Buffer->GetLifetimeId());
                hash = PZF4Mix(hash, attribute.Buffer->GetChangeSerial());
            }
        }
    }
    if (program && backend) {
        SizeT ordinal = 0;
        for (const auto& binding : backend->GetSamplerUniformBindings()) {
            if (ordinal++ >= 8) break;
            const auto texture = PZF4ResolveSamplerTexture(program, binding);
            hash = PZF4Mix(hash, binding.frontendLocation);
            if (texture) {
                hash = PZF4Mix(hash, texture->GetLifetimeId());
                hash = PZF4Mix(hash, texture->GetContentVersion());
            }
        }
    }
    return hash ? hash : 1;
}

static SizeT PZF4IndexTypeSize(GLenum type) {
    switch (type) {
    case GL_UNSIGNED_BYTE: return sizeof(Uint8);
    case GL_UNSIGNED_SHORT: return sizeof(Uint16);
    case GL_UNSIGNED_INT: return sizeof(Uint32);
    default: return 0;
    }
}

static Uint64 PZF4ReadIndex(const Uint8* data, GLenum type, SizeT ordinal) {
    if (type == GL_UNSIGNED_BYTE) return data[ordinal];
    if (type == GL_UNSIGNED_SHORT) {
        Uint16 value = 0;
        std::memcpy(&value, data + ordinal * sizeof(value), sizeof(value));
        return value;
    }
    Uint32 value = 0;
    std::memcpy(&value, data + ordinal * sizeof(value), sizeof(value));
    return value;
}

static Uint64 PZF4RestartValue(GLenum type) {
    switch (type) {
    case GL_UNSIGNED_BYTE: return 0xffu;
    case GL_UNSIGNED_SHORT: return 0xffffu;
    case GL_UNSIGNED_INT: return 0xffffffffu;
    default: return ~Uint64{0};
    }
}

static PZF4IndexStats PZF4BuildVertexSamples(
    GLenum indexType, const void* indices, GLsizei count, GLint baseVertex,
    const SharedPtr<MG_State::GLState::VertexArrayObject>& vao) {
    PZF4IndexStats result;
    result.indexed = indexType != 0;
    result.vertices.reserve(std::min<SizeT>(count > 0 ? static_cast<SizeT>(count) : 0,
                                            kPZF4MaxVertexSamples));
    if (count <= 0) return result;
    if (!result.indexed) {
        const Int64 first = static_cast<Int64>(reinterpret_cast<SizeT>(indices));
        result.readable = true;
        result.defined = true;
        result.minVertex = first;
        result.maxVertex = first + static_cast<Int64>(count) - 1;
        const SizeT wanted = std::min<SizeT>(static_cast<SizeT>(count), kPZF4MaxVertexSamples);
        const SizeT step = std::max<SizeT>(1, static_cast<SizeT>(count) / wanted);
        for (SizeT i = 0; i < static_cast<SizeT>(count) && result.vertices.size() < wanted; i += step) {
            result.vertices.push_back(first + static_cast<Int64>(i));
        }
        result.scanned = result.vertices.size();
        result.hash = PZF4Mix(PZF4Mix(0x4152524159530001ull, static_cast<Uint64>(first)), count);
        return result;
    }
    const SizeT elementSize = PZF4IndexTypeSize(indexType);
    const auto& ebo = vao ? vao->GetIndexBufferBindingSlot().GetBoundObject()
                          : SharedPtr<MG_State::GLState::BufferObject>{};
    if (!ebo || elementSize == 0 || !ebo->MappedData()) return result;
    result.byteOffset = reinterpret_cast<SizeT>(indices);
    result.defined = ebo->HasDefinedContent();
    const SizeT requestedCount = static_cast<SizeT>(count);
    if (result.byteOffset > ebo->GetSize() ||
        requestedCount > (ebo->GetSize() - result.byteOffset) / elementSize) return result;
    result.byteCount = requestedCount * elementSize;
    const Uint8* data = ebo->MappedData() + result.byteOffset;
    result.hash = PZF4HashSampledBytes(data, result.byteCount, kPZF4MaxTextureByteSamples);
    result.readable = true;
    const SizeT scans = std::min(requestedCount, kPZF4MaxIndexScans);
    const SizeT scanStep = std::max<SizeT>(1, requestedCount / scans);
    const SizeT vertexStep = std::max<SizeT>(1, scans / kPZF4MaxVertexSamples);
    const Uint64 restart = PZF4RestartValue(indexType);
    Bool haveVertex = false;
    SizeT scannedOrdinal = 0;
    for (SizeT i = 0; i < requestedCount && scannedOrdinal < scans; i += scanStep, ++scannedOrdinal) {
        const Uint64 raw = PZF4ReadIndex(data, indexType, i);
        if (raw == restart) {
            ++result.restartCount;
            continue;
        }
        const Int64 vertex = static_cast<Int64>(raw) + static_cast<Int64>(baseVertex);
        if (!haveVertex) {
            result.minVertex = result.maxVertex = vertex;
            haveVertex = true;
        } else {
            result.minVertex = std::min(result.minVertex, vertex);
            result.maxVertex = std::max(result.maxVertex, vertex);
        }
        if ((scannedOrdinal % vertexStep) == 0 && result.vertices.size() < kPZF4MaxVertexSamples) {
            result.vertices.push_back(vertex);
        }
    }
    result.scanned = scannedOrdinal;
    return result;
}

static SizeT PZF4ComponentSize(GLenum type) {
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE: return 1;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT: return 2;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT: return 4;
    default: return 0;
    }
}

static Bool PZF4DecodeComponent(const Uint8* data, GLenum type, Bool normalized, Float& out) {
    switch (type) {
    case GL_FLOAT:
        std::memcpy(&out, data, sizeof(out));
        return true;
    case GL_HALF_FLOAT: {
        Uint16 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = MG_Util::DecodeHalfBitsToFloat(value);
        return true;
    }
    case GL_BYTE: {
        Int8 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = normalized ? std::max(-1.0f, static_cast<Float>(value) / 127.0f)
                         : static_cast<Float>(value);
        return true;
    }
    case GL_UNSIGNED_BYTE: {
        Uint8 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = normalized ? static_cast<Float>(value) / 255.0f : static_cast<Float>(value);
        return true;
    }
    case GL_SHORT: {
        Int16 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = normalized ? std::max(-1.0f, static_cast<Float>(value) / 32767.0f)
                         : static_cast<Float>(value);
        return true;
    }
    case GL_UNSIGNED_SHORT: {
        Uint16 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = normalized ? static_cast<Float>(value) / 65535.0f : static_cast<Float>(value);
        return true;
    }
    case GL_INT: {
        Int32 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = normalized ? std::max(-1.0f, static_cast<Float>(value) / 2147483647.0f)
                         : static_cast<Float>(value);
        return true;
    }
    case GL_UNSIGNED_INT: {
        Uint32 value = 0;
        std::memcpy(&value, data, sizeof(value));
        out = normalized ? static_cast<Float>(value) / 4294967295.0f : static_cast<Float>(value);
        return true;
    }
    default:
        return false;
    }
}

static PZF4AttributeStats PZF4ScanAttribute(
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    const SharedPtr<MG_State::GLState::VertexArrayObject>& vao,
    const Vector<Int64>& vertices, Bool wantUv) {
    PZF4AttributeStats result;
    if (!program || !vao || vertices.empty()) return result;
    const auto& attributes = vao->GetAllAttributes();
    const Uint32 activeMask = program->GetActiveAttributeLocationMask();
    for (SizeT location = 0; location < attributes.size() && location < 32; ++location) {
        if ((activeMask & (Uint32{1} << location)) == 0) continue;
        const String& name = program->GetAttribName(static_cast<Uint>(location));
        if ((wantUv && !PZF4IsUvName(name)) || (!wantUv && !PZF4IsPositionName(name))) continue;
        const auto& attribute = attributes[location];
        if (!attribute.Enabled || attribute.Divisor != 0) continue;
        result.found = true;
        result.location = static_cast<Uint>(location);
        result.name = name;
        result.type = static_cast<GLenum>(attribute.Type);
        result.components = std::clamp(attribute.Size, 1, 4);
        result.offset = attribute.Offset;
        const SizeT componentSize = PZF4ComponentSize(result.type);
        const SizeT elementSize = componentSize * static_cast<SizeT>(result.components);
        result.stride = attribute.Stride != 0 ? attribute.Stride : static_cast<Int>(elementSize);
        if (!attribute.Buffer || !attribute.Buffer->MappedData() || componentSize == 0 ||
            result.stride <= 0 || elementSize == 0) return result;
        const Uint8* bytes = attribute.Buffer->MappedData();
        const SizeT bufferSize = attribute.Buffer->GetSize();
        for (Int component = 0; component < result.components; ++component) {
            result.lo[component] = std::numeric_limits<Float>::infinity();
            result.hi[component] = -std::numeric_limits<Float>::infinity();
        }
        Uint64 hash = 1469598103934665603ull;
        for (const Int64 vertex : vertices) {
            if (vertex < 0) {
                ++result.outOfBounds;
                continue;
            }
            const SizeT unsignedVertex = static_cast<SizeT>(vertex);
            if (unsignedVertex > (std::numeric_limits<SizeT>::max() - result.offset) /
                                     static_cast<SizeT>(result.stride)) {
                ++result.outOfBounds;
                continue;
            }
            const SizeT at = result.offset + unsignedVertex * static_cast<SizeT>(result.stride);
            if (at > bufferSize || elementSize > bufferSize - at) {
                ++result.outOfBounds;
                continue;
            }
            Bool sampleFinite = true;
            for (Int component = 0; component < result.components; ++component) {
                Float value = 0.0f;
                if (!PZF4DecodeComponent(bytes + at + static_cast<SizeT>(component) * componentSize,
                                         result.type, attribute.Normalized, value)) return result;
                if (!std::isfinite(value)) {
                    sampleFinite = false;
                    ++result.nonFinite;
                } else {
                    result.lo[component] = std::min(result.lo[component], value);
                    result.hi[component] = std::max(result.hi[component], value);
                }
            }
            hash = PZF4Mix(hash, PZF3HashBytes(bytes + at, elementSize));
            if (sampleFinite) ++result.samples;
        }
        result.hash = PZF4Mix(hash, result.samples);
        result.finite = result.nonFinite == 0;
        result.readable = result.samples > 0;
        return result;
    }
    return result;
}

static Int PZF4AlphaChannel(const SharedPtr<MG_State::GLState::ITextureObject>& texture,
                            SizeT components, Bool& constant, Uint8& constantValue) {
    constant = false;
    constantValue = 255;
    if (!texture) return -1;
    const auto& swizzle = texture->GetAllSwizzleParams();
    switch (swizzle[3]) {
    case TextureSwizzleParam::Zero:
        constant = true;
        constantValue = 0;
        return -1;
    case TextureSwizzleParam::One:
        constant = true;
        constantValue = 255;
        return -1;
    case TextureSwizzleParam::Red: return components >= 1 ? 0 : -1;
    case TextureSwizzleParam::Green: return components >= 2 ? 1 : -1;
    case TextureSwizzleParam::Blue: return components >= 3 ? 2 : -1;
    case TextureSwizzleParam::Alpha:
        if (components >= 4) return 3;
        constant = true;
        constantValue = 255;
        return -1;
    default: return -1;
    }
}

static Uint64 PZF4LogTextureShadow(
    Uint64 serial, PZF3Family family,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend,
    const PZF4AttributeStats& uv) {
    if (!program || !backend) return 0;
    Uint64 combined = 0x5445585455524504ull;
    Uint ordinal = 0;
    for (const auto& binding : backend->GetSamplerUniformBindings()) {
        if (ordinal >= 4) break;
        const auto texture = PZF4ResolveSamplerTexture(program, binding);
        const String& uniformName = program->IsValidUniformLocation(static_cast<Int>(binding.frontendLocation))
            ? program->GetUniformName(binding.frontendLocation) : String{};
        Uint nativeTexture = 0;
        if (texture) {
            auto* slot = TextureImpl::g_backendTextureObjects.Find(texture.get());
            if (slot && *slot) nativeTexture = (*slot)->GetBackendTextureId();
            combined = PZF4Mix(combined, texture->GetLifetimeId());
            combined = PZF4Mix(combined, texture->GetContentVersion());
        }

        Bool shadowReadable = false;
        Bool shadowDirty = false;
        Bool alphaSupported = false;
        Uint64 shadowHash = 0;
        Uint alphaSamples = 0, alphaZero = 0, alphaFull = 0, alphaMid = 0;
        Uint alphaMin = 255, alphaMax = 0;
        Uint64 alphaSum = 0;
        IntVec3 mipSize{};
        SizeT byteSize = 0;
        SizeT bytesPerPixel = 0;
        Uint level = 0;
        if (texture) {
            if (auto* mipmap = MG_State::GLState::AsMipmapTexture(texture.get())) {
                const auto& targets = texture->GetUploadTargets();
                level = texture->GetLevelRange().x();
                if (!targets.empty() && level < mipmap->GetMipmapLevelCount()) {
                    const TextureUploadTarget uploadTarget = targets[0];
                    mipSize = mipmap->GetMipmapTexelSize(uploadTarget, level);
                    byteSize = mipmap->GetMipmapByteSize(uploadTarget, level);
                    shadowDirty = mipmap->IsStorageDirty(uploadTarget, level);
                    const auto* bytes = static_cast<const Uint8*>(mipmap->MapMipmapData(uploadTarget, level));
                    shadowReadable = bytes && byteSize > 0;
                    if (shadowReadable) {
                        shadowHash = PZF4HashSampledBytes(bytes, byteSize, kPZF4MaxTextureByteSamples);
                        bytesPerPixel = MG_Util::GetSizedInternalFormatSizeInBytes(texture->GetFormat());
                        const SizeT components = MG_Util::GetBaseInternalFormatComponentCount(texture->GetFormat());
                        Bool constantAlpha = false;
                        Uint8 constantValue = 255;
                        const Int alphaChannel = PZF4AlphaChannel(texture, components,
                                                                   constantAlpha, constantValue);
                        const Bool byteComponents = components > 0 && components <= 4 &&
                                                    bytesPerPixel == components;
                        alphaSupported = constantAlpha || (byteComponents && alphaChannel >= 0);
                        const SizeT width = mipSize.x() > 0 ? static_cast<SizeT>(mipSize.x()) : 0;
                        const SizeT height = mipSize.y() > 0 ? static_cast<SizeT>(mipSize.y()) : 0;
                        const SizeT depth = mipSize.z() > 0 ? static_cast<SizeT>(mipSize.z()) : 1;
                        if (alphaSupported && width && height && bytesPerPixel &&
                            width <= std::numeric_limits<SizeT>::max() / height &&
                            width * height <= std::numeric_limits<SizeT>::max() / depth &&
                            width * height * depth <= byteSize / bytesPerPixel) {
                            Float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
                            if (uv.readable && uv.components >= 2) {
                                u0 = std::clamp(uv.lo[0], 0.0f, 1.0f);
                                u1 = std::clamp(uv.hi[0], 0.0f, 1.0f);
                                v0 = std::clamp(uv.lo[1], 0.0f, 1.0f);
                                v1 = std::clamp(uv.hi[1], 0.0f, 1.0f);
                                if (u1 < u0) std::swap(u0, u1);
                                if (v1 < v0) std::swap(v0, v1);
                            }
                            const SizeT x0 = std::min(width - 1, static_cast<SizeT>(u0 * (width - 1)));
                            const SizeT x1 = std::min(width - 1, static_cast<SizeT>(u1 * (width - 1)));
                            const SizeT y0 = std::min(height - 1, static_cast<SizeT>(v0 * (height - 1)));
                            const SizeT y1 = std::min(height - 1, static_cast<SizeT>(v1 * (height - 1)));
                            const SizeT regionWidth = x1 - x0 + 1;
                            const SizeT regionHeight = y1 - y0 + 1;
                            const SizeT sx = std::min<SizeT>(regionWidth, 64);
                            const SizeT sy = std::min<SizeT>(regionHeight, 64);
                            const SizeT sz = std::max<SizeT>(1, std::min<SizeT>(depth,
                                kPZF4MaxAlphaSamples / std::max<SizeT>(1, sx * sy)));
                            for (SizeT zi = 0; zi < sz; ++zi) {
                                const SizeT z = sz == 1 ? 0 : zi * (depth - 1) / (sz - 1);
                                for (SizeT yi = 0; yi < sy; ++yi) {
                                    const SizeT y = sy == 1 ? y0 : y0 + yi * (regionHeight - 1) / (sy - 1);
                                    for (SizeT xi = 0; xi < sx; ++xi) {
                                        const SizeT x = sx == 1 ? x0 : x0 + xi * (regionWidth - 1) / (sx - 1);
                                        const SizeT pixel = (z * width * height + y * width + x) * bytesPerPixel;
                                        const Uint alpha = constantAlpha ? constantValue
                                            : bytes[pixel + static_cast<SizeT>(alphaChannel)];
                                        ++alphaSamples;
                                        alphaSum += alpha;
                                        alphaMin = std::min(alphaMin, alpha);
                                        alphaMax = std::max(alphaMax, alpha);
                                        if (alpha == 0) ++alphaZero;
                                        else if (alpha == 255) ++alphaFull;
                                        else ++alphaMid;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        MGLOG_I("MOBILEPZ_PZF4_TEXTURE serial=%llu family=%s program=%u ordinal=%u "
                "name=%s front_texture=%u lifetime=%llu native_texture=%u content_version=%llu "
                "format=%d level=%u size=%d,%d,%d byte_size=%zu bpp=%zu complete=%d "
                "shadow_readable=%d shadow_dirty=%d shadow_hash=%016llx alpha_supported=%d "
                "alpha_samples=%u alpha_zero=%u alpha_mid=%u alpha_full=%u alpha_min=%u "
                "alpha_max=%u alpha_mean=%g uv_region=%g,%g,%g,%g",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family),
                program->GetExternalIndex(), ordinal, uniformName.c_str(),
                texture ? texture->GetExternalIndex() : 0,
                static_cast<unsigned long long>(texture ? texture->GetLifetimeId() : 0),
                nativeTexture,
                static_cast<unsigned long long>(texture ? texture->GetContentVersion() : 0),
                static_cast<Int>(texture ? texture->GetFormat() : TextureInternalFormat::Unknown),
                level, mipSize.x(), mipSize.y(), mipSize.z(), byteSize, bytesPerPixel,
                texture && texture->IsComplete() ? 1 : 0, shadowReadable ? 1 : 0,
                shadowDirty ? 1 : 0, static_cast<unsigned long long>(shadowHash),
                alphaSupported ? 1 : 0, alphaSamples, alphaZero, alphaMid, alphaFull,
                alphaSamples ? alphaMin : 0, alphaSamples ? alphaMax : 0,
                alphaSamples ? static_cast<double>(alphaSum) / alphaSamples : 0.0,
                uv.readable ? uv.lo[0] : 0.0f, uv.readable ? uv.hi[0] : 0.0f,
                uv.readable ? uv.lo[1] : 0.0f, uv.readable ? uv.hi[1] : 0.0f);
        combined = PZF4Mix(combined, shadowHash);
        combined = PZF4Mix(combined, nativeTexture);
        ++ordinal;
    }
    return combined;
}

static PZF4DrawData PZF4LogDrawData(
    Uint64 serial, Uint64 drawId, Uint64 signature, PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend,
    GLenum mode, GLsizei count, GLenum indexType, const void* indices,
    GLsizei instances, GLint baseVertex, GLuint baseInstance) {
    PZF4DrawData result;
    result.drawId = drawId;
    const auto& vao = MG_State::pGLContext->GetBoundVertexArray();
    const PZF4IndexStats index = PZF4BuildVertexSamples(indexType, indices, count, baseVertex, vao);
    const PZF4AttributeStats position = PZF4ScanAttribute(program, vao, index.vertices, false);
    const PZF4AttributeStats uv = PZF4ScanAttribute(program, vao, index.vertices, true);
    result.geometryHash = PZF4Mix(PZF4Mix(index.hash, position.hash), uv.hash);
    result.uboHash = program ? PZF3HashBytes(program->GetUBOData(), program->GetUBOSize()) : 0;
    result.textureHash = PZF4LogTextureShadow(serial, record.family, program, backend, uv);

    if (g_GLESFuncs.glGetIntegerv) {
        g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &result.drawFbo);
    }
    if (result.drawFbo != 0 && g_GLESFuncs.glGetFramebufferAttachmentParameteriv) {
        g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
            GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &result.outputType);
        if (result.outputType != GL_NONE) {
            g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &result.outputName);
        }
        if (result.outputType == GL_TEXTURE) {
            g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &result.outputLevel);
            g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &result.outputLayer);
        }
    }

    MGLOG_I("MOBILEPZ_PZF4_DRAW_ID serial=%llu draw_id=%llu family=%s program=%u program_draw=%llu "
            "signature=%016llx geometry_hash=%016llx texture_hash=%016llx ubo_hash=%016llx "
            "mode=0x%x count=%d indexed=%d index_type=0x%x index_offset=%zu instances=%d "
            "base_vertex=%d base_instance=%u index_readable=%d index_defined=%d index_bytes=%zu "
            "index_scanned=%zu restart=%u vertex_min=%lld vertex_max=%lld "
            "position_loc=%u position_name=%s position_readable=%d position_samples=%u "
            "position_oob=%u position_finite=%d position_hash=%016llx "
            "position_min=%g,%g,%g,%g position_max=%g,%g,%g,%g "
            "uv_loc=%u uv_name=%s uv_readable=%d uv_samples=%u uv_oob=%u uv_finite=%d "
            "uv_hash=%016llx uv_min=%g,%g,%g,%g uv_max=%g,%g,%g,%g",
            static_cast<unsigned long long>(serial), static_cast<unsigned long long>(drawId),
            PZF3FamilyName(record.family), record.external,
            static_cast<unsigned long long>(record.draws),
            static_cast<unsigned long long>(signature),
            static_cast<unsigned long long>(result.geometryHash),
            static_cast<unsigned long long>(result.textureHash),
            static_cast<unsigned long long>(result.uboHash), mode, count,
            indexType != 0 ? 1 : 0, indexType, reinterpret_cast<SizeT>(indices), instances,
            baseVertex, baseInstance, index.readable ? 1 : 0, index.defined ? 1 : 0,
            index.byteCount, index.scanned, index.restartCount,
            static_cast<long long>(index.minVertex), static_cast<long long>(index.maxVertex),
            position.location, position.name.c_str(), position.readable ? 1 : 0,
            position.samples, position.outOfBounds, position.finite ? 1 : 0,
            static_cast<unsigned long long>(position.hash),
            position.readable ? position.lo[0] : 0.0f, position.readable ? position.lo[1] : 0.0f,
            position.readable ? position.lo[2] : 0.0f, position.readable ? position.lo[3] : 0.0f,
            position.readable ? position.hi[0] : 0.0f, position.readable ? position.hi[1] : 0.0f,
            position.readable ? position.hi[2] : 0.0f, position.readable ? position.hi[3] : 0.0f,
            uv.location, uv.name.c_str(), uv.readable ? 1 : 0, uv.samples, uv.outOfBounds,
            uv.finite ? 1 : 0, static_cast<unsigned long long>(uv.hash),
            uv.readable ? uv.lo[0] : 0.0f, uv.readable ? uv.lo[1] : 0.0f,
            uv.readable ? uv.lo[2] : 0.0f, uv.readable ? uv.lo[3] : 0.0f,
            uv.readable ? uv.hi[0] : 0.0f, uv.readable ? uv.hi[1] : 0.0f,
            uv.readable ? uv.hi[2] : 0.0f, uv.readable ? uv.hi[3] : 0.0f);

    MGLOG_I("MOBILEPZ_PZF4_OUTPUT serial=%llu draw_id=%llu family=%s program=%u draw_fbo=%d "
            "default_framebuffer=%d color0_type=0x%x color0_name=%d color0_level=%d "
            "color0_layer=%d lineage_key=native_texture_name rendering_output_mutation=0",
            static_cast<unsigned long long>(serial), static_cast<unsigned long long>(drawId),
            PZF3FamilyName(record.family), record.external,
            result.drawFbo, result.drawFbo == 0 ? 1 : 0, result.outputType,
            result.outputName, result.outputLevel, result.outputLayer);
    return result;
}

static void PZF4LogQueryResult(PZF4QueryRecord& query, GLuint result) {
    ++g_pzf4QueryResults;
    const SizeT familyIndex = PZF3FamilyIndex(query.family);
    ++g_pzf4FamilyResults[familyIndex];
    if (result != 0) {
        ++g_pzf4SamplesPassed;
        ++g_pzf4FamilyPassed[familyIndex];
    } else {
        ++g_pzf4ZeroSamples;
    }
    MGLOG_I("MOBILEPZ_PZF4_SAMPLE_RESULT serial=%llu draw_id=%llu family=%s program=%u "
            "signature=%016llx geometry_hash=%016llx texture_hash=%016llx "
            "any_samples_passed=%u draw_fbo=%d color0_type=0x%x color0_name=%d "
            "present_latency=%llu query_async=1 query_blocking=0 error_drain=0",
            static_cast<unsigned long long>(query.serial),
            static_cast<unsigned long long>(query.drawId), PZF3FamilyName(query.family),
            query.program, static_cast<unsigned long long>(query.signature),
            static_cast<unsigned long long>(query.geometryHash),
            static_cast<unsigned long long>(query.textureHash), result != 0 ? 1u : 0u,
            query.drawFbo, query.outputType, query.outputName,
            static_cast<unsigned long long>(g_pzf3PresentSerial - query.beginPresent));
}

static void PZF4PollQueries(SizeT maxChecks) {
    if (!g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration) ||
        g_pzf4PendingQueries == 0 || !g_GLESFuncs.glGetQueryObjectuiv) return;
    SizeT checked = 0;
    SizeT visited = 0;
    while (checked < maxChecks && visited < kPZF4QuerySlots && g_pzf4PendingQueries > 0) {
        PZF4QueryRecord& query = g_pzf4Queries[g_pzf4PollCursor];
        g_pzf4PollCursor = (g_pzf4PollCursor + 1) % kPZF4QuerySlots;
        ++visited;
        if (query.id == 0 || !query.ended || query.contextGeneration != g_backendContextGeneration) continue;
        ++checked;
        GLuint available = GL_FALSE;
        g_GLESFuncs.glGetQueryObjectuiv(query.id, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available != GL_TRUE) continue;
        GLuint result = 0;
        g_GLESFuncs.glGetQueryObjectuiv(query.id, GL_QUERY_RESULT, &result);
        PZF4LogQueryResult(query, result);
        if (g_GLESFuncs.glDeleteQueries) g_GLESFuncs.glDeleteQueries(1, &query.id);
        query = {};
        --g_pzf4PendingQueries;
    }
}

static Int PZF4StartQuery(Uint64 serial, Uint64 signature, PZF3Family family, Uint program,
                          const PZF4DrawData& data) {
    if (!g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration)) return -1;
    if (!g_GLESFuncs.glGenQueries || !g_GLESFuncs.glDeleteQueries ||
        !g_GLESFuncs.glBeginQuery || !g_GLESFuncs.glEndQuery ||
        !g_GLESFuncs.glGetQueryiv || !g_GLESFuncs.glGetQueryObjectuiv) {
        ++g_pzf4QueryUnsupported;
        MGLOG_I("MOBILEPZ_PZF4_QUERY_SKIP serial=%llu family=%s program=%u reason=unsupported",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family), program);
        return -1;
    }
    GLint active = 0;
    g_GLESFuncs.glGetQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &active);
    if (active != 0) {
        ++g_pzf4QueryConflict;
        MGLOG_I("MOBILEPZ_PZF4_QUERY_SKIP serial=%llu family=%s program=%u "
                "reason=application_query_active active_query=%d",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family), program, active);
        return -1;
    }
    SizeT slot = kPZF4QuerySlots;
    for (SizeT i = 0; i < kPZF4QuerySlots; ++i) {
        if (g_pzf4Queries[i].id == 0) {
            slot = i;
            break;
        }
    }
    if (slot == kPZF4QuerySlots) {
        ++g_pzf4QuerySlotFull;
        MGLOG_I("MOBILEPZ_PZF4_QUERY_SKIP serial=%llu family=%s program=%u reason=pending_ring_full",
                static_cast<unsigned long long>(serial), PZF3FamilyName(family), program);
        return -1;
    }
    GLuint queryId = 0;
    g_GLESFuncs.glGenQueries(1, &queryId);
    if (queryId == 0) {
        ++g_pzf4QueryUnsupported;
        return -1;
    }
    PZF4QueryRecord& query = g_pzf4Queries[slot];
    query.id = queryId;
    query.contextGeneration = g_backendContextGeneration;
    query.drawId = data.drawId;
    query.serial = serial;
    query.signature = signature;
    query.geometryHash = data.geometryHash;
    query.textureHash = data.textureHash;
    query.beginPresent = g_pzf3PresentSerial;
    query.family = family;
    query.program = program;
    query.drawFbo = data.drawFbo;
    query.outputType = data.outputType;
    query.outputName = data.outputName;
    ++g_pzf4PendingQueries;
    ++g_pzf4QueryStarted;
    g_GLESFuncs.glBeginQuery(GL_ANY_SAMPLES_PASSED, queryId);
    MGLOG_I("MOBILEPZ_PZF4_QUERY_BEGIN serial=%llu draw_id=%llu family=%s program=%u query=%u "
            "signature=%016llx target=GL_ANY_SAMPLES_PASSED application_query_active=0",
            static_cast<unsigned long long>(serial), static_cast<unsigned long long>(data.drawId),
            PZF3FamilyName(family), program, queryId,
            static_cast<unsigned long long>(signature));
    return static_cast<Int>(slot);
}

static Int PZF4BeginDraw(
    Uint64 serial, PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend,
    GLenum mode, GLsizei count, GLenum indexType, const void* indices,
    GLsizei instances, GLint baseVertex, GLuint baseInstance) {
    const auto begin = g_pzf4Lifecycle.Begin(count, instances);
    if (begin.firstInGeneration) {
        MGLOG_I("MOBILEPZ_PZF4_BEGIN_GENERATION generation=%u begin_calls=%llu "
                "count=%d instances=%d context_ready=1 bounded_marker=1",
                g_pzf4Lifecycle.ContextGeneration(),
                static_cast<unsigned long long>(g_pzf4Lifecycle.BeginCalls()), count, instances);
    }
    if (!begin.allowed) return -1;
    PZF4PollQueries(4);
    const Uint64 signature = PZF4ComputeSignature(record, program, backend, mode, count,
                                                   indexType, indices, instances,
                                                   baseVertex, baseInstance);
    const auto reservation = g_pzf4Lifecycle.Reserve(PZF3FamilyIndex(record.family), signature);
    if (!reservation.selected) return -1;
    const PZF4DrawData data = PZF4LogDrawData(serial, reservation.drawId, signature,
                                              record, program, backend,
                                              mode, count, indexType, indices, instances,
                                              baseVertex, baseInstance);
    return PZF4StartQuery(serial, signature, record.family, record.external, data);
}

static void PZF4EndDraw(Int token) {
    if (token < 0 || static_cast<SizeT>(token) >= kPZF4QuerySlots) return;
    PZF4QueryRecord& query = g_pzf4Queries[static_cast<SizeT>(token)];
    if (query.id == 0 || query.ended || !g_GLESFuncs.glEndQuery ||
        !g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration) ||
        query.contextGeneration != g_backendContextGeneration) return;
    g_GLESFuncs.glEndQuery(GL_ANY_SAMPLES_PASSED);
    query.ended = true;
    MGLOG_I("MOBILEPZ_PZF4_QUERY_END serial=%llu draw_id=%llu family=%s program=%u query=%u "
            "driver_draw_returned=1 rendering_output_mutation=0 error_drain=0",
            static_cast<unsigned long long>(query.serial),
            static_cast<unsigned long long>(query.drawId), PZF3FamilyName(query.family),
            query.program, query.id);
}

static void PZF4LogSummary(const char* reason) {
    MGLOG_I("MOBILEPZ_PZF4_SUMMARY reason=%s presents=%llu context_ready=%d "
            "context_generation=%u context_ready_events=%llu context_destroyed_events=%llu "
            "begin_calls=%llu reject_context_not_ready=%llu reject_count_zero=%llu "
            "reject_instances_zero=%llu reject_duplicate_signature=%llu "
            "reject_family_limit=%llu reject_global_limit=%llu selected=%llu signatures=%zu "
            "query_started=%llu query_pending=%u query_results=%llu samples_passed=%llu "
            "zero_samples=%llu query_unsupported=%llu query_conflict=%llu query_slot_full=%llu "
            "skinned=%llu/%llu/%llu static=%llu/%llu/%llu world=%llu/%llu/%llu "
            "textured=%llu/%llu/%llu other=%llu/%llu/%llu",
            reason ? reason : "unknown", static_cast<unsigned long long>(g_pzf3PresentSerial),
            g_pzf4Lifecycle.ContextReady() ? 1 : 0, g_pzf4Lifecycle.ContextGeneration(),
            static_cast<unsigned long long>(g_pzf4Lifecycle.ContextReadyEvents()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.ContextDestroyedEvents()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.BeginCalls()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.RejectContextNotReady()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.RejectCountZero()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.RejectInstancesZero()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.RejectDuplicateSignature()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.RejectFamilyLimit()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.RejectGlobalLimit()),
            static_cast<unsigned long long>(g_pzf4Lifecycle.Selected()),
            g_pzf4Lifecycle.SignatureCount(),
            static_cast<unsigned long long>(g_pzf4QueryStarted), g_pzf4PendingQueries,
            static_cast<unsigned long long>(g_pzf4QueryResults),
            static_cast<unsigned long long>(g_pzf4SamplesPassed),
            static_cast<unsigned long long>(g_pzf4ZeroSamples),
            static_cast<unsigned long long>(g_pzf4QueryUnsupported),
            static_cast<unsigned long long>(g_pzf4QueryConflict),
            static_cast<unsigned long long>(g_pzf4QuerySlotFull),
            static_cast<unsigned long long>(g_pzf4Lifecycle.FamilySelected(0)),
            static_cast<unsigned long long>(g_pzf4FamilyResults[0]),
            static_cast<unsigned long long>(g_pzf4FamilyPassed[0]),
            static_cast<unsigned long long>(g_pzf4Lifecycle.FamilySelected(1)),
            static_cast<unsigned long long>(g_pzf4FamilyResults[1]),
            static_cast<unsigned long long>(g_pzf4FamilyPassed[1]),
            static_cast<unsigned long long>(g_pzf4Lifecycle.FamilySelected(2)),
            static_cast<unsigned long long>(g_pzf4FamilyResults[2]),
            static_cast<unsigned long long>(g_pzf4FamilyPassed[2]),
            static_cast<unsigned long long>(g_pzf4Lifecycle.FamilySelected(3)),
            static_cast<unsigned long long>(g_pzf4FamilyResults[3]),
            static_cast<unsigned long long>(g_pzf4FamilyPassed[3]),
            static_cast<unsigned long long>(g_pzf4Lifecycle.FamilySelected(4)),
            static_cast<unsigned long long>(g_pzf4FamilyResults[4]),
            static_cast<unsigned long long>(g_pzf4FamilyPassed[4]));
#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
    PZF5LogSummary(reason);
#endif
#ifdef MOBILEPZ_PZF6_DRAW_DELTA
    PZF6LogSummary(reason);
#endif
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
    PZF8LogSummary(reason);
#endif
}

static void PZF4ClearQueries(const char* reason, Bool deleteNative) {
    for (auto& query : g_pzf4Queries) {
        if (query.id != 0) {
            MGLOG_I("MOBILEPZ_PZF4_QUERY_ABANDON serial=%llu draw_id=%llu family=%s "
                    "program=%u query=%u query_generation=%u reason=%s blocking_read=0",
                    static_cast<unsigned long long>(query.serial),
                    static_cast<unsigned long long>(query.drawId), PZF3FamilyName(query.family),
                    query.program, query.id, query.contextGeneration,
                    reason ? reason : "unknown");
            if (deleteNative && query.contextGeneration == g_backendContextGeneration &&
                g_GLESFuncs.glDeleteQueries) {
                g_GLESFuncs.glDeleteQueries(1, &query.id);
            }
        }
        query = {};
    }
    g_pzf4PendingQueries = 0;
    g_pzf4PollCursor = 0;
}

static void PZF4OnContextReady(Uint contextGeneration) {
    if (!g_pzf4Lifecycle.OnContextReady(contextGeneration)) return;
    const Uint staleQueries = g_pzf4PendingQueries;
    // A generation change means any retained query ids belong to a dead
    // context. Discard their bookkeeping without issuing GL in the new one.
    PZF4ClearQueries("context_rearmed", false);
    MGLOG_I("MOBILEPZ_PZF4_CONTEXT_READY generation=%u rearmed=1 "
            "signatures_reset=1 stale_queries_discarded=%u rendering_mutation=0",
            contextGeneration, staleQueries);
#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
    PZF5OnContextReady(contextGeneration);
#endif
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
    PZF8OnContextReady(contextGeneration);
#endif
}

static void PZF4OnPresentBoundary() {
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
    PZF8OnPresentBoundary();
#endif
    PZF4PollQueries(kPZF4QuerySlots);
    if (g_pzf3PresentSerial == 1 || g_pzf3PresentSerial == 60 ||
        g_pzf3PresentSerial == 300 || g_pzf3PresentSerial == 600 ||
        g_pzf3PresentSerial == 1800 || g_pzf3PresentSerial == 3600) {
        PZF4LogSummary("present_milestone");
    }
}

static void PZF4OnContextDestroyed() {
    const Bool canUseGl = g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration);
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
    if (canUseGl) PZF8FinalizeBatch("context_destroyed");
#endif
    if (canUseGl) PZF4PollQueries(kPZF4QuerySlots);
    PZF4LogSummary("context_destroyed");
    PZF4ClearQueries("context_destroyed", canUseGl);
#ifdef MOBILEPZ_PZF5_RENDER_TARGET_READBACK
    PZF5OnContextDestroyed();
#endif
#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
    PZF8OnContextDestroyed();
#endif
    g_pzf4Lifecycle.OnContextDestroyed();
    MGLOG_I("MOBILEPZ_PZF4_CONTEXT_NOT_READY generation=%u context_ready=0 "
            "query_pending=0 rendering_mutation=0", g_backendContextGeneration);
}
