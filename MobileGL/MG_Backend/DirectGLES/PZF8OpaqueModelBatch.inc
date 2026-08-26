// MobilePZ PZF8 - bounded alpha-useful offscreen model-batch delta.
// Included by PZF4VisibilityProbe.inc after the PZF6 readback helpers.

#ifdef MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA

constexpr Uint kPZF8MaxBatchesPerContext = 16;
constexpr Uint kPZF8MaxBatchesPerPrimaryFamily = 8;
constexpr GLint kPZF8ReadRows = 64;
constexpr Uint64 kPZF8MaxAlphaLogs = 64;
constexpr Uint64 kPZF8MaxConsumerChecks = 65536;

enum class PZF8AlphaClass : Uint8 { NoSampler = 0, Unknown, AllZero, Useful };

static const char* PZF8AlphaClassName(PZF8AlphaClass value) {
    switch (value) {
    case PZF8AlphaClass::AllZero: return "ALL_ZERO";
    case PZF8AlphaClass::Useful: return "USEFUL";
    case PZF8AlphaClass::Unknown: return "UNKNOWN";
    default: return "NO_SAMPLER";
    }
}

struct PZF8PrimaryAlpha {
    PZF8AlphaClass classification = PZF8AlphaClass::NoSampler;
    Uint frontTexture = 0;
    Uint64 textureLifetime = 0;
    Uint nativeTexture = 0;
    Uint64 contentVersion = 0;
    TextureTarget textureTarget = TextureTarget::Unknown;
    TextureUploadTarget uploadTarget = TextureUploadTarget::Unknown;
    TextureInternalFormat internalFormat = TextureInternalFormat::Unknown;
    Uint level = 0;
    Int width = 0;
    Int height = 0;
    Bool complete = false;
    Bool shadowReadable = false;
    Bool shadowDirty = false;
    Bool alphaSupported = false;
    Uint alphaSamples = 0;
    Uint alphaZero = 0;
    Uint alphaNonzero = 0;
    Uint alphaMin = 0;
    Uint alphaMax = 0;
};

struct PZF8CompletedBatch {
    Bool valid = false;
    Bool consumerMatched = false;
    Uint contextGeneration = 0;
    Uint capture = 0;
    GLint fbo = 0;
    GLint texture = 0;
    Uint64 firstSerial = 0;
    Uint64 lastSerial = 0;
    Uint64 changedPixels = 0;
};

struct PZF8Batch {
    Bool active = false;
    PZF6DrawDeltaToken readback;
    Uint primaryFamily = 0;
    Uint64 startPresent = 0;
    Uint64 firstSerial = 0;
    Uint64 lastSerial = 0;
    Uint firstProgram = 0;
    Uint lastProgram = 0;
    Uint64 draws = 0;
    Uint64 skinnedDraws = 0;
    Uint64 staticDraws = 0;
    Uint64 usefulAlphaDraws = 0;
    Uint64 allZeroAlphaDraws = 0;
    Uint64 unknownAlphaDraws = 0;
    PZF8PrimaryAlpha firstAlpha;
};

static PZF8Batch g_pzf8Batch;
static Array<PZF8CompletedBatch, kPZF8MaxBatchesPerContext> g_pzf8Completed{};
static Array<Uint, 2> g_pzf8PrimaryFamilyStarted{};
static Uint g_pzf8BatchesStarted = 0;
static Uint64 g_pzf8LastStartedPresent = std::numeric_limits<Uint64>::max();
static Uint64 g_pzf8ModelDrawsObserved = 0;
static Uint64 g_pzf8UsefulAlphaDraws = 0;
static Uint64 g_pzf8AllZeroAlphaDraws = 0;
static Uint64 g_pzf8UnknownAlphaDraws = 0;
static Uint64 g_pzf8NoSamplerDraws = 0;
static Uint64 g_pzf8AlphaLogs = 0;
static Uint64 g_pzf8RejectSamePresent = 0;
static Uint64 g_pzf8RejectFamilyLimit = 0;
static Uint64 g_pzf8RejectGlobalLimit = 0;
static Uint64 g_pzf8BeginAttempts = 0;
static Uint64 g_pzf8CompletedBatches = 0;
static Uint64 g_pzf8ChangedBatches = 0;
static Uint64 g_pzf8NoChangeBatches = 0;
static Uint64 g_pzf8ReadbackUnsupported = 0;
static Uint64 g_pzf8ReadbackIncomplete = 0;
static Uint64 g_pzf8ReadbackEmpty = 0;
static Uint64 g_pzf8Pixels = 0;
static Uint64 g_pzf8ChangedPixels = 0;
static Uint64 g_pzf8RgbChangedPixels = 0;
static Uint64 g_pzf8AlphaChangedPixels = 0;
static Uint64 g_pzf8NonTransparentGain = 0;
static Uint64 g_pzf8NonTransparentLoss = 0;
static Uint64 g_pzf8DrawsInBatches = 0;
static Uint64 g_pzf8SkinnedDrawsInBatches = 0;
static Uint64 g_pzf8StaticDrawsInBatches = 0;
static Uint64 g_pzf8UsefulDrawsInBatches = 0;
static Uint64 g_pzf8AllZeroDrawsInBatches = 0;
static Uint64 g_pzf8UnknownDrawsInBatches = 0;
static Uint64 g_pzf8ConsumerChecks = 0;
static Uint64 g_pzf8ConsumerMatches = 0;
static Uint64 g_pzf8PendingConsumers = 0;
#ifdef MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
static Uint g_pzf9ModelLogLines = 0;
constexpr Uint kPZF9MaxModelLogLinesPerContext = 256;
#endif
#ifdef MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
static Uint g_pzf10ModelLogLines = 0;
constexpr Uint kPZF10MaxModelLogLinesPerContext = 256;
#endif
#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
constexpr Uint kPZF11MaxAllZeroPerFamily = 8;
constexpr Uint kPZF11MaxUsefulControlsPerFamily = 2;
constexpr Uint kPZF11MaxCapturesPerContext = 20;
constexpr Uint64 kPZF11MaxFullReadBytes = 4ull * 1024ull * 1024ull;
constexpr GLint kPZF11SampleRows = 64;
constexpr GLint kPZF11MaxSampleWidth = 4096;

struct PZF11Summary {
    Uint generation = 0;
    Uint selected = 0;
    Uint successful = 0;
    Uint exactFullReads = 0;
    Uint sampledReads = 0;
    Uint unsupported = 0;
    Uint badDimensions = 0;
    Uint incompleteFbo = 0;
    Uint sentinelReads = 0;
    Uint cpuShadowReads = 0;
    Uint cpuShadowMissing = 0;
    Uint cpuAllZero = 0;
    Uint cpuRgbAlphaZero = 0;
    Uint cpuNonTransparent = 0;
    Uint64 pixels = 0;
    Uint64 zeroRgba = 0;
    Uint64 rgbNonzero = 0;
    Uint64 alphaNonzero = 0;
    Uint64 cpuZeroGpuZero = 0;
    Uint64 cpuZeroGpuRgbAlphaZero = 0;
    Uint64 cpuZeroGpuNonTransparent = 0;
    Uint64 cpuUsefulGpuZero = 0;
    Uint64 cpuUsefulGpuRgbAlphaZero = 0;
    Uint64 cpuUsefulGpuNonTransparent = 0;
};

static PZF11Summary g_pzf11Summary;
static Array<Uint, 2> g_pzf11AllZeroPerFamily{};
static Array<Uint, 2> g_pzf11UsefulPerFamily{};
static Array<Uint64, kPZF11MaxCapturesPerContext> g_pzf11TextureLifetimes{};
static Uint g_pzf11TextureLifetimeCount = 0;
static GLuint g_pzf11ReadFbo = 0;
#endif

static Int PZF8FamilySlot(PZF3Family family) {
    if (family == PZF3Family::Skinned) return 0;
    if (family == PZF3Family::StaticModel) return 1;
    return -1;
}

