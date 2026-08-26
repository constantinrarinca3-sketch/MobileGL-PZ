// MobilePZ PZF13 - producer lineage for the null-allocated model textures.
// Included after PZF12NullRenderTarget.inc inside DirectGLES' anonymous namespace.

constexpr SizeT kPZF13MaxIdentityKeys = 512;
constexpr Uint64 kPZF13MaxProducerLogLines = 256;
constexpr Uint64 kPZF13MaxModelLogLines = 256;

struct PZF13IdentityKey {
    Bool used = false;
    Uint64 lifetime = 0;
    Uint frontendFbo = 0;
    FramebufferAttachmentType frontendAttachment = FramebufferAttachmentType::None;
    Uint64 clientSequence = 0;
    Uint expectedNative = 0;
};

static Array<PZF13IdentityKey, kPZF13MaxIdentityKeys> g_pzf13IdentityKeys{};
static Uint64 g_pzf13IdentityOverflow = 0;
static Uint64 g_pzf13CandidateWrites = 0;
static Uint64 g_pzf13ProducerLogLines = 0;
static Uint64 g_pzf13ModelLogLines = 0;

static Bool PZF13ReserveIdentity(Uint64 lifetime, Uint frontendFbo,
                                 FramebufferAttachmentType frontendAttachment,
                                 Uint64 clientSequence, Uint expectedNative) {
    for (const auto& key : g_pzf13IdentityKeys) {
        if (key.used && key.lifetime == lifetime && key.frontendFbo == frontendFbo &&
            key.frontendAttachment == frontendAttachment &&
            key.clientSequence == clientSequence && key.expectedNative == expectedNative) {
            return false;
        }
    }
    for (auto& key : g_pzf13IdentityKeys) {
        if (key.used) continue;
        key.used = true;
        key.lifetime = lifetime;
        key.frontendFbo = frontendFbo;
        key.frontendAttachment = frontendAttachment;
        key.clientSequence = clientSequence;
        key.expectedNative = expectedNative;
        return true;
    }
    ++g_pzf13IdentityOverflow;
    return false;
}

