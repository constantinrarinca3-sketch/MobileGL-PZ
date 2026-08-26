// MobilePZ PZF12 - exact producer/attachment lineage for the PZF11 CPU_ZERO_GPU_ZERO
// model textures. Included by PZF3CrossFamilyTrace.inc inside its anonymous namespace.
//
// The candidate set is deliberately narrow: level-0 2D RGBA8 colour draw targets no
// larger than 512x512 whose latest PZF9 client event is TexImage2D(NULL). The first
// draw for each (texture lifetime, frontend FBO, attachment, client allocation,
// backend texture name) checks the actual ES attachment name. A mismatch is repaired
// by invalidating and replaying that FBO's existing frontend state; a match performs
// no rendering mutation.

constexpr SizeT kPZF12MaxTargetStates = 256;
constexpr Uint64 kPZF12MaxLogLines = 256;
constexpr Int kPZF12MaxDimension = 512;

struct PZF12TargetState {
    Bool used = false;
    Uint64 lifetime = 0;
    Uint frontendFbo = 0;
    FramebufferAttachmentType frontendAttachment = FramebufferAttachmentType::None;
    Uint64 clientSequence = 0;
    Uint expectedNative = 0;
};

struct PZF12Summary {
    Uint generation = 0;
    Uint64 drawsScanned = 0;
    Uint64 candidateDraws = 0;
    Uint64 targetStates = 0;
    Uint64 targetStateOverflow = 0;
    Uint64 missingBackendFbo = 0;
    Uint64 missingBackendTexture = 0;
    Uint64 attachmentQueries = 0;
    Uint64 identityMatches = 0;
    Uint64 identityMismatches = 0;
    Uint64 repairsSucceeded = 0;
    Uint64 repairsFailed = 0;
    Uint64 incompleteBefore = 0;
    Uint64 incompleteAfter = 0;
    Uint64 logLines = 0;
};

static Array<PZF12TargetState, kPZF12MaxTargetStates> g_pzf12Targets{};
static PZF12Summary g_pzf12Summary;

static PZF12TargetState* PZF12FindOrReserve(Uint64 lifetime, Uint frontendFbo,
                                            FramebufferAttachmentType frontendAttachment,
                                            Uint64 clientSequence, Uint expectedNative) {
    for (auto& state : g_pzf12Targets) {
        if (!state.used) continue;
        if (state.lifetime == lifetime && state.frontendFbo == frontendFbo &&
            state.frontendAttachment == frontendAttachment &&
            state.clientSequence == clientSequence && state.expectedNative == expectedNative) {
            return nullptr; // This exact producer identity was already checked.
        }
    }
    for (auto& state : g_pzf12Targets) {
        if (state.used) continue;
        state.used = true;
        state.lifetime = lifetime;
        state.frontendFbo = frontendFbo;
        state.frontendAttachment = frontendAttachment;
        state.clientSequence = clientSequence;
        state.expectedNative = expectedNative;
        ++g_pzf12Summary.targetStates;
        return &state;
    }
    ++g_pzf12Summary.targetStateOverflow;
    return nullptr;
}

static Bool PZF12CandidateTexture(
    const SharedPtr<MG_State::GLState::ITextureObject>& texture,
    const MG_State::GLState::FramebufferAttachmentObject& attachment,
    ::MobilePZ::PZF9::Record& upload) {
    if (!texture || texture->GetTarget() != TextureTarget::Texture2D ||
        texture->GetFormat() != TextureInternalFormat::RGBA8 || attachment.IsLayered() ||
        attachment.GetTextureLevel() != 0 ||
        attachment.GetTextureUploadTarget() != TextureUploadTarget::Texture2D) {
        return false;
    }
    const auto size = texture->GetBaseSize();
    if (size.x() <= 0 || size.y() <= 0 || size.x() > kPZF12MaxDimension ||
        size.y() > kPZF12MaxDimension) {
        return false;
    }
    if (!::MobilePZ::PZF9::PeekRecord(texture->GetLifetimeId(), upload)) return false;
    return upload.lastKind == ::MobilePZ::PZF9::UploadKind::NullImage2D &&
           upload.level == 0 && upload.width == size.x() && upload.height == size.y();
}