static PZF8PrimaryAlpha PZF8InspectPrimaryAlpha(
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend) {
    PZF8PrimaryAlpha result;
    if (!program || !backend) return result;
    const auto& bindings = backend->GetSamplerUniformBindings();
    if (bindings.empty()) return result;
    const auto& binding = *bindings.begin();
    const auto texture = PZF4ResolveSamplerTexture(program, binding);
    if (!texture) return result;

    result.classification = PZF8AlphaClass::Unknown;
    result.frontTexture = texture->GetExternalIndex();
    result.textureLifetime = texture->GetLifetimeId();
    result.contentVersion = texture->GetContentVersion();
    result.textureTarget = texture->GetTarget();
    result.internalFormat = texture->GetFormat();
    result.complete = texture->IsComplete();
    if (auto* slot = TextureImpl::g_backendTextureObjects.Find(texture.get()); slot && *slot) {
        result.nativeTexture = (*slot)->GetBackendTextureId();
    }

    auto* mipmap = MG_State::GLState::AsMipmapTexture(texture.get());
    const auto& targets = texture->GetUploadTargets();
    if (!mipmap || targets.empty()) return result;
    const Uint level = texture->GetLevelRange().x();
    if (level >= mipmap->GetMipmapLevelCount()) return result;
    const TextureUploadTarget uploadTarget = targets[0];
    const IntVec3 mipSize = mipmap->GetMipmapTexelSize(uploadTarget, level);
    result.uploadTarget = uploadTarget;
    result.level = level;
    result.width = mipSize.x();
    result.height = mipSize.y();
    const SizeT byteSize = mipmap->GetMipmapByteSize(uploadTarget, level);
    result.shadowDirty = mipmap->IsStorageDirty(uploadTarget, level);
    const auto* bytes = static_cast<const Uint8*>(mipmap->MapMipmapData(uploadTarget, level));
    result.shadowReadable = bytes && byteSize > 0;
    if (!result.shadowReadable || result.shadowDirty) return result;

    const SizeT bpp = MG_Util::GetSizedInternalFormatSizeInBytes(texture->GetFormat());
    const SizeT components = MG_Util::GetBaseInternalFormatComponentCount(texture->GetFormat());
    Bool constantAlpha = false;
    Uint8 constantValue = 255;
    const Int alphaChannel = PZF4AlphaChannel(texture, components, constantAlpha, constantValue);
    result.alphaSupported = constantAlpha ||
        (components > 0 && components <= 4 && bpp == components && alphaChannel >= 0);
    if (!result.alphaSupported || bpp == 0 || mipSize.x() <= 0 || mipSize.y() <= 0) return result;

    const SizeT width = static_cast<SizeT>(mipSize.x());
    const SizeT height = static_cast<SizeT>(mipSize.y());
    const SizeT depth = mipSize.z() > 0 ? static_cast<SizeT>(mipSize.z()) : 1;
    if (width > std::numeric_limits<SizeT>::max() / height ||
        width * height > std::numeric_limits<SizeT>::max() / depth) return result;
    const SizeT pixels = width * height * depth;
    if (pixels == 0 || pixels > byteSize / bpp) return result;

    const SizeT samples = std::min<SizeT>(pixels, kPZF4MaxAlphaSamples);
    for (SizeT sample = 0; sample < samples; ++sample) {
        const SizeT pixel = samples == 1 ? 0 : sample * (pixels - 1) / (samples - 1);
        const Uint alpha = constantAlpha ? constantValue
            : bytes[pixel * bpp + static_cast<SizeT>(alphaChannel)];
        if (sample == 0) result.alphaMin = result.alphaMax = alpha;
        else {
            result.alphaMin = std::min(result.alphaMin, alpha);
            result.alphaMax = std::max(result.alphaMax, alpha);
        }
        ++result.alphaSamples;
        if (alpha == 0) ++result.alphaZero;
        else ++result.alphaNonzero;
    }
    result.classification = result.alphaNonzero != 0
        ? PZF8AlphaClass::Useful : PZF8AlphaClass::AllZero;
    return result;
}

static Bool PZF8PrepareReadbackToken(PZF6DrawDeltaToken& token) {
    if (!PZF5CanReadback() || !g_GLESFuncs.glGetFramebufferAttachmentParameteriv) {
        ++g_pzf8ReadbackUnsupported;
        return false;
    }
    GLint outputType = GL_NONE;
    g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &token.fbo);
    if (token.fbo == 0) return false;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &outputType);
    if (outputType != GL_TEXTURE) return false;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &token.texture);
    if (token.texture == 0) return false;
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &token.level);
    g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &token.layer);
    g_GLESFuncs.glGetIntegerv(GL_VIEWPORT, token.viewport.data());

    token.readX = std::max<GLint>(0, token.viewport[0]);
    token.readWidth = token.viewport[2] - (token.readX - token.viewport[0]);
    const GLint readY = std::max<GLint>(0, token.viewport[1]);
    const GLint readHeight = token.viewport[3] - (readY - token.viewport[1]);
    if (token.readWidth > kPZF6MaxReadWidth) {
        token.readX += (token.readWidth - kPZF6MaxReadWidth) / 2;
        token.readWidth = kPZF6MaxReadWidth;
    }
    if (token.readWidth <= 0 || readHeight <= 0) {
        ++g_pzf8ReadbackEmpty;
        return false;
    }
    const GLint rows = std::min(kPZF8ReadRows, readHeight);
    token.rowY.reserve(static_cast<SizeT>(rows));
    for (GLint rowIndex = 0; rowIndex < rows; ++rowIndex) {
        const GLint begin = rowIndex * readHeight / rows;
        const GLint end = (rowIndex + 1) * readHeight / rows;
        const GLint height = std::max<GLint>(1, end - begin);
        const GLint offset = static_cast<GLint>(
            (token.rowPhase * 17u + static_cast<Uint>(rowIndex) * 7u) %
            static_cast<Uint>(height));
        token.rowY.push_back(readY + begin + offset);
    }
    return true;
}

static void PZF8LogAlpha(Uint64 serial, const PZF3ProgramRecord& record,
                         const PZF8PrimaryAlpha& alpha) {
    if (g_pzf8AlphaLogs >= kPZF8MaxAlphaLogs) return;
    ++g_pzf8AlphaLogs;
    MGLOG_I("MOBILEPZ_PZF8_INPUT_ALPHA serial=%llu present=%llu family=%s program=%u "
            "class=%s front_texture=%u texture_lifetime=%llu native_texture=%u "
            "content_version=%llu complete=%d shadow_readable=%d shadow_dirty=%d "
            "alpha_supported=%d alpha_samples=%u alpha_zero=%u alpha_nonzero=%u "
            "alpha_min=%u alpha_max=%u rendering_output_mutation=0 error_drain=0",
            static_cast<unsigned long long>(serial),
            static_cast<unsigned long long>(g_pzf3PresentSerial),
            PZF3FamilyName(record.family), record.external,
            PZF8AlphaClassName(alpha.classification), alpha.frontTexture,
            static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
            static_cast<unsigned long long>(alpha.contentVersion), alpha.complete ? 1 : 0,
            alpha.shadowReadable ? 1 : 0, alpha.shadowDirty ? 1 : 0,
            alpha.alphaSupported ? 1 : 0, alpha.alphaSamples, alpha.alphaZero,
            alpha.alphaNonzero, alpha.alphaMin, alpha.alphaMax);
}

#ifdef MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
static const char* PZF9ProvenanceClass(const ::MobilePZ::PZF9::Record& upload) {
    if (upload.source.valid && upload.source.nonzero != 0 &&
        upload.processed.valid && upload.processed.nonzero == 0) return "CLIENT_NONZERO_CONVERSION_ZEROED";
    if (upload.processed.valid && upload.processed.nonzero != 0 &&
        upload.driverPayload.valid && upload.driverPayload.nonzero == 0) return "SHADOW_NONZERO_DRIVER_PAYLOAD_ZEROED";
    if (upload.source.valid && upload.source.nonzero == 0) return "CLIENT_ALREADY_ALL_ZERO";
    if (upload.lastKind == ::MobilePZ::PZF9::UploadKind::NullImage2D) return "CLIENT_NULL_ALLOCATION";
    if (upload.source.valid && upload.source.nonzero != 0 &&
        upload.processed.valid && upload.processed.nonzero != 0 &&
        upload.driverPayload.valid && upload.driverPayload.nonzero != 0) return "NONZERO_PRESERVED_TO_DRIVER";
    if (!upload.source.valid && upload.backendShadow.valid && upload.backendShadow.nonzero == 0)
        return "NO_CLIENT_BYTES_SHADOW_ALL_ZERO";
    return "INCOMPLETE_PROVENANCE";
}