static void PZF13InspectFramebuffer(
    const SharedPtr<MG_State::GLState::FramebufferObject>& frontendFbo,
    ::MobilePZ::PZF13::WriteKind operation) {
    if (!frontendFbo || frontendFbo->IsDefaultFramebuffer()) return;

    auto* backendFboSlot = FramebufferImpl::g_backendFramebufferObjects.Find(frontendFbo.get());
    const auto backendFbo = backendFboSlot ? *backendFboSlot : nullptr;

    for (const auto frontendAttachment : frontendFbo->GetDrawBuffers()) {
        if (frontendAttachment < FramebufferAttachmentType::Color0 ||
            frontendAttachment > FramebufferAttachmentType::Color31) {
            continue;
        }
        const auto& attachment = frontendFbo->GetAttachment(frontendAttachment);
        if (!attachment.IsTexture()) continue;
        const auto& texture = attachment.GetTexture();
        ::MobilePZ::PZF9::Record upload;
        if (!PZF12CandidateTexture(texture, attachment, upload)) continue;

        ++g_pzf13CandidateWrites;
        ::MobilePZ::PZF13::RecordWrite(
            texture->GetLifetimeId(), texture->GetExternalIndex(), frontendFbo->GetExternalIndex(),
            static_cast<Int>(frontendAttachment), operation);
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
        if (operation != ::MobilePZ::PZF13::WriteKind::Draw) {
            ::MobilePZ::PZF14::RecordProducerWrite(
                g_backendContextGeneration, texture->GetLifetimeId(), texture->GetExternalIndex(),
                frontendFbo->GetExternalIndex(), static_cast<Int>(frontendAttachment),
                upload.clientSequence, static_cast<Uint>(operation));
        }
#endif

        if (!backendFbo) continue;
        auto* backendTextureSlot = TextureImpl::g_backendTextureObjects.Find(texture.get());
        if (!backendTextureSlot || !*backendTextureSlot) continue;
        const Uint expectedNative = (*backendTextureSlot)->GetBackendTextureId();
        if (!PZF13ReserveIdentity(texture->GetLifetimeId(), frontendFbo->GetExternalIndex(),
                                  frontendAttachment, upload.clientSequence, expectedNative)) {
            continue;
        }

        const GLenum backendAttachment = backendFbo->GetBackendAttachmentType(frontendAttachment);
        const Uint expectedFbo = backendFbo->GetBackendFramebufferId();
        Uint boundFbo = FramebufferImpl::CurrentFramebufferBinding(FramebufferTarget::Draw);
        if (boundFbo != expectedFbo) {
            backendFbo->Bind(FramebufferTarget::Draw);
            boundFbo = FramebufferImpl::CurrentFramebufferBinding(FramebufferTarget::Draw);
        }

        GLint objectTypeBefore = GL_NONE;
        GLint objectNameBefore = 0;
        g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
            GL_DRAW_FRAMEBUFFER, backendAttachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
            &objectTypeBefore);
        g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
            GL_DRAW_FRAMEBUFFER, backendAttachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
            &objectNameBefore);
        const GLenum statusBefore = g_GLESFuncs.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        const Bool mismatch = objectTypeBefore != GL_TEXTURE ||
                              objectNameBefore != static_cast<GLint>(expectedNative);

        Bool repaired = false;
        GLint objectTypeAfter = objectTypeBefore;
        GLint objectNameAfter = objectNameBefore;
        GLenum statusAfter = statusBefore;
        if (mismatch) {
            backendFbo->InvalidateSyncedState();
            backendFbo->SyncToBackend(frontendFbo, FramebufferTarget::Draw);
            backendFbo->Bind(FramebufferTarget::Draw);
            g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, backendAttachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                &objectTypeAfter);
            g_GLESFuncs.glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, backendAttachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                &objectNameAfter);
            statusAfter = g_GLESFuncs.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            repaired = objectTypeAfter == GL_TEXTURE &&
                       objectNameAfter == static_cast<GLint>(expectedNative) &&
                       statusAfter == GL_FRAMEBUFFER_COMPLETE;
        }
        ::MobilePZ::PZF13::RecordIdentity(texture->GetLifetimeId(), mismatch, repaired);

        if (g_pzf13ProducerLogLines < kPZF13MaxProducerLogLines) {
            ++g_pzf13ProducerLogLines;
            MGLOG_I("MOBILEPZ_PZF13_PRODUCER generation=%u operation=%s front_fbo=%u "
                    "backend_fbo=%u bound_fbo=%u front_attachment=%d backend_attachment=0x%x "
                    "front_texture=%u texture_lifetime=%llu native_expected=%u "
                    "object_type_before=0x%x native_before=%d status_before=0x%x "
                    "mismatch=%d repaired=%d object_type_after=0x%x native_after=%d "
                    "status_after=0x%x client_sequence=%llu null_image_calls=%llu "
                    "matching_path_mutation=0 mismatch_path_mutation=%d error_drain=0",
                    g_backendContextGeneration, ::MobilePZ::PZF13::WriteKindName(operation),
                    frontendFbo->GetExternalIndex(), expectedFbo, boundFbo,
                    static_cast<Int>(frontendAttachment), backendAttachment,
                    texture->GetExternalIndex(),
                    static_cast<unsigned long long>(texture->GetLifetimeId()), expectedNative,
                    objectTypeBefore, objectNameBefore, statusBefore, mismatch ? 1 : 0,
                    repaired ? 1 : 0, objectTypeAfter, objectNameAfter, statusAfter,
                    static_cast<unsigned long long>(upload.clientSequence),
                    static_cast<unsigned long long>(upload.nullImageCalls), mismatch ? 1 : 0);
        }
    }
}

static void PZF13BeforeFramebufferWrite(const char*, ::MobilePZ::PZF13::WriteKind operation) {
    if (!MG_State::pGLContext || !g_GLESFuncs.glGetFramebufferAttachmentParameteriv ||
        !g_GLESFuncs.glCheckFramebufferStatus) {
        return;
    }
    const auto& slot = MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw);
    PZF13InspectFramebuffer(slot.GetBoundObject(), operation);
}

static void PZF13BeforeNamedFramebufferWrite(
    const SharedPtr<MG_State::GLState::FramebufferObject>& framebuffer,
    const char*, ::MobilePZ::PZF13::WriteKind operation) {
    if (!g_GLESFuncs.glGetFramebufferAttachmentParameteriv || !g_GLESFuncs.glCheckFramebufferStatus) return;
    PZF13InspectFramebuffer(framebuffer, operation);
}

