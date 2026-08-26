// MobilePZ PZF14 - raw native draw route inside clear-to-detach windows.
// Included inside DirectGLES' diagnostic anonymous namespace after PZF13.

static Uint64 PZF14ThreadHash() {
    return static_cast<Uint64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

static void PZF14InspectRawDraw(const char* entry, GLenum mode, GLsizei count,
                                GLsizei instances, Bool indexed, GLsizei subdraws,
                                Bool repairEligible) {
    if (!MG_State::pGLContext || !::MobilePZ::PZF14::HasActiveWindows()) return;

    const auto& drawSlot = MG_State::pGLContext->GetFramebufferBindingSlot(FramebufferTarget::Draw);
    const auto& frontendFbo = drawSlot.GetBoundObject();
    const Uint frontFbo = frontendFbo ? frontendFbo->GetExternalIndex() : 0;
    FramebufferImpl::BackendFramebufferObject* backendFbo = nullptr;
    Uint expectedBackendFbo = 0;
    Bool expectedBackendFboKnown = frontendFbo == nullptr || frontendFbo->IsDefaultFramebuffer();
    if (frontendFbo && !frontendFbo->IsDefaultFramebuffer()) {
        auto* backendFboSlot = FramebufferImpl::g_backendFramebufferObjects.Find(frontendFbo.get());
        if (backendFboSlot && *backendFboSlot) {
            backendFbo = backendFboSlot->get();
            expectedBackendFbo = backendFbo->GetBackendFramebufferId();
            expectedBackendFboKnown = true;
        }
    }

    const auto& frontendProgram = MG_State::pGLContext->GetProgramForDraw();
    const Uint frontProgram = frontendProgram ? frontendProgram->GetExternalIndex() : 0;
    auto* backendProgram = GetCurrentBackendProgram();
    const Uint expectedBackendProgram = backendProgram ? backendProgram->GetBackendProgramId() : 0;
    const Bool expectedBackendProgramKnown = backendProgram != nullptr;

    const Bool stateQueried = ::MobilePZ::PZF14::TryReserveRawStateQuery() &&
                              g_GLESFuncs.glGetIntegerv != nullptr;
    GLint driverFboBefore = -1;
    GLint driverFboAfter = -1;
    GLint driverProgramBefore = -1;
    GLint driverProgramAfter = -1;
    Bool backendFboMismatch = false;
    Bool backendFboRepaired = false;
    Bool programMismatch = false;
    Bool programRepaired = false;

    if (stateQueried) {
        g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &driverFboBefore);
        g_GLESFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &driverProgramBefore);
        driverFboAfter = driverFboBefore;
        driverProgramAfter = driverProgramBefore;

        backendFboMismatch = repairEligible && expectedBackendFboKnown &&
                             driverFboBefore != static_cast<GLint>(expectedBackendFbo);
        if (backendFboMismatch) {
            if (backendFbo) backendFbo->Bind(FramebufferTarget::Draw);
            else FramebufferImpl::BindFramebufferId(GL_DRAW_FRAMEBUFFER, 0);
            g_GLESFuncs.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &driverFboAfter);
            backendFboRepaired = driverFboAfter == static_cast<GLint>(expectedBackendFbo);
        }

        programMismatch = repairEligible && expectedBackendProgramKnown &&
                          driverProgramBefore != static_cast<GLint>(expectedBackendProgram);
        if (programMismatch && expectedBackendProgram != 0 && g_GLESFuncs.glUseProgram) {
            g_GLESFuncs.glUseProgram(expectedBackendProgram);
            PrgramImpl::g_lastUsedBackendProgramId = expectedBackendProgram;
            g_GLESFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &driverProgramAfter);
            programRepaired = driverProgramAfter == static_cast<GLint>(expectedBackendProgram);
        }
    }

    const Bool drawTargetsFrontendFbo = repairEligible &&
        (!stateQueried || !expectedBackendFboKnown ||
         driverFboAfter == static_cast<GLint>(expectedBackendFbo));
    // PZF13's classified probe no longer owns this call in a PZF14 build. Record
    // it only when the native draw really targets the prepared frontend FBO;
    // internal conversion passes intentionally borrow another driver FBO/program.
    if (drawTargetsFrontendFbo) {
        PZF13BeforeFramebufferWrite(entry, ::MobilePZ::PZF13::WriteKind::Draw);
    }

    const auto observation = ::MobilePZ::PZF14::RecordRawDraw(
        frontFbo, expectedBackendFbo, driverFboBefore, driverFboAfter,
        frontProgram, expectedBackendProgram, driverProgramBefore, driverProgramAfter,
        stateQueried, backendFboMismatch, backendFboRepaired, programMismatch,
        programRepaired, drawTargetsFrontendFbo, PZF14ThreadHash());
    if (!observation.observed || !observation.shouldLog) return;

    GLint viewport[4] = {-1, -1, -1, -1};
    GLint scissorBox[4] = {-1, -1, -1, -1};
    GLboolean colorMask[4] = {GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE};
    GLint scissorEnabled = -1;
    GLint blendEnabled = -1;
    if (stateQueried) {
        g_GLESFuncs.glGetIntegerv(GL_VIEWPORT, viewport);
        g_GLESFuncs.glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
        if (g_GLESFuncs.glGetBooleanv) g_GLESFuncs.glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        if (g_GLESFuncs.glIsEnabled) {
            scissorEnabled = g_GLESFuncs.glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0;
            blendEnabled = g_GLESFuncs.glIsEnabled(GL_BLEND) ? 1 : 0;
        }
    }

    MGLOG_I("MOBILEPZ_PZF14_RAW_DRAW generation=%u sequence=%llu entry=%s mode=0x%x "
            "count=%d instances=%d indexed=%d subdraws=%d window_lifetime=%llu "
            "window_front_texture=%u window_fbo=%u window_attachment=%d client_sequence=%llu "
            "current_front_fbo=%u match_window_fbo=%d expected_backend_fbo=%u "
            "driver_fbo_before=%d driver_fbo_after=%d fbo_mismatch=%d fbo_repaired=%d "
            "front_program=%u expected_backend_program=%u driver_program_before=%d "
            "driver_program_after=%d program_mismatch=%d program_repaired=%d "
            "viewport=%d,%d,%d,%d scissor_enabled=%d scissor_box=%d,%d,%d,%d "
            "color_mask=%d%d%d%d blend_enabled=%d thread_hash=%llu state_query=%d "
            "repair_eligible=%d rendering_output_mutation=%d additional_gl_calls=bounded error_drain=0",
            observation.window.generation,
            static_cast<unsigned long long>(observation.window.lastSequence),
            entry ? entry : "unknown", mode, count, instances, indexed ? 1 : 0, subdraws,
            static_cast<unsigned long long>(observation.window.lifetime),
            observation.window.frontTexture, observation.window.frontFbo,
            observation.window.attachment,
            static_cast<unsigned long long>(observation.window.clientSequence), frontFbo,
            observation.matchedExpectedFbo ? 1 : 0, expectedBackendFbo,
            driverFboBefore, driverFboAfter, backendFboMismatch ? 1 : 0,
            backendFboRepaired ? 1 : 0, frontProgram, expectedBackendProgram,
            driverProgramBefore, driverProgramAfter, programMismatch ? 1 : 0,
            programRepaired ? 1 : 0, viewport[0], viewport[1], viewport[2], viewport[3],
            scissorEnabled, scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3],
            colorMask[0] ? 1 : 0, colorMask[1] ? 1 : 0, colorMask[2] ? 1 : 0,
            colorMask[3] ? 1 : 0, blendEnabled,
            static_cast<unsigned long long>(observation.window.lastThreadHash),
            stateQueried ? 1 : 0, repairEligible ? 1 : 0,
            (backendFboRepaired || programRepaired) ? 1 : 0);
}