static void PZF9LogModelTexture(Uint64 serial, const PZF3ProgramRecord& draw,
                                const PZF8PrimaryAlpha& alpha) {
    if (alpha.textureLifetime == 0 || g_pzf9ModelLogLines >= kPZF9MaxModelLogLinesPerContext) return;
    ::MobilePZ::PZF9::Record upload;
    const Int result = ::MobilePZ::PZF9::TakeModelSnapshot(alpha.textureLifetime, upload);
    if (result == 0) return;
    ++g_pzf9ModelLogLines;
    if (result == 2) {
        MGLOG_I("MOBILEPZ_PZF9_MODEL_TEXTURE status=no_upload_record generation=%u serial=%llu "
                "family=%s program=%u front_texture=%u texture_lifetime=%llu native_texture=%u "
                "draw_alpha_class=%s rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
                g_backendContextGeneration, static_cast<unsigned long long>(serial),
                PZF3FamilyName(draw.family), draw.external, alpha.frontTexture,
                static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
                PZF8AlphaClassName(alpha.classification));
        return;
    }

    MGLOG_I("MOBILEPZ_PZF9_MODEL_TEXTURE status=matched generation=%u serial=%llu family=%s program=%u "
            "front_texture=%u texture_lifetime=%llu native_texture=%u draw_alpha_class=%s "
            "classification=%s client_sequence=%llu backend_sequence=%llu last_kind=%s "
            "level=%d offset=%dx%d size=%dx%d internal_format=0x%x input_format=0x%x input_type=0x%x pbo=%d "
            "image_calls=%llu subimage_calls=%llu null_image_calls=%llu "
            "source_valid=%d source_bytes=%llu source_zero=%llu source_nonzero=%llu source_hash=%016llx "
            "processed_valid=%d processed_bytes=%llu processed_zero=%llu processed_nonzero=%llu "
            "processed_hash=%016llx source_nonzero_processed_zero=%llu "
            "backend_seen=%d backend_format=0x%x backend_type=0x%x backend_conversion=%u "
            "backend_shadow_valid=%d backend_shadow_bytes=%llu backend_shadow_nonzero=%llu "
            "backend_shadow_hash=%016llx driver_payload_valid=%d driver_payload_bytes=%llu "
            "driver_payload_nonzero=%llu driver_payload_hash=%016llx "
            "rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
            g_backendContextGeneration, static_cast<unsigned long long>(serial),
            PZF3FamilyName(draw.family), draw.external, alpha.frontTexture,
            static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
            PZF8AlphaClassName(alpha.classification), PZF9ProvenanceClass(upload),
            static_cast<unsigned long long>(upload.clientSequence),
            static_cast<unsigned long long>(upload.backendSequence),
            ::MobilePZ::PZF9::UploadKindName(upload.lastKind), upload.level,
            upload.xoffset, upload.yoffset, upload.width, upload.height,
            upload.internalFormat, upload.inputFormat, upload.inputType, upload.pbo ? 1 : 0,
            static_cast<unsigned long long>(upload.imageCalls),
            static_cast<unsigned long long>(upload.subImageCalls),
            static_cast<unsigned long long>(upload.nullImageCalls),
            upload.source.valid ? 1 : 0, static_cast<unsigned long long>(upload.source.bytes),
            static_cast<unsigned long long>(upload.source.zero),
            static_cast<unsigned long long>(upload.source.nonzero),
            static_cast<unsigned long long>(upload.source.hash),
            upload.processed.valid ? 1 : 0, static_cast<unsigned long long>(upload.processed.bytes),
            static_cast<unsigned long long>(upload.processed.zero),
            static_cast<unsigned long long>(upload.processed.nonzero),
            static_cast<unsigned long long>(upload.processed.hash),
            static_cast<unsigned long long>(upload.sourceNonzeroProcessedZero),
            upload.backendSeen ? 1 : 0, upload.backendFormat, upload.backendType,
            upload.backendConversion, upload.backendShadow.valid ? 1 : 0,
            static_cast<unsigned long long>(upload.backendShadow.bytes),
            static_cast<unsigned long long>(upload.backendShadow.nonzero),
            static_cast<unsigned long long>(upload.backendShadow.hash),
            upload.driverPayload.valid ? 1 : 0,
            static_cast<unsigned long long>(upload.driverPayload.bytes),
            static_cast<unsigned long long>(upload.driverPayload.nonzero),
            static_cast<unsigned long long>(upload.driverPayload.hash));
}

static void PZF9LogSummary(const char* reason) {
    const auto summary = ::MobilePZ::PZF9::GetSummary();
    MGLOG_I("MOBILEPZ_PZF9_SUMMARY reason=%s generation=%u records=%llu client_calls=%llu "
            "image_calls=%llu subimage_calls=%llu null_image_calls=%llu "
            "source_nonzero_calls=%llu source_all_zero_calls=%llu processed_nonzero_calls=%llu "
            "processed_all_zero_calls=%llu source_nonzero_processed_zero=%llu backend_observations=%llu "
            "driver_payload_nonzero=%llu driver_payload_all_zero=%llu model_lookups=%llu "
            "model_matches=%llu model_missing=%llu model_logs=%llu context_log_lines=%u "
            "max_context_log_lines=%u rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
            reason ? reason : "unknown", summary.contextGeneration,
            static_cast<unsigned long long>(summary.records),
            static_cast<unsigned long long>(summary.clientCalls),
            static_cast<unsigned long long>(summary.imageCalls),
            static_cast<unsigned long long>(summary.subImageCalls),
            static_cast<unsigned long long>(summary.nullImageCalls),
            static_cast<unsigned long long>(summary.sourceNonzeroCalls),
            static_cast<unsigned long long>(summary.sourceAllZeroCalls),
            static_cast<unsigned long long>(summary.processedNonzeroCalls),
            static_cast<unsigned long long>(summary.processedAllZeroCalls),
            static_cast<unsigned long long>(summary.sourceNonzeroProcessedZero),
            static_cast<unsigned long long>(summary.backendObservations),
            static_cast<unsigned long long>(summary.driverPayloadNonzero),
            static_cast<unsigned long long>(summary.driverPayloadAllZero),
            static_cast<unsigned long long>(summary.modelLookups),
            static_cast<unsigned long long>(summary.modelMatches),
            static_cast<unsigned long long>(summary.modelMissing),
            static_cast<unsigned long long>(summary.modelLogs), g_pzf9ModelLogLines,
            kPZF9MaxModelLogLinesPerContext);
}
#endif

#ifdef MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
static const char* PZF10CopyClass(const ::MobilePZ::PZF10::Record& copy) {
    if (!copy.eligible) return "COPY_NOT_RGBA8_NATIVE_FALLBACK";
    if (copy.readError != 0) return "COPY_READ_ERROR_NATIVE_FALLBACK";
    if (copy.uploadError != 0) return "COPY_UPLOAD_ERROR_NATIVE_FALLBACK";
    if (!copy.staged || !copy.source.valid) return "COPY_UNOBSERVED_NATIVE_FALLBACK";
    if (copy.source.zeroRgba == copy.source.pixels) return "COPY_SOURCE_ALL_ZERO_STAGED";
    if (copy.source.alphaNonzero == 0 && copy.source.rgbNonzero != 0)
        return "COPY_SOURCE_RGB_ALPHA_ZERO_STAGED";
    if (copy.source.alphaNonzero != 0) return "COPY_SOURCE_NONTRANSPARENT_STAGED";
    return "COPY_SOURCE_INDETERMINATE_STAGED";
}

static void PZF10LogModelTexture(Uint64 serial, const PZF3ProgramRecord& draw,
                                 const PZF8PrimaryAlpha& alpha) {
    if (alpha.textureLifetime == 0 || g_pzf10ModelLogLines >= kPZF10MaxModelLogLinesPerContext) return;
    ::MobilePZ::PZF10::Record copy;
    const Int result = ::MobilePZ::PZF10::TakeModelSnapshot(alpha.textureLifetime, copy);
    if (result == 0) return;
    ++g_pzf10ModelLogLines;
    if (result == 2) {
        MGLOG_I("MOBILEPZ_PZF10_MODEL_TEXTURE status=no_copy_record generation=%u serial=%llu "
                "family=%s program=%u front_texture=%u texture_lifetime=%llu native_texture=%u "
                "draw_alpha_class=%s classification=NULL_ALLOCATION_WITHOUT_COPY_EVENT "
                "rendering_output_mutation=copy_repair_only additional_gl_calls=bounded error_probe=bounded",
                g_backendContextGeneration, static_cast<unsigned long long>(serial),
                PZF3FamilyName(draw.family), draw.external, alpha.frontTexture,
                static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
                PZF8AlphaClassName(alpha.classification));
        return;
    }

    MGLOG_I("MOBILEPZ_PZF10_MODEL_TEXTURE status=matched generation=%u serial=%llu family=%s program=%u "
            "front_texture=%u texture_lifetime=%llu native_texture=%u draw_alpha_class=%s "
            "classification=%s sequence=%llu kind=%s level=%d src=%d,%d dst=%d,%d size=%dx%d "
            "image_calls=%llu subimage_calls=%llu eligible=%d staged=%d shadow_updated=%d "
            "read_fbo=%u read_buffer=0x%x read_fbo_status=0x%x read_error=0x%x upload_error=0x%x "
            "source_valid=%d source_bytes=%llu source_pixels=%llu source_zero_rgba=%llu "
            "source_rgb_nonzero=%llu source_alpha_nonzero=%llu source_alpha_min=%u source_alpha_max=%u "
            "source_hash=%016llx rendering_output_mutation=copy_repair_only additional_gl_calls=bounded "
            "error_probe=bounded",
            g_backendContextGeneration, static_cast<unsigned long long>(serial),
            PZF3FamilyName(draw.family), draw.external, alpha.frontTexture,
            static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
            PZF8AlphaClassName(alpha.classification), PZF10CopyClass(copy),
            static_cast<unsigned long long>(copy.sequence),
            ::MobilePZ::PZF10::CopyKindName(copy.lastKind), copy.level,
            copy.srcX, copy.srcY, copy.dstX, copy.dstY, copy.width, copy.height,
            static_cast<unsigned long long>(copy.imageCalls),
            static_cast<unsigned long long>(copy.subImageCalls), copy.eligible ? 1 : 0,
            copy.staged ? 1 : 0, copy.shadowUpdated ? 1 : 0, copy.readFbo,
            copy.readBuffer, copy.readFboStatus, copy.readError, copy.uploadError,
            copy.source.valid ? 1 : 0, static_cast<unsigned long long>(copy.source.bytes),
            static_cast<unsigned long long>(copy.source.pixels),
            static_cast<unsigned long long>(copy.source.zeroRgba),
            static_cast<unsigned long long>(copy.source.rgbNonzero),
            static_cast<unsigned long long>(copy.source.alphaNonzero),
            copy.source.alphaMin, copy.source.alphaMax,
            static_cast<unsigned long long>(copy.source.hash));
}