static void PZF12BeforeDraw(Uint64 serial, const PZF3ProgramRecord& program) {
    ++g_pzf12Summary.drawsScanned;
    if (!MG_State::pGLContext || !g_GLESFuncs.glGetFramebufferAttachmentParameteriv ||
        !g_GLESFuncs.glCheckFramebufferStatus) {
        return;
    }

    const auto& drawSlot = MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw);
    const auto& frontendFbo = drawSlot.GetBoundObject();
    if (!frontendFbo || frontendFbo->IsDefaultFramebuffer()) return;

    auto* backendFboSlot = FramebufferImpl::g_backendFramebufferObjects.Find(frontendFbo.get());
    if (!backendFboSlot || !*backendFboSlot) {
        ++g_pzf12Summary.missingBackendFbo;
        return;
    }
    const auto& backendFbo = *backendFboSlot;

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
        ++g_pzf12Summary.candidateDraws;

        auto* backendTextureSlot = TextureImpl::g_backendTextureObjects.Find(texture.get());
        if (!backendTextureSlot || !*backendTextureSlot) {
            ++g_pzf12Summary.missingBackendTexture;
            continue;
        }
        const Uint expectedNative = (*backendTextureSlot)->GetBackendTextureId();
        if (!PZF12FindOrReserve(texture->GetLifetimeId(), frontendFbo->GetExternalIndex(),
                                frontendAttachment, upload.clientSequence, expectedNative)) {
            continue;
        }

        const GLenum backendAttachment = backendFbo->GetBackendAttachmentType(frontendAttachment);
        const Uint expectedFbo = backendFbo->GetBackendFramebufferId();
        Uint boundFbo = FramebufferImpl::CurrentFramebufferBinding(FramebufferTarget::Draw);
        if (boundFbo != expectedFbo) {
            // PrepareForDraw should already have bound this FBO. Restoring the intended
            // frontend draw target is safer than querying or drawing into an unrelated one.
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
        ++g_pzf12Summary.attachmentQueries;
        if (statusBefore != GL_FRAMEBUFFER_COMPLETE) ++g_pzf12Summary.incompleteBefore;

        const Bool mismatch = objectTypeBefore != GL_TEXTURE ||
                              objectNameBefore != static_cast<GLint>(expectedNative);
        Bool repaired = false;
        GLint objectTypeAfter = objectTypeBefore;
        GLint objectNameAfter = objectNameBefore;
        GLenum statusAfter = statusBefore;
        if (mismatch) {
            ++g_pzf12Summary.identityMismatches;
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
            if (repaired) ++g_pzf12Summary.repairsSucceeded;
            else ++g_pzf12Summary.repairsFailed;
            if (statusAfter != GL_FRAMEBUFFER_COMPLETE) ++g_pzf12Summary.incompleteAfter;
        } else {
            ++g_pzf12Summary.identityMatches;
        }

        if (g_pzf12Summary.logLines < kPZF12MaxLogLines) {
            ++g_pzf12Summary.logLines;
            const auto size = texture->GetBaseSize();
            MGLOG_I("MOBILEPZ_PZF12_NULL_RT generation=%u serial=%llu present=%llu family=%s "
                    "program=%u front_fbo=%u backend_fbo=%u bound_fbo=%u "
                    "front_attachment=%d backend_attachment=0x%x front_texture=%u "
                    "texture_lifetime=%llu native_expected=%u object_type_before=0x%x "
                    "native_before=%d fbo_status_before=0x%x mismatch=%d repaired=%d "
                    "object_type_after=0x%x native_after=%d fbo_status_after=0x%x "
                    "client_sequence=%llu null_image_calls=%llu size=%dx%d level=0 "
                    "matching_path_mutation=0 mismatch_path_mutation=%d error_drain=0",
                    g_backendContextGeneration, static_cast<unsigned long long>(serial),
                    static_cast<unsigned long long>(g_pzf3PresentSerial),
                    PZF3FamilyName(program.family), program.external,
                    frontendFbo->GetExternalIndex(), expectedFbo, boundFbo,
                    static_cast<Int>(frontendAttachment), backendAttachment,
                    texture->GetExternalIndex(),
                    static_cast<unsigned long long>(texture->GetLifetimeId()), expectedNative,
                    objectTypeBefore, objectNameBefore, statusBefore, mismatch ? 1 : 0,
                    repaired ? 1 : 0, objectTypeAfter, objectNameAfter, statusAfter,
                    static_cast<unsigned long long>(upload.clientSequence),
                    static_cast<unsigned long long>(upload.nullImageCalls),
                    size.x(), size.y(), mismatch ? 1 : 0);
        }
    }
}