static void PZF14LogSummary(const char* reason) {
    const auto summary = ::MobilePZ::PZF14::GetSummary();
    MGLOG_I("MOBILEPZ_PZF14_SUMMARY reason=%s generation=%u sequence=%llu windows_opened=%llu "
            "windows_closed=%llu windows_active=%llu window_overflow=%llu superseded_windows=%llu "
            "clear_writes=%llu non_draw_writes=%llu raw_draws_during_windows=%llu "
            "raw_draws_expected_fbo=%llu raw_draws_other_fbo=%llu state_queries=%llu "
            "state_query_overflow=%llu backend_fbo_mismatches=%llu backend_fbo_repairs=%llu "
            "program_mismatches=%llu program_repairs=%llu clear_only_sessions=%llu "
            "expected_fbo_sessions=%llu other_fbo_sessions=%llu repaired_fbo_sessions=%llu "
            "repaired_program_sessions=%llu non_draw_population_sessions=%llu raw_log_lines=%llu "
            "session_log_lines=%llu max_windows=%zu max_state_queries=%llu "
            "max_raw_log_lines=%llu max_session_log_lines=%llu matching_path_mutation=0 "
            "mismatch_path_mutation=backend_binding_resync error_drain=0",
            reason ? reason : "unknown", summary.generation,
            static_cast<unsigned long long>(summary.eventSequence),
            static_cast<unsigned long long>(summary.windowsOpened),
            static_cast<unsigned long long>(summary.windowsClosed),
            static_cast<unsigned long long>(::MobilePZ::PZF14::g_activeWindows.load(std::memory_order_acquire)),
            static_cast<unsigned long long>(summary.windowOverflow),
            static_cast<unsigned long long>(summary.supersededWindows),
            static_cast<unsigned long long>(summary.clearWrites),
            static_cast<unsigned long long>(summary.nonDrawWrites),
            static_cast<unsigned long long>(summary.rawDrawsDuringWindows),
            static_cast<unsigned long long>(summary.rawDrawsExpectedFbo),
            static_cast<unsigned long long>(summary.rawDrawsOtherFbo),
            static_cast<unsigned long long>(summary.stateQueries),
            static_cast<unsigned long long>(summary.stateQueryOverflow),
            static_cast<unsigned long long>(summary.backendFboMismatches),
            static_cast<unsigned long long>(summary.backendFboRepairs),
            static_cast<unsigned long long>(summary.programMismatches),
            static_cast<unsigned long long>(summary.programRepairs),
            static_cast<unsigned long long>(summary.clearOnlySessions),
            static_cast<unsigned long long>(summary.expectedFboSessions),
            static_cast<unsigned long long>(summary.otherFboSessions),
            static_cast<unsigned long long>(summary.repairedFboSessions),
            static_cast<unsigned long long>(summary.repairedProgramSessions),
            static_cast<unsigned long long>(summary.nonDrawPopulationSessions),
            static_cast<unsigned long long>(summary.rawLogLines),
            static_cast<unsigned long long>(summary.sessionLogLines),
            ::MobilePZ::PZF14::kMaxWindows,
            static_cast<unsigned long long>(::MobilePZ::PZF14::kMaxRawStateQueries),
            static_cast<unsigned long long>(::MobilePZ::PZF14::kMaxRawLogLines),
            static_cast<unsigned long long>(::MobilePZ::PZF14::kMaxSessionLogLines));
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
    MGLOG_I("MOBILEPZ_PZF15_SUMMARY reason=%s generation=%u submission_events=%llu "
            "events_expected_fbo=%llu events_other_fbo=%llu push_attrib=%llu pop_attrib=%llu "
            "begin=%llu vertices=%llu end=%llu empty_end=%llu immediate_attempts=%llu "
            "immediate_resource_failures=%llu immediate_dispatches=%llu frontend_draw_calls=%llu "
            "client_array_dispatches=%llu color=%llu texcoord=%llu texenv=%llu "
            "native_draw_expected_sessions=%llu immediate_dispatch_no_native_sessions=%llu "
            "immediate_resource_failure_sessions=%llu immediate_attempt_no_dispatch_sessions=%llu "
            "frontend_draw_no_native_submit_sessions=%llu legacy_end_empty_batch_sessions=%llu "
            "legacy_sequence_incomplete_sessions=%llu legacy_state_only_no_submit_sessions=%llu "
            "no_legacy_submission_sessions=%llu "
            "event_log_lines=%llu max_event_log_lines=%llu max_events_per_window=%llu "
            "rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
            reason ? reason : "unknown", summary.generation,
            static_cast<unsigned long long>(summary.submissionEvents),
            static_cast<unsigned long long>(summary.submissionEventsExpectedFbo),
            static_cast<unsigned long long>(summary.submissionEventsOtherFbo),
            static_cast<unsigned long long>(summary.legacyPushAttribCalls),
            static_cast<unsigned long long>(summary.legacyPopAttribCalls),
            static_cast<unsigned long long>(summary.legacyBeginCalls),
            static_cast<unsigned long long>(summary.legacyVertexCalls),
            static_cast<unsigned long long>(summary.legacyEndCalls),
            static_cast<unsigned long long>(summary.legacyEmptyEnds),
            static_cast<unsigned long long>(summary.immediateAttempts),
            static_cast<unsigned long long>(summary.immediateResourceFailures),
            static_cast<unsigned long long>(summary.immediateDispatches),
            static_cast<unsigned long long>(summary.frontendDrawCalls),
            static_cast<unsigned long long>(summary.clientArrayDispatches),
            static_cast<unsigned long long>(summary.legacyColorCalls),
            static_cast<unsigned long long>(summary.legacyTexCoordCalls),
            static_cast<unsigned long long>(summary.legacyTexEnvCalls),
            static_cast<unsigned long long>(summary.nativeDrawExpectedSessions),
            static_cast<unsigned long long>(summary.immediateDispatchNoNativeSessions),
            static_cast<unsigned long long>(summary.immediateResourceFailureSessions),
            static_cast<unsigned long long>(summary.immediateAttemptNoDispatchSessions),
            static_cast<unsigned long long>(summary.frontendDrawNoNativeSubmitSessions),
            static_cast<unsigned long long>(summary.legacyEndEmptyBatchSessions),
            static_cast<unsigned long long>(summary.legacySequenceIncompleteSessions),
            static_cast<unsigned long long>(summary.legacyStateOnlyNoSubmitSessions),
            static_cast<unsigned long long>(summary.noLegacySubmissionSessions),
            static_cast<unsigned long long>(summary.submissionLogLines),
            static_cast<unsigned long long>(::MobilePZ::PZF14::kMaxSubmissionLogLines),
            static_cast<unsigned long long>(::MobilePZ::PZF14::kMaxSubmissionLogLinesPerWindow));
#endif
}

static void PZF14OnPresentBoundary() {
    if (g_pzf3PresentSerial == 1 || g_pzf3PresentSerial == 60 ||
        g_pzf3PresentSerial == 300 || g_pzf3PresentSerial == 600 ||
        g_pzf3PresentSerial == 1800 || g_pzf3PresentSerial == 3600) {
        PZF14LogSummary("present_milestone");
    }
}

static void PZF14OnContextReady(Uint generation) {
    ::MobilePZ::PZF14::OnContextReady(generation);
    MGLOG_I("MOBILEPZ_PZF14_CONTEXT_READY generation=%u trace_reset=1 max_windows=%zu "
            "max_state_queries=%llu matching_path_mutation=0 "
            "mismatch_path_mutation=backend_binding_resync error_drain=0",
            generation, ::MobilePZ::PZF14::kMaxWindows,
            static_cast<unsigned long long>(::MobilePZ::PZF14::kMaxRawStateQueries));
}

static void PZF14OnContextDestroyed() {
    PZF14LogSummary("context_destroyed");
    MGLOG_I("MOBILEPZ_PZF14_CONTEXT_NOT_READY generation=%u rendering_output_mutation=0 error_drain=0",
            g_backendContextGeneration);
}