static void PZF10LogSummary(const char* reason) {
    const auto summary = ::MobilePZ::PZF10::GetSummary();
    MGLOG_I("MOBILEPZ_PZF10_SUMMARY reason=%s generation=%u calls=%llu image_calls=%llu "
            "subimage_calls=%llu eligible=%llu staged=%llu native_fallback=%llu read_errors=%llu "
            "upload_errors=%llu source_nontransparent=%llu source_rgb_alpha_zero=%llu "
            "source_all_zero=%llu shadow_updated=%llu model_lookups=%llu model_matches=%llu "
            "model_missing=%llu model_logs=%llu context_log_lines=%u max_context_log_lines=%u "
            "rendering_output_mutation=copy_repair_only additional_gl_calls=bounded error_probe=bounded",
            reason ? reason : "unknown", summary.contextGeneration,
            static_cast<unsigned long long>(summary.calls),
            static_cast<unsigned long long>(summary.imageCalls),
            static_cast<unsigned long long>(summary.subImageCalls),
            static_cast<unsigned long long>(summary.eligible),
            static_cast<unsigned long long>(summary.staged),
            static_cast<unsigned long long>(summary.nativeFallback),
            static_cast<unsigned long long>(summary.readErrors),
            static_cast<unsigned long long>(summary.uploadErrors),
            static_cast<unsigned long long>(summary.sourceNontransparent),
            static_cast<unsigned long long>(summary.sourceRgbAlphaZero),
            static_cast<unsigned long long>(summary.sourceAllZero),
            static_cast<unsigned long long>(summary.shadowUpdated),
            static_cast<unsigned long long>(summary.modelLookups),
            static_cast<unsigned long long>(summary.modelMatches),
            static_cast<unsigned long long>(summary.modelMissing),
            static_cast<unsigned long long>(summary.modelLogs), g_pzf10ModelLogLines,
            kPZF10MaxModelLogLinesPerContext);
}
#endif

#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
static Bool PZF11TextureAlreadySelected(Uint64 lifetime) {
    for (Uint i = 0; i < g_pzf11TextureLifetimeCount; ++i) {
        if (g_pzf11TextureLifetimes[i] == lifetime) return true;
    }
    return false;
}

static Bool PZF11SelectTexture(PZF3Family family, const PZF8PrimaryAlpha& alpha) {
    const Int familySlot = PZF8FamilySlot(family);
    if (familySlot < 0 || alpha.textureLifetime == 0 ||
        g_pzf11TextureLifetimeCount >= kPZF11MaxCapturesPerContext ||
        PZF11TextureAlreadySelected(alpha.textureLifetime)) return false;

    const SizeT slot = static_cast<SizeT>(familySlot);
    if (alpha.classification == PZF8AlphaClass::AllZero) {
        if (g_pzf11AllZeroPerFamily[slot] >= kPZF11MaxAllZeroPerFamily) return false;
        ++g_pzf11AllZeroPerFamily[slot];
    } else if (alpha.classification == PZF8AlphaClass::Useful) {
        if (g_pzf11UsefulPerFamily[slot] >= kPZF11MaxUsefulControlsPerFamily) return false;
        ++g_pzf11UsefulPerFamily[slot];
    } else {
        return false;
    }

    g_pzf11TextureLifetimes[g_pzf11TextureLifetimeCount++] = alpha.textureLifetime;
    ++g_pzf11Summary.selected;
    return true;
}

static void PZF11ObserveComparison(::MobilePZ::PZF11::ComparisonClass comparison) {
    using Comparison = ::MobilePZ::PZF11::ComparisonClass;
    switch (comparison) {
    case Comparison::CpuZeroGpuZero: ++g_pzf11Summary.cpuZeroGpuZero; break;
    case Comparison::CpuZeroGpuRgbAlphaZero: ++g_pzf11Summary.cpuZeroGpuRgbAlphaZero; break;
    case Comparison::CpuZeroGpuNonTransparent: ++g_pzf11Summary.cpuZeroGpuNonTransparent; break;
    case Comparison::CpuUsefulGpuZero: ++g_pzf11Summary.cpuUsefulGpuZero; break;
    case Comparison::CpuUsefulGpuRgbAlphaZero: ++g_pzf11Summary.cpuUsefulGpuRgbAlphaZero; break;
    case Comparison::CpuUsefulGpuNonTransparent: ++g_pzf11Summary.cpuUsefulGpuNonTransparent; break;
    default: break;
    }
}

static Bool PZF11ReadCpuShadow(
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend, const PZF8PrimaryAlpha& alpha,
    GLint readX, GLint readWidth, GLint rows, Bool exactFullRead,
    ::MobilePZ::PZF5::ReadbackStats& stats) {
    if (!program || !backend || alpha.shadowDirty || readWidth <= 0 || rows <= 0) return false;
    const auto& bindings = backend->GetSamplerUniformBindings();
    if (bindings.empty()) return false;
    const auto texture = PZF4ResolveSamplerTexture(program, *bindings.begin());
    if (!texture || texture->GetLifetimeId() != alpha.textureLifetime ||
        texture->GetFormat() != TextureInternalFormat::RGBA8) return false;
    auto* mipmap = MG_State::GLState::AsMipmapTexture(texture.get());
    if (!mipmap || alpha.level >= mipmap->GetMipmapLevelCount()) return false;
    const SizeT byteSize = mipmap->GetMipmapByteSize(alpha.uploadTarget, alpha.level);
    const auto* bytes = static_cast<const Uint8*>(
        mipmap->MapMipmapData(alpha.uploadTarget, alpha.level));
    if (!bytes || alpha.width <= 0 || alpha.height <= 0) return false;
    const SizeT width = static_cast<SizeT>(alpha.width);
    const SizeT height = static_cast<SizeT>(alpha.height);
    if (width > std::numeric_limits<SizeT>::max() / height ||
        width * height > std::numeric_limits<SizeT>::max() / 4u ||
        byteSize < width * height * 4u) return false;

    if (exactFullRead) {
        stats.Observe(bytes, width * height);
        return true;
    }
    const SizeT rowPixels = static_cast<SizeT>(readWidth);
    for (GLint row = 0; row < rows; ++row) {
        const GLint readY = rows == 1 ? 0 : row * (alpha.height - 1) / (rows - 1);
        const SizeT offset = (static_cast<SizeT>(readY) * width +
                              static_cast<SizeT>(readX)) * 4u;
        stats.Observe(bytes + offset, rowPixels);
    }
    return true;
}