static void PZF12LogSummary(const char* reason) {
    MGLOG_I("MOBILEPZ_PZF12_SUMMARY reason=%s generation=%u presents=%llu "
            "draws_scanned=%llu candidate_draws=%llu target_states=%llu "
            "target_state_overflow=%llu missing_backend_fbo=%llu missing_backend_texture=%llu "
            "attachment_queries=%llu identity_matches=%llu identity_mismatches=%llu "
            "repairs_succeeded=%llu repairs_failed=%llu incomplete_before=%llu "
            "incomplete_after=%llu log_lines=%llu max_target_states=%zu max_log_lines=%llu "
            "candidate_size_max=%d matching_path_mutation=0 "
            "mismatch_path_mutation=fbo_attachment_resync error_drain=0",
            reason ? reason : "unknown", g_pzf12Summary.generation,
            static_cast<unsigned long long>(g_pzf3PresentSerial),
            static_cast<unsigned long long>(g_pzf12Summary.drawsScanned),
            static_cast<unsigned long long>(g_pzf12Summary.candidateDraws),
            static_cast<unsigned long long>(g_pzf12Summary.targetStates),
            static_cast<unsigned long long>(g_pzf12Summary.targetStateOverflow),
            static_cast<unsigned long long>(g_pzf12Summary.missingBackendFbo),
            static_cast<unsigned long long>(g_pzf12Summary.missingBackendTexture),
            static_cast<unsigned long long>(g_pzf12Summary.attachmentQueries),
            static_cast<unsigned long long>(g_pzf12Summary.identityMatches),
            static_cast<unsigned long long>(g_pzf12Summary.identityMismatches),
            static_cast<unsigned long long>(g_pzf12Summary.repairsSucceeded),
            static_cast<unsigned long long>(g_pzf12Summary.repairsFailed),
            static_cast<unsigned long long>(g_pzf12Summary.incompleteBefore),
            static_cast<unsigned long long>(g_pzf12Summary.incompleteAfter),
            static_cast<unsigned long long>(g_pzf12Summary.logLines),
            kPZF12MaxTargetStates, static_cast<unsigned long long>(kPZF12MaxLogLines),
            kPZF12MaxDimension);
}

static void PZF12OnPresentBoundary() {
    if (g_pzf3PresentSerial == 1 || g_pzf3PresentSerial == 60 ||
        g_pzf3PresentSerial == 300 || g_pzf3PresentSerial == 600 ||
        g_pzf3PresentSerial == 1800 || g_pzf3PresentSerial == 3600) {
        PZF12LogSummary("present_milestone");
    }
}

static void PZF12OnContextReady(Uint generation) {
    g_pzf12Targets = {};
    g_pzf12Summary = {};
    g_pzf12Summary.generation = generation;
    MGLOG_I("MOBILEPZ_PZF12_CONTEXT_READY generation=%u target_states_reset=1 "
            "matching_path_mutation=0 mismatch_path_mutation=fbo_attachment_resync "
            "error_drain=0", generation);
}

static void PZF12OnContextDestroyed() {
    PZF12LogSummary("context_destroyed");
    MGLOG_I("MOBILEPZ_PZF12_CONTEXT_NOT_READY generation=%u matching_path_mutation=0 "
            "error_drain=0", g_pzf12Summary.generation);
}