static void PZF13BeforeModelDraw(
    Uint64 serial, const PZF3ProgramRecord& record,
    const SharedPtr<MG_State::GLState::ProgramObject>& program,
    PrgramImpl::BackendProgramObjectImpl* backend) {
    if (PZF8FamilySlot(record.family) < 0) return;
    const PZF8PrimaryAlpha alpha = PZF8InspectPrimaryAlpha(program, backend);
    if (alpha.classification != PZF8AlphaClass::AllZero || alpha.textureLifetime == 0) return;
    ::MobilePZ::PZF9::Record upload;
    if (!::MobilePZ::PZF9::PeekRecord(alpha.textureLifetime, upload) ||
        upload.lastKind != ::MobilePZ::PZF9::UploadKind::NullImage2D) {
        return;
    }
    if (g_pzf13ModelLogLines >= kPZF13MaxModelLogLines) return;

    ::MobilePZ::PZF13::Record lineage;
    if (!::MobilePZ::PZF13::TakeConsumerSnapshot(alpha.textureLifetime, lineage)) return;
    ++g_pzf13ModelLogLines;
    const Uint64 framebufferWrites = lineage.drawWrites + lineage.clearWrites +
        lineage.clearBufferWrites + lineage.blitWrites + lineage.namedClearWrites +
        lineage.namedBlitWrites;
    const Uint64 directWrites = lineage.directClearWrites + lineage.directUploadWrites;
    const char* classification = "NO_PRODUCER_PATH";
    if (lineage.repairsSucceeded != 0) classification = "STALE_ATTACHMENT_REPAIRED";
    else if (framebufferWrites != 0) classification = "FRAMEBUFFER_PRODUCER_OBSERVED";
    else if (directWrites != 0) classification = "DIRECT_TEXTURE_PRODUCER_OBSERVED";
    else if (lineage.imageWriteBinds != 0) classification = "IMAGE_WRITE_BIND_OBSERVED";
    else if (lineage.currentlyAttached || lineage.attachCalls != 0)
        classification = "ATTACHED_WITHOUT_POPULATION";

    MGLOG_I("MOBILEPZ_PZF13_MODEL_TEXTURE generation=%u serial=%llu present=%llu "
            "family=%s program=%u front_texture=%u texture_lifetime=%llu native_texture=%u "
            "classification=%s client_sequence=%llu null_image_calls=%llu "
            "tracked_null_allocations=%llu attach_calls=%llu detach_calls=%llu attached_now=%d "
            "status_checks=%llu status_complete=%llu status_incomplete=%llu last_status=0x%x "
            "draw_writes=%llu clear_writes=%llu clear_buffer_writes=%llu blit_writes=%llu "
            "named_clear_writes=%llu named_blit_writes=%llu direct_clear_writes=%llu "
            "direct_upload_writes=%llu image_write_binds=%llu identity_matches=%llu "
            "identity_mismatches=%llu repairs_succeeded=%llu repairs_failed=%llu "
            "last_fbo=%u last_attachment=%d last_write=%s rendering_output_mutation=%d "
            "additional_gl_calls=bounded error_drain=0",
            g_backendContextGeneration, static_cast<unsigned long long>(serial),
            static_cast<unsigned long long>(g_pzf3PresentSerial), PZF3FamilyName(record.family),
            record.external, alpha.frontTexture,
            static_cast<unsigned long long>(alpha.textureLifetime), alpha.nativeTexture,
            classification, static_cast<unsigned long long>(upload.clientSequence),
            static_cast<unsigned long long>(upload.nullImageCalls),
            static_cast<unsigned long long>(lineage.nullAllocations),
            static_cast<unsigned long long>(lineage.attachCalls),
            static_cast<unsigned long long>(lineage.detachCalls), lineage.currentlyAttached ? 1 : 0,
            static_cast<unsigned long long>(lineage.statusChecks),
            static_cast<unsigned long long>(lineage.statusComplete),
            static_cast<unsigned long long>(lineage.statusIncomplete), lineage.lastStatus,
            static_cast<unsigned long long>(lineage.drawWrites),
            static_cast<unsigned long long>(lineage.clearWrites),
            static_cast<unsigned long long>(lineage.clearBufferWrites),
            static_cast<unsigned long long>(lineage.blitWrites),
            static_cast<unsigned long long>(lineage.namedClearWrites),
            static_cast<unsigned long long>(lineage.namedBlitWrites),
            static_cast<unsigned long long>(lineage.directClearWrites),
            static_cast<unsigned long long>(lineage.directUploadWrites),
            static_cast<unsigned long long>(lineage.imageWriteBinds),
            static_cast<unsigned long long>(lineage.identityMatches),
            static_cast<unsigned long long>(lineage.identityMismatches),
            static_cast<unsigned long long>(lineage.repairsSucceeded),
            static_cast<unsigned long long>(lineage.repairsFailed), lineage.lastFbo,
            lineage.lastAttachment, ::MobilePZ::PZF13::WriteKindName(lineage.lastWrite),
            lineage.repairsSucceeded != 0 ? 1 : 0);
}