static void PZF11ReadModelTexture(
    Uint64 serial, const PZF3ProgramRecord& draw,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend, const PZF8PrimaryAlpha& alpha) {
    if (!PZF11SelectTexture(draw.family, alpha)) return;
    const Uint capture = g_pzf11Summary.selected;
    const Bool supported = alpha.nativeTexture != 0 &&
        alpha.textureTarget == TextureTarget::Texture2D &&
        alpha.uploadTarget == TextureUploadTarget::Texture2D &&
        alpha.internalFormat == TextureInternalFormat::RGBA8;

    const char* status = "unsupported";
    const char* readMode = "none";
    Bool attachmentDetached = false;
    Bool exactFullRead = false;
    GLint readX = 0;
    GLint readWidth = 0;
    GLint rows = 0;
    Uint sentinelReads = 0;
    GLenum framebufferStatus = 0;
    Bool cpuShadowRead = false;
    ::MobilePZ::PZF5::ReadbackStats cpuStats;
    ::MobilePZ::PZF5::ReadbackStats stats;

    const Bool functionsAvailable = PZF5CanReadback() && g_GLESFuncs.glGenFramebuffers &&
        g_GLESFuncs.glFramebufferTexture2D && g_GLESFuncs.glFinish;
    if (!supported || !functionsAvailable) {
        ++g_pzf11Summary.unsupported;
    } else if (alpha.width <= 0 || alpha.height <= 0) {
        status = "bad_dimensions";
        ++g_pzf11Summary.badDimensions;
    } else {
        const Uint64 fullBytes = static_cast<Uint64>(alpha.width) *
                                 static_cast<Uint64>(alpha.height) * 4ull;
        exactFullRead = fullBytes <= kPZF11MaxFullReadBytes;
        readX = exactFullRead ? 0 : std::max<GLint>(0, (alpha.width - kPZF11MaxSampleWidth) / 2);
        readWidth = exactFullRead ? alpha.width : std::min<GLint>(alpha.width, kPZF11MaxSampleWidth);
        rows = exactFullRead ? alpha.height : std::min<GLint>(alpha.height, kPZF11SampleRows);
        const SizeT pixelCount = static_cast<SizeT>(readWidth) * static_cast<SizeT>(rows);
        Vector<Uint8> rgba(pixelCount * 4u, static_cast<Uint8>(0xa5));
        cpuShadowRead = PZF11ReadCpuShadow(program, backend, alpha, readX, readWidth,
                                           rows, exactFullRead, cpuStats);
        if (cpuShadowRead) ++g_pzf11Summary.cpuShadowReads;
        else ++g_pzf11Summary.cpuShadowMissing;

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

        g_GLESFuncs.glFinish();
        if (g_pzf11ReadFbo == 0) g_GLESFuncs.glGenFramebuffers(1, &g_pzf11ReadFbo);
        if (g_pzf11ReadFbo == 0) {
            status = "fbo_allocation_failed";
            ++g_pzf11Summary.unsupported;
        } else {
            g_GLESFuncs.glBindFramebuffer(GL_READ_FRAMEBUFFER, g_pzf11ReadFbo);
            g_GLESFuncs.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_TEXTURE_2D, alpha.nativeTexture,
                                               static_cast<GLint>(alpha.level));
            g_GLESFuncs.glReadBuffer(GL_COLOR_ATTACHMENT0);
            framebufferStatus = g_GLESFuncs.glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
            if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE) {
                g_GLESFuncs.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                g_GLESFuncs.glPixelStorei(GL_PACK_ALIGNMENT, 1);
                g_GLESFuncs.glPixelStorei(GL_PACK_ROW_LENGTH, 0);
                g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_ROWS, 0);
                g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
                if (exactFullRead) {
                    readMode = "full";
                    g_GLESFuncs.glReadPixels(0, 0, alpha.width, alpha.height,
                                             GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                    if (std::all_of(rgba.begin(), rgba.end(), [](Uint8 value) {
                            return value == static_cast<Uint8>(0xa5);
                        })) {
                        sentinelReads = 1;
                    }
                } else {
                    readMode = "sampled_rows";
                    const SizeT rowBytes = static_cast<SizeT>(readWidth) * 4u;
                    for (GLint row = 0; row < rows; ++row) {
                        Uint8* destination = rgba.data() + static_cast<SizeT>(row) * rowBytes;
                        const GLint readY = rows == 1 ? 0 : row * (alpha.height - 1) / (rows - 1);
                        g_GLESFuncs.glReadPixels(readX, readY, readWidth, 1,
                                                 GL_RGBA, GL_UNSIGNED_BYTE, destination);
                        if (std::all_of(destination, destination + rowBytes, [](Uint8 value) {
                                return value == static_cast<Uint8>(0xa5);
                            })) {
                            ++sentinelReads;
                        }
                    }
                }
                if (sentinelReads == 0) stats.Observe(rgba.data(), pixelCount);
            } else {
                status = "incomplete_fbo";
                ++g_pzf11Summary.incompleteFbo;
            }

            g_GLESFuncs.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_TEXTURE_2D, 0, 0);
            attachmentDetached = true;
        }

        g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_PIXELS, previousPackSkipPixels);
        g_GLESFuncs.glPixelStorei(GL_PACK_SKIP_ROWS, previousPackSkipRows);
        g_GLESFuncs.glPixelStorei(GL_PACK_ROW_LENGTH, previousPackRowLength);
        g_GLESFuncs.glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
        g_GLESFuncs.glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
        g_GLESFuncs.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFbo));
        g_GLESFuncs.glReadBuffer(static_cast<GLenum>(previousReadBuffer));

        if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE) {
            if (sentinelReads != 0) {
                status = "sentinel_no_data";
                ++g_pzf11Summary.sentinelReads;
            } else if (stats.pixels != 0) {
                status = "captured";
                ++g_pzf11Summary.successful;
                if (exactFullRead) ++g_pzf11Summary.exactFullReads;
                else ++g_pzf11Summary.sampledReads;
                g_pzf11Summary.pixels += stats.pixels;
                g_pzf11Summary.zeroRgba += stats.zeroRgba;
                g_pzf11Summary.rgbNonzero += stats.rgbNonzero;
                g_pzf11Summary.alphaNonzero += stats.alphaMid + stats.alphaFull;
            }
        }
    }

    const auto cpuClass = ::MobilePZ::PZF11::ClassifyGpu(cpuStats);
    if (cpuClass == ::MobilePZ::PZF11::GpuContentClass::AllZero) {
        ++g_pzf11Summary.cpuAllZero;
    } else if (cpuClass == ::MobilePZ::PZF11::GpuContentClass::RgbAlphaZero) {
        ++g_pzf11Summary.cpuRgbAlphaZero;
    } else if (cpuClass == ::MobilePZ::PZF11::GpuContentClass::NonTransparent) {
        ++g_pzf11Summary.cpuNonTransparent;
    }
    const Bool cpuAllZero = cpuClass == ::MobilePZ::PZF11::GpuContentClass::AllZero;
    const Bool cpuUseful = cpuClass == ::MobilePZ::PZF11::GpuContentClass::NonTransparent;
    const auto gpuClass = ::MobilePZ::PZF11::ClassifyGpu(stats);
    const auto comparison = ::MobilePZ::PZF11::Compare(cpuAllZero, cpuUseful, stats);
    PZF11ObserveComparison(comparison);
    MGLOG_I("MOBILEPZ_PZF11_MODEL_TEXTURE_GPU capture=%u generation=%u status=%s "
            "classification=%s gpu_class=%s serial=%llu present=%llu family=%s program=%u "
            "front_texture=%u texture_lifetime=%llu native_texture=%u level=%u size=%dx%d "
            "texture_target=%d upload_target=%d front_internal_format_enum=%d "
            "cpu_shadow_read=%d cpu_class=%s cpu_pixels=%llu cpu_zero_rgba=%llu "
            "cpu_rgb_nonzero=%llu cpu_alpha_zero=%llu cpu_alpha_mid=%llu cpu_alpha_full=%llu "
            "cpu_rgba_hash=%016llx cpu_alpha_sample_class=%s cpu_alpha_samples=%u "
            "cpu_alpha_sample_zero=%u cpu_alpha_sample_nonzero=%u "
            "read_mode=%s exact_full_read=%d read_x=%d read_width=%d rows=%d sentinel_reads=%u "
            "pixels=%llu zero_rgba=%llu rgb_nonzero=%llu alpha_zero=%llu alpha_mid=%llu "
            "alpha_full=%llu nontransparent_colour=%llu rgba_min=%u,%u,%u,%u "
            "rgba_max=%u,%u,%u,%u rgba_hash=%016llx read_fbo=%u fbo_status=0x%x "
            "blocking_readback=1 gl_finish=1 state_restored=1 attachment_detached=%d "
            "rendering_output_mutation=0 error_drain=0",
            capture, g_backendContextGeneration, status,
            ::MobilePZ::PZF11::ComparisonClassName(comparison),
            ::MobilePZ::PZF11::GpuContentClassName(gpuClass),
            static_cast<unsigned long long>(serial),
            static_cast<unsigned long long>(g_pzf3PresentSerial),
            PZF3FamilyName(draw.family), draw.external, alpha.frontTexture,
            static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
            alpha.level, alpha.width, alpha.height, static_cast<Int>(alpha.textureTarget),
            static_cast<Int>(alpha.uploadTarget), static_cast<Uint>(alpha.internalFormat),
            cpuShadowRead ? 1 : 0, ::MobilePZ::PZF11::CpuContentClassName(cpuClass),
            static_cast<unsigned long long>(cpuStats.pixels),
            static_cast<unsigned long long>(cpuStats.zeroRgba),
            static_cast<unsigned long long>(cpuStats.rgbNonzero),
            static_cast<unsigned long long>(cpuStats.alphaZero),
            static_cast<unsigned long long>(cpuStats.alphaMid),
            static_cast<unsigned long long>(cpuStats.alphaFull),
            static_cast<unsigned long long>(cpuStats.pixels ? cpuStats.hash : 0ull),
            PZF8AlphaClassName(alpha.classification), alpha.alphaSamples, alpha.alphaZero,
            alpha.alphaNonzero, readMode, exactFullRead ? 1 : 0, readX, readWidth, rows,
            sentinelReads, static_cast<unsigned long long>(stats.pixels),
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
            static_cast<unsigned long long>(stats.pixels ? stats.hash : 0ull),
            g_pzf11ReadFbo, framebufferStatus, attachmentDetached ? 1 : 0);
}

static void PZF11LogSummary(const char* reason) {
    MGLOG_I("MOBILEPZ_PZF11_SUMMARY reason=%s generation=%u selected=%u successful=%u "
            "all_zero_skinned=%u all_zero_static=%u useful_skinned=%u useful_static=%u "
            "exact_full_reads=%u sampled_reads=%u unsupported=%u bad_dimensions=%u "
            "incomplete_fbo=%u sentinel_reads=%u cpu_shadow_reads=%u cpu_shadow_missing=%u "
            "cpu_all_zero=%u cpu_rgb_alpha_zero=%u cpu_nontransparent=%u "
            "pixels=%llu zero_rgba=%llu rgb_nonzero=%llu "
            "alpha_nonzero=%llu cpu_zero_gpu_zero=%llu cpu_zero_gpu_rgb_alpha_zero=%llu "
            "cpu_zero_gpu_nontransparent=%llu cpu_useful_gpu_zero=%llu "
            "cpu_useful_gpu_rgb_alpha_zero=%llu cpu_useful_gpu_nontransparent=%llu "
            "max_captures_per_context=%u max_all_zero_per_family=%u "
            "max_useful_controls_per_family=%u full_read_max_bytes=%llu sampled_rows=%d "
            "blocking_readback=1 gl_finish=1 state_restored=1 attachment_detached=1 "
            "rendering_output_mutation=0 error_drain=0",
            reason ? reason : "unknown", g_pzf11Summary.generation,
            g_pzf11Summary.selected, g_pzf11Summary.successful,
            g_pzf11AllZeroPerFamily[0], g_pzf11AllZeroPerFamily[1],
            g_pzf11UsefulPerFamily[0], g_pzf11UsefulPerFamily[1],
            g_pzf11Summary.exactFullReads, g_pzf11Summary.sampledReads,
            g_pzf11Summary.unsupported, g_pzf11Summary.badDimensions,
            g_pzf11Summary.incompleteFbo, g_pzf11Summary.sentinelReads,
            g_pzf11Summary.cpuShadowReads, g_pzf11Summary.cpuShadowMissing,
            g_pzf11Summary.cpuAllZero, g_pzf11Summary.cpuRgbAlphaZero,
            g_pzf11Summary.cpuNonTransparent,
            static_cast<unsigned long long>(g_pzf11Summary.pixels),
            static_cast<unsigned long long>(g_pzf11Summary.zeroRgba),
            static_cast<unsigned long long>(g_pzf11Summary.rgbNonzero),
            static_cast<unsigned long long>(g_pzf11Summary.alphaNonzero),
            static_cast<unsigned long long>(g_pzf11Summary.cpuZeroGpuZero),
            static_cast<unsigned long long>(g_pzf11Summary.cpuZeroGpuRgbAlphaZero),
            static_cast<unsigned long long>(g_pzf11Summary.cpuZeroGpuNonTransparent),
            static_cast<unsigned long long>(g_pzf11Summary.cpuUsefulGpuZero),
            static_cast<unsigned long long>(g_pzf11Summary.cpuUsefulGpuRgbAlphaZero),
            static_cast<unsigned long long>(g_pzf11Summary.cpuUsefulGpuNonTransparent),
            kPZF11MaxCapturesPerContext, kPZF11MaxAllZeroPerFamily,
            kPZF11MaxUsefulControlsPerFamily,
            static_cast<unsigned long long>(kPZF11MaxFullReadBytes), kPZF11SampleRows);
}

static void PZF11OnContextReady(Uint generation) {
    g_pzf11Summary = {};
    g_pzf11Summary.generation = generation;
    g_pzf11AllZeroPerFamily = {};
    g_pzf11UsefulPerFamily = {};
    g_pzf11TextureLifetimes = {};
    g_pzf11TextureLifetimeCount = 0;
    g_pzf11ReadFbo = 0;
    MGLOG_I("MOBILEPZ_PZF11_CONTEXT_READY generation=%u captures_reset=1 read_fbo_reset=1 "
            "rendering_output_mutation=0 error_drain=0", generation);
}

static void PZF11OnContextDestroyed() {
    g_pzf11ReadFbo = 0;
    MGLOG_I("MOBILEPZ_PZF11_CONTEXT_NOT_READY generation=%u read_fbo=0 "
            "rendering_output_mutation=0 error_drain=0", g_backendContextGeneration);
}
#endif

static void PZF8CountAlpha(const PZF8PrimaryAlpha& alpha) {
    switch (alpha.classification) {
    case PZF8AlphaClass::Useful: ++g_pzf8UsefulAlphaDraws; break;
    case PZF8AlphaClass::AllZero: ++g_pzf8AllZeroAlphaDraws; break;
    case PZF8AlphaClass::Unknown: ++g_pzf8UnknownAlphaDraws; break;
    default: ++g_pzf8NoSamplerDraws; break;
    }
}

static void PZF8ObserveBatchDraw(Uint64 serial, const PZF3ProgramRecord& record,
                                 const PZF8PrimaryAlpha& alpha) {
    if (!g_pzf8Batch.active) return;
    ++g_pzf8Batch.draws;
    ++g_pzf8DrawsInBatches;
    g_pzf8Batch.lastSerial = serial;
    g_pzf8Batch.lastProgram = record.external;
    if (record.family == PZF3Family::Skinned) {
        ++g_pzf8Batch.skinnedDraws;
        ++g_pzf8SkinnedDrawsInBatches;
    } else {
        ++g_pzf8Batch.staticDraws;
        ++g_pzf8StaticDrawsInBatches;
    }
    if (alpha.classification == PZF8AlphaClass::Useful) {
        ++g_pzf8Batch.usefulAlphaDraws;
        ++g_pzf8UsefulDrawsInBatches;
    } else if (alpha.classification == PZF8AlphaClass::AllZero) {
        ++g_pzf8Batch.allZeroAlphaDraws;
        ++g_pzf8AllZeroDrawsInBatches;
    } else {
        ++g_pzf8Batch.unknownAlphaDraws;
        ++g_pzf8UnknownDrawsInBatches;
    }
}

static Bool PZF8StartBatch(Uint64 serial, const PZF3ProgramRecord& record,
                           const PZF8PrimaryAlpha& alpha) {
    const Int familySlot = PZF8FamilySlot(record.family);
    if (familySlot < 0 || alpha.classification != PZF8AlphaClass::Useful) return false;
    if (g_pzf8BatchesStarted >= kPZF8MaxBatchesPerContext) {
        ++g_pzf8RejectGlobalLimit;
        return false;
    }
    if (g_pzf8PrimaryFamilyStarted[static_cast<SizeT>(familySlot)] >=
        kPZF8MaxBatchesPerPrimaryFamily) {
        ++g_pzf8RejectFamilyLimit;
        return false;
    }
    if (g_pzf8LastStartedPresent == g_pzf3PresentSerial) {
        ++g_pzf8RejectSamePresent;
        return false;
    }

    PZF6DrawDeltaToken token;
    token.capture = g_pzf8BatchesStarted + 1;
    token.contextGeneration = g_backendContextGeneration;
    token.rowPhase = g_pzf8BatchesStarted;
    token.serial = serial;
    token.family = record.family;
    token.program = record.external;
    ++g_pzf8BeginAttempts;
    if (!PZF8PrepareReadbackToken(token)) return false;
    if (!PZF6ReadRows(token, token.before, token.beforeSentinelRows, token.beforeFboStatus)) {
        if (token.beforeFboStatus != GL_FRAMEBUFFER_COMPLETE) ++g_pzf8ReadbackIncomplete;
        else ++g_pzf8ReadbackEmpty;
        return false;
    }

    token.active = true;
    g_pzf8Batch = {};
    g_pzf8Batch.active = true;
    g_pzf8Batch.readback = std::move(token);
    g_pzf8Batch.primaryFamily = static_cast<Uint>(familySlot);
    g_pzf8Batch.startPresent = g_pzf3PresentSerial;
    g_pzf8Batch.firstSerial = g_pzf8Batch.lastSerial = serial;
    g_pzf8Batch.firstProgram = g_pzf8Batch.lastProgram = record.external;
    g_pzf8Batch.firstAlpha = alpha;
    ++g_pzf8BatchesStarted;
    ++g_pzf8PrimaryFamilyStarted[static_cast<SizeT>(familySlot)];
    g_pzf8LastStartedPresent = g_pzf3PresentSerial;

    const auto& rb = g_pzf8Batch.readback;
    MGLOG_I("MOBILEPZ_PZF8_BATCH_BEGIN capture=%u generation=%u present=%llu "
            "first_serial=%llu primary_family=%s first_program=%u draw_fbo=%d texture=%d "
            "level=%d layer=%d viewport=%d,%d,%d,%d read_x=%d read_width=%d rows=%zu "
            "row_phase=%u input_alpha_class=%s input_front_texture=%u "
            "input_native_texture=%u input_alpha_samples=%u input_alpha_zero=%u "
            "input_alpha_nonzero=%u fbo_status_before=0x%x sentinel_rows_before=%u "
            "blocking_readback=1 state_restored=1 rendering_output_mutation=0 error_drain=0",
            rb.capture, g_backendContextGeneration,
            static_cast<unsigned long long>(g_pzf3PresentSerial),
            static_cast<unsigned long long>(serial), PZF3FamilyName(record.family),
            record.external, rb.fbo, rb.texture, rb.level, rb.layer,
            rb.viewport[0], rb.viewport[1], rb.viewport[2], rb.viewport[3], rb.readX,
            rb.readWidth, rb.rowY.size(), rb.rowPhase, PZF8AlphaClassName(alpha.classification),
            alpha.frontTexture, alpha.nativeTexture, alpha.alphaSamples, alpha.alphaZero,
            alpha.alphaNonzero, rb.beforeFboStatus, rb.beforeSentinelRows);
    return true;
}