static void PZF13LogSummary(const char* reason) {
    const auto summary = ::MobilePZ::PZF13::GetSummary();
    MGLOG_I("MOBILEPZ_PZF13_SUMMARY reason=%s generation=%u presents=%llu records=%llu "
            "record_overflow=%llu null_allocations=%llu attaches=%llu detaches=%llu "
            "status_checks=%llu framebuffer_writes=%llu direct_writes=%llu "
            "image_write_binds=%llu candidate_writes=%llu identity_queries=%llu "
            "identity_matches=%llu identity_mismatches=%llu repairs_succeeded=%llu "
            "repairs_failed=%llu identity_key_overflow=%llu consumers=%llu consumer_logs=%llu "
            "producer_log_lines=%llu model_log_lines=%llu max_records=%zu "
            "max_identity_keys=%zu max_producer_log_lines=%llu max_model_log_lines=%llu "
            "matching_path_mutation=0 mismatch_path_mutation=fbo_attachment_resync error_drain=0",
            reason ? reason : "unknown", summary.generation,
            static_cast<unsigned long long>(g_pzf3PresentSerial),
            static_cast<unsigned long long>(summary.records),
            static_cast<unsigned long long>(summary.recordOverflow),
            static_cast<unsigned long long>(summary.nullAllocations),
            static_cast<unsigned long long>(summary.attaches),
            static_cast<unsigned long long>(summary.detaches),
            static_cast<unsigned long long>(summary.statusChecks),
            static_cast<unsigned long long>(summary.framebufferWrites),
            static_cast<unsigned long long>(summary.directWrites),
            static_cast<unsigned long long>(summary.imageWriteBinds),
            static_cast<unsigned long long>(g_pzf13CandidateWrites),
            static_cast<unsigned long long>(summary.identityQueries),
            static_cast<unsigned long long>(summary.identityMatches),
            static_cast<unsigned long long>(summary.identityMismatches),
            static_cast<unsigned long long>(summary.repairsSucceeded),
            static_cast<unsigned long long>(summary.repairsFailed),
            static_cast<unsigned long long>(g_pzf13IdentityOverflow),
            static_cast<unsigned long long>(summary.consumers),
            static_cast<unsigned long long>(summary.consumerLogs),
            static_cast<unsigned long long>(g_pzf13ProducerLogLines),
            static_cast<unsigned long long>(g_pzf13ModelLogLines),
            ::MobilePZ::PZF13::kMaxRecords, kPZF13MaxIdentityKeys,
            static_cast<unsigned long long>(kPZF13MaxProducerLogLines),
            static_cast<unsigned long long>(kPZF13MaxModelLogLines));
}

static void PZF13OnPresentBoundary() {
    if (g_pzf3PresentSerial == 1 || g_pzf3PresentSerial == 60 ||
        g_pzf3PresentSerial == 300 || g_pzf3PresentSerial == 600 ||
        g_pzf3PresentSerial == 1800 || g_pzf3PresentSerial == 3600) {
        PZF13LogSummary("present_milestone");
    }
}

static void PZF13OnContextReady(Uint generation) {
    g_pzf13IdentityKeys = {};
    g_pzf13IdentityOverflow = 0;
    g_pzf13CandidateWrites = 0;
    g_pzf13ProducerLogLines = 0;
    g_pzf13ModelLogLines = 0;
    ::MobilePZ::PZF13::OnContextReady(generation);
    MGLOG_I("MOBILEPZ_PZF13_CONTEXT_READY generation=%u lineage_reset=1 identity_keys_reset=1 "
            "matching_path_mutation=0 mismatch_path_mutation=fbo_attachment_resync error_drain=0",
            generation);
}

static void PZF13OnContextDestroyed() {
    PZF13LogSummary("context_destroyed");
    MGLOG_I("MOBILEPZ_PZF13_CONTEXT_NOT_READY generation=%u rendering_output_mutation=0 error_drain=0",
            g_backendContextGeneration);
}