static void PZF8FinalizeBatch(const char* reason) {
    if (!g_pzf8Batch.active) return;
    g_pzf8Batch.active = false;
    PZF6DrawDeltaToken& token = g_pzf8Batch.readback;
    token.active = false;
    Vector<Uint8> after;
    Uint afterSentinelRows = 0;
    GLenum afterFboStatus = 0;
    const Bool validContext = token.contextGeneration == g_backendContextGeneration &&
        g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration);
    if (!validContext || !PZF6ReadRows(token, after, afterSentinelRows, afterFboStatus) ||
        after.size() != token.before.size()) {
        if (afterFboStatus != GL_FRAMEBUFFER_COMPLETE) ++g_pzf8ReadbackIncomplete;
        else ++g_pzf8ReadbackEmpty;
        MGLOG_I("MOBILEPZ_PZF8_BATCH_RESULT capture=%u generation=%u status=after_no_data "
                "end_reason=%s first_serial=%llu last_serial=%llu draw_fbo=%d texture=%d "
                "draws=%llu useful_alpha_draws=%llu all_zero_alpha_draws=%llu "
                "unknown_alpha_draws=%llu before_bytes=%zu after_bytes=%zu "
                "fbo_status_before=0x%x fbo_status_after=0x%x state_restored=1 "
                "rendering_output_mutation=0 error_drain=0",
                token.capture, g_backendContextGeneration, reason ? reason : "unknown",
                static_cast<unsigned long long>(g_pzf8Batch.firstSerial),
                static_cast<unsigned long long>(g_pzf8Batch.lastSerial), token.fbo, token.texture,
                static_cast<unsigned long long>(g_pzf8Batch.draws),
                static_cast<unsigned long long>(g_pzf8Batch.usefulAlphaDraws),
                static_cast<unsigned long long>(g_pzf8Batch.allZeroAlphaDraws),
                static_cast<unsigned long long>(g_pzf8Batch.unknownAlphaDraws),
                token.before.size(), after.size(), token.beforeFboStatus, afterFboStatus);
        g_pzf8Batch = {};
        return;
    }

    ::MobilePZ::PZF5::ReadbackStats beforeStats, afterStats;
    ::MobilePZ::PZF6::DrawDeltaStats delta;
    const SizeT pixels = token.before.size() / 4u;
    beforeStats.Observe(token.before.data(), pixels);
    afterStats.Observe(after.data(), pixels);
    delta.Observe(token.before.data(), after.data(), pixels);
    ++g_pzf8CompletedBatches;
    if (delta.changed != 0) ++g_pzf8ChangedBatches;
    else ++g_pzf8NoChangeBatches;
    g_pzf8Pixels += delta.pixels;
    g_pzf8ChangedPixels += delta.changed;
    g_pzf8RgbChangedPixels += delta.rgbChanged;
    g_pzf8AlphaChangedPixels += delta.alphaChanged;
    g_pzf8NonTransparentGain += delta.nonTransparentGain;
    g_pzf8NonTransparentLoss += delta.nonTransparentLoss;

    auto& completed = g_pzf8Completed[token.capture - 1];
    completed.valid = true;
    completed.contextGeneration = token.contextGeneration;
    completed.capture = token.capture;
    completed.fbo = token.fbo;
    completed.texture = token.texture;
    completed.firstSerial = g_pzf8Batch.firstSerial;
    completed.lastSerial = g_pzf8Batch.lastSerial;
    completed.changedPixels = delta.changed;
    ++g_pzf8PendingConsumers;

    Uint64 rowHash = 0x505a463800000001ull;
    for (GLint row : token.rowY) rowHash = PZF4Mix(rowHash, static_cast<Uint64>(row));
    const double changedPercent = delta.pixels
        ? 100.0 * static_cast<double>(delta.changed) / delta.pixels : 0.0;
    MGLOG_I("MOBILEPZ_PZF8_BATCH_RESULT capture=%u generation=%u status=%s "
            "end_reason=%s start_present=%llu end_present=%llu first_serial=%llu "
            "last_serial=%llu primary_family=%s first_program=%u last_program=%u "
            "draw_fbo=%d texture=%d level=%d layer=%d draws=%llu skinned_draws=%llu "
            "static_draws=%llu useful_alpha_draws=%llu all_zero_alpha_draws=%llu "
            "unknown_alpha_draws=%llu input_front_texture=%u input_native_texture=%u "
            "input_alpha_samples=%u input_alpha_zero=%u input_alpha_nonzero=%u "
            "rows=%zu row_phase=%u row_y_hash=%016llx pixels=%llu changed=%llu "
            "changed_percent=%g rgb_changed=%llu alpha_changed=%llu rgb_gain=%llu "
            "rgb_loss=%llu alpha_gain=%llu alpha_loss=%llu nontransparent_gain=%llu "
            "nontransparent_loss=%llu before_nontransparent=%llu after_nontransparent=%llu "
            "before_hash=%016llx after_hash=%016llx fbo_status_before=0x%x "
            "fbo_status_after=0x%x sentinel_rows_before=%u sentinel_rows_after=%u "
            "consumer_pending=1 blocking_readback=1 same_regions=1 target_verified=1 "
            "state_restored=1 rendering_output_mutation=0 error_drain=0",
            token.capture, g_backendContextGeneration,
            delta.changed != 0 ? "captured_changed" : "captured_no_change",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(g_pzf8Batch.startPresent),
            static_cast<unsigned long long>(g_pzf3PresentSerial),
            static_cast<unsigned long long>(g_pzf8Batch.firstSerial),
            static_cast<unsigned long long>(g_pzf8Batch.lastSerial),
            g_pzf8Batch.primaryFamily == 0 ? "SKINNED" : "STATIC_MODEL",
            g_pzf8Batch.firstProgram, g_pzf8Batch.lastProgram, token.fbo, token.texture,
            token.level, token.layer, static_cast<unsigned long long>(g_pzf8Batch.draws),
            static_cast<unsigned long long>(g_pzf8Batch.skinnedDraws),
            static_cast<unsigned long long>(g_pzf8Batch.staticDraws),
            static_cast<unsigned long long>(g_pzf8Batch.usefulAlphaDraws),
            static_cast<unsigned long long>(g_pzf8Batch.allZeroAlphaDraws),
            static_cast<unsigned long long>(g_pzf8Batch.unknownAlphaDraws),
            g_pzf8Batch.firstAlpha.frontTexture, g_pzf8Batch.firstAlpha.nativeTexture,
            g_pzf8Batch.firstAlpha.alphaSamples, g_pzf8Batch.firstAlpha.alphaZero,
            g_pzf8Batch.firstAlpha.alphaNonzero, token.rowY.size(), token.rowPhase,
            static_cast<unsigned long long>(rowHash), static_cast<unsigned long long>(delta.pixels),
            static_cast<unsigned long long>(delta.changed), changedPercent,
            static_cast<unsigned long long>(delta.rgbChanged),
            static_cast<unsigned long long>(delta.alphaChanged),
            static_cast<unsigned long long>(delta.rgbGain),
            static_cast<unsigned long long>(delta.rgbLoss),
            static_cast<unsigned long long>(delta.alphaGain),
            static_cast<unsigned long long>(delta.alphaLoss),
            static_cast<unsigned long long>(delta.nonTransparentGain),
            static_cast<unsigned long long>(delta.nonTransparentLoss),
            static_cast<unsigned long long>(beforeStats.nonTransparentColour),
            static_cast<unsigned long long>(afterStats.nonTransparentColour),
            static_cast<unsigned long long>(beforeStats.hash),
            static_cast<unsigned long long>(afterStats.hash), token.beforeFboStatus,
            afterFboStatus, token.beforeSentinelRows, afterSentinelRows);
    g_pzf8Batch = {};
}

static void PZF8CheckConsumer(
    Uint64 serial, const PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend) {
    if (g_pzf8PendingConsumers == 0 || g_pzf8ConsumerChecks >= kPZF8MaxConsumerChecks ||
        PZF8FamilySlot(record.family) >= 0) return;
    ++g_pzf8ConsumerChecks;
    for (auto& completed : g_pzf8Completed) {
        if (!completed.valid || completed.consumerMatched ||
            completed.contextGeneration != g_backendContextGeneration) continue;
        Uint samplerOrdinal = 0, frontendTexture = 0;
        Uint64 textureLifetime = 0;
        if (!PZF5FindSamplerLineage(program, backend, completed.texture,
                                    samplerOrdinal, frontendTexture, textureLifetime)) continue;
        completed.consumerMatched = true;
        ++g_pzf8ConsumerMatches;
        --g_pzf8PendingConsumers;
        MGLOG_I("MOBILEPZ_PZF8_CONSUMER capture=%u generation=%u producer_fbo=%d "
                "producer_texture=%d producer_first_serial=%llu producer_last_serial=%llu "
                "producer_changed_pixels=%llu consumer_serial=%llu consumer_present=%llu "
                "consumer_family=%s consumer_program=%u sampler_ordinal=%u "
                "consumer_front_texture=%u consumer_texture_lifetime=%llu "
                "lineage_match=1 rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
                completed.capture, g_backendContextGeneration, completed.fbo, completed.texture,
                static_cast<unsigned long long>(completed.firstSerial),
                static_cast<unsigned long long>(completed.lastSerial),
                static_cast<unsigned long long>(completed.changedPixels),
                static_cast<unsigned long long>(serial),
                static_cast<unsigned long long>(g_pzf3PresentSerial),
                PZF3FamilyName(record.family), record.external, samplerOrdinal,
                frontendTexture, static_cast<unsigned long long>(textureLifetime));
    }
}

static void PZF8BeforeDraw(
    Uint64 serial, const PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend, GLsizei count, GLsizei instances) {
    if (!g_pzf4Lifecycle.ContextReadyFor(g_backendContextGeneration)) return;
    const Bool modelDraw = PZF8FamilySlot(record.family) >= 0 && count > 0 && instances > 0;
    if (g_pzf8Batch.active) {
        Bool boundary = !modelDraw;
        if (modelDraw) {
            GLint currentFbo = 0;
            if (!g_GLESFuncs.glGetIntegerv) boundary = true;
            else {
                g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentFbo);
                boundary = currentFbo != g_pzf8Batch.readback.fbo;
            }
        }
        if (boundary) PZF8FinalizeBatch(modelDraw ? "fbo_boundary" : "non_model_boundary");
    }
    PZF8CheckConsumer(serial, record, program, backend);
    if (!modelDraw || (!g_pzf8Batch.active &&
        g_pzf8BatchesStarted >= kPZF8MaxBatchesPerContext)) return;

    ++g_pzf8ModelDrawsObserved;
    const PZF8PrimaryAlpha alpha = PZF8InspectPrimaryAlpha(program, backend);
    PZF8CountAlpha(alpha);
#ifdef MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
    PZF9LogModelTexture(serial, record, alpha);
#endif
#ifdef MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
    PZF10LogModelTexture(serial, record, alpha);
#endif
#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
    PZF11ReadModelTexture(serial, record, program, backend, alpha);
#endif
    PZF8LogAlpha(serial, record, alpha);
    if (!g_pzf8Batch.active && !PZF8StartBatch(serial, record, alpha)) return;
    PZF8ObserveBatchDraw(serial, record, alpha);
}

static void PZF8LogSummary(const char* reason) {
    MGLOG_I("MOBILEPZ_PZF8_SUMMARY reason=%s presents=%llu context_generation=%u "
            "model_draws_observed=%llu alpha_useful=%llu alpha_all_zero=%llu "
            "alpha_unknown=%llu no_sampler=%llu alpha_logs=%llu begin_attempts=%llu "
            "batches_started=%u primary_skinned=%u primary_static=%u completed_batches=%llu "
            "changed_batches=%llu no_change_batches=%llu readback_unsupported=%llu "
            "readback_incomplete=%llu readback_empty=%llu reject_same_present=%llu "
            "reject_family_limit=%llu reject_global_limit=%llu draws_in_batches=%llu "
            "skinned_draws=%llu static_draws=%llu useful_draws_in_batches=%llu "
            "all_zero_draws_in_batches=%llu unknown_draws_in_batches=%llu pixels=%llu "
            "changed_pixels=%llu rgb_changed_pixels=%llu alpha_changed_pixels=%llu "
            "nontransparent_gain=%llu nontransparent_loss=%llu consumer_checks=%llu "
            "consumer_matches=%llu pending_consumers=%llu active_batch=%d "
            "max_batches_per_context=%u max_batches_per_primary_family=%u rows_per_batch=%d "
            "blocking_readback=1 same_regions=1 state_restored=1 "
            "rendering_output_mutation=0 error_drain=0",
            reason ? reason : "unknown", static_cast<unsigned long long>(g_pzf3PresentSerial),
            g_backendContextGeneration, static_cast<unsigned long long>(g_pzf8ModelDrawsObserved),
            static_cast<unsigned long long>(g_pzf8UsefulAlphaDraws),
            static_cast<unsigned long long>(g_pzf8AllZeroAlphaDraws),
            static_cast<unsigned long long>(g_pzf8UnknownAlphaDraws),
            static_cast<unsigned long long>(g_pzf8NoSamplerDraws),
            static_cast<unsigned long long>(g_pzf8AlphaLogs),
            static_cast<unsigned long long>(g_pzf8BeginAttempts), g_pzf8BatchesStarted,
            g_pzf8PrimaryFamilyStarted[0], g_pzf8PrimaryFamilyStarted[1],
            static_cast<unsigned long long>(g_pzf8CompletedBatches),
            static_cast<unsigned long long>(g_pzf8ChangedBatches),
            static_cast<unsigned long long>(g_pzf8NoChangeBatches),
            static_cast<unsigned long long>(g_pzf8ReadbackUnsupported),
            static_cast<unsigned long long>(g_pzf8ReadbackIncomplete),
            static_cast<unsigned long long>(g_pzf8ReadbackEmpty),
            static_cast<unsigned long long>(g_pzf8RejectSamePresent),
            static_cast<unsigned long long>(g_pzf8RejectFamilyLimit),
            static_cast<unsigned long long>(g_pzf8RejectGlobalLimit),
            static_cast<unsigned long long>(g_pzf8DrawsInBatches),
            static_cast<unsigned long long>(g_pzf8SkinnedDrawsInBatches),
            static_cast<unsigned long long>(g_pzf8StaticDrawsInBatches),
            static_cast<unsigned long long>(g_pzf8UsefulDrawsInBatches),
            static_cast<unsigned long long>(g_pzf8AllZeroDrawsInBatches),
            static_cast<unsigned long long>(g_pzf8UnknownDrawsInBatches),
            static_cast<unsigned long long>(g_pzf8Pixels),
            static_cast<unsigned long long>(g_pzf8ChangedPixels),
            static_cast<unsigned long long>(g_pzf8RgbChangedPixels),
            static_cast<unsigned long long>(g_pzf8AlphaChangedPixels),
            static_cast<unsigned long long>(g_pzf8NonTransparentGain),
            static_cast<unsigned long long>(g_pzf8NonTransparentLoss),
            static_cast<unsigned long long>(g_pzf8ConsumerChecks),
            static_cast<unsigned long long>(g_pzf8ConsumerMatches),
            static_cast<unsigned long long>(g_pzf8PendingConsumers),
            g_pzf8Batch.active ? 1 : 0, kPZF8MaxBatchesPerContext,
            kPZF8MaxBatchesPerPrimaryFamily, kPZF8ReadRows);
#ifdef MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
    PZF9LogSummary(reason);
#endif
#ifdef MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
    PZF10LogSummary(reason);
#endif
#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
    PZF11LogSummary(reason);
#endif
}

static void PZF8ResetContext() {
    g_pzf8Batch = {};
    g_pzf8Completed = {};
    g_pzf8PrimaryFamilyStarted = {};
    g_pzf8BatchesStarted = 0;
    g_pzf8LastStartedPresent = std::numeric_limits<Uint64>::max();
    g_pzf8ModelDrawsObserved = g_pzf8UsefulAlphaDraws = g_pzf8AllZeroAlphaDraws = 0;
    g_pzf8UnknownAlphaDraws = g_pzf8NoSamplerDraws = g_pzf8AlphaLogs = 0;
    g_pzf8RejectSamePresent = g_pzf8RejectFamilyLimit = g_pzf8RejectGlobalLimit = 0;
    g_pzf8BeginAttempts = g_pzf8CompletedBatches = g_pzf8ChangedBatches = 0;
    g_pzf8NoChangeBatches = g_pzf8ReadbackUnsupported = g_pzf8ReadbackIncomplete = 0;
    g_pzf8ReadbackEmpty = g_pzf8Pixels = g_pzf8ChangedPixels = 0;
    g_pzf8RgbChangedPixels = g_pzf8AlphaChangedPixels = 0;
    g_pzf8NonTransparentGain = g_pzf8NonTransparentLoss = 0;
    g_pzf8DrawsInBatches = g_pzf8SkinnedDrawsInBatches = g_pzf8StaticDrawsInBatches = 0;
    g_pzf8UsefulDrawsInBatches = g_pzf8AllZeroDrawsInBatches = 0;
    g_pzf8UnknownDrawsInBatches = g_pzf8ConsumerChecks = g_pzf8ConsumerMatches = 0;
    g_pzf8PendingConsumers = 0;
}

static void PZF8OnContextReady(Uint generation) {
    PZF8ResetContext();
#ifdef MOBILEPZ_PZF9_MODEL_TEXTURE_UPLOAD_PROVENANCE
    g_pzf9ModelLogLines = 0;
    ::MobilePZ::PZF9::OnContextReady(generation);
#endif
#ifdef MOBILEPZ_PZF10_COPY_TEXTURE_REPAIR
    g_pzf10ModelLogLines = 0;
    ::MobilePZ::PZF10::OnContextReady(generation);
#endif
#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
    PZF11OnContextReady(generation);
#endif
    MGLOG_I("MOBILEPZ_PZF8_CONTEXT_READY generation=%u batches_reset=1 "
            "completed_lineage_reset=1 rendering_output_mutation=0 error_drain=0", generation);
}

static void PZF8OnPresentBoundary() { PZF8FinalizeBatch("present_boundary"); }

static void PZF8OnContextDestroyed() {
#ifdef MOBILEPZ_PZF11_MODEL_TEXTURE_GPU_READBACK
    PZF11OnContextDestroyed();
#endif
    g_pzf8Batch = {};
    g_pzf8Completed = {};
    g_pzf8PendingConsumers = 0;
    MGLOG_I("MOBILEPZ_PZF8_CONTEXT_NOT_READY generation=%u active_batch=0 "
            "pending_consumers=0 rendering_output_mutation=0 error_drain=0",
            g_backendContextGeneration);
}

#endif // MOBILEPZ_PZF8_OPAQUE_MODEL_BATCH_DELTA
