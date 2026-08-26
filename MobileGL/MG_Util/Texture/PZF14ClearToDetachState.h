// MobilePZ PZF14 - bounded clear-to-detach raw-draw correlation.
//
// PZF13 proved that sampled NULL model textures were attached to complete FBOs,
// cleared, never seen by the PZF3-classified draw probe, and detached. PZF14
// opens a window at that clear and observes every native GLES draw regardless of
// program classification. It records only frontend/backend mismatches and never
// invents content or redirects a draw to a different frontend FBO.
#pragma once

#include <Includes.h>
#include <atomic>
#include <mutex>

namespace MobilePZ::PZF14 {

using MobileGL::Bool;
using MobileGL::Int;
using MobileGL::Uint;
using MobileGL::Uint64;
using MobileGL::UnorderedMap;

constexpr MobileGL::SizeT kMaxWindows = 4096;
constexpr Uint64 kMaxRawStateQueries = 8192;
constexpr Uint64 kMaxRawLogLines = 512;
constexpr Uint64 kMaxSessionLogLines = 512;
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
constexpr Uint64 kMaxSubmissionLogLines = 512;
constexpr Uint64 kMaxSubmissionLogLinesPerWindow = 8;

enum class SubmissionEvent : Uint {
    PushAttrib,
    PopAttrib,
    Begin,
    Vertex,
    End,
    EmptyEnd,
    ImmediateAttempt,
    ImmediateResourceFailure,
    ImmediateDispatch,
    FrontendDraw,
    ClientArrayDispatch,
    Color,
    TexCoord,
    TexEnv,
};

inline const char* SubmissionEventName(SubmissionEvent event) {
    switch (event) {
    case SubmissionEvent::PushAttrib: return "PUSH_ATTRIB";
    case SubmissionEvent::PopAttrib: return "POP_ATTRIB";
    case SubmissionEvent::Begin: return "BEGIN";
    case SubmissionEvent::Vertex: return "VERTEX";
    case SubmissionEvent::End: return "END";
    case SubmissionEvent::EmptyEnd: return "EMPTY_END";
    case SubmissionEvent::ImmediateAttempt: return "IMMEDIATE_ATTEMPT";
    case SubmissionEvent::ImmediateResourceFailure: return "IMMEDIATE_RESOURCE_FAILURE";
    case SubmissionEvent::ImmediateDispatch: return "IMMEDIATE_DISPATCH";
    case SubmissionEvent::FrontendDraw: return "FRONTEND_DRAW";
    case SubmissionEvent::ClientArrayDispatch: return "CLIENT_ARRAY_DISPATCH";
    case SubmissionEvent::Color: return "COLOR";
    case SubmissionEvent::TexCoord: return "TEXCOORD";
    case SubmissionEvent::TexEnv: return "TEXENV";
    }
    return "UNKNOWN";
}
#endif

struct Window {
    Uint generation = 0;
    Uint64 lifetime = 0;
    Uint frontTexture = 0;
    Uint frontFbo = 0;
    Int attachment = -1;
    Uint64 clientSequence = 0;
    Uint64 openSequence = 0;
    Uint64 lastSequence = 0;
    Uint64 closeSequence = 0;
    Uint64 clearWrites = 0;
    Uint64 nonDrawWrites = 0;
    Uint64 rawDrawsExpectedFbo = 0;
    Uint64 rawDrawsOtherFbo = 0;
    Uint64 driverStateQueries = 0;
    Uint64 backendFboMismatches = 0;
    Uint64 backendFboRepairs = 0;
    Uint64 programMismatches = 0;
    Uint64 programRepairs = 0;
    Uint64 rawLogLines = 0;
    Uint lastFrontFbo = 0;
    Uint lastExpectedBackendFbo = 0;
    Int lastDriverFboBefore = -1;
    Int lastDriverFboAfter = -1;
    Uint lastFrontProgram = 0;
    Uint lastExpectedBackendProgram = 0;
    Int lastDriverProgramBefore = -1;
    Int lastDriverProgramAfter = -1;
    Uint64 lastThreadHash = 0;
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
    Uint64 submissionEvents = 0;
    Uint64 legacyPushAttribCalls = 0;
    Uint64 legacyPopAttribCalls = 0;
    Uint64 legacyBeginCalls = 0;
    Uint64 legacyVertexCalls = 0;
    Uint64 legacyEndCalls = 0;
    Uint64 legacyEmptyEnds = 0;
    Uint64 immediateAttempts = 0;
    Uint64 immediateResourceFailures = 0;
    Uint64 immediateDispatches = 0;
    Uint64 frontendDrawCalls = 0;
    Uint64 clientArrayDispatches = 0;
    Uint64 legacyColorCalls = 0;
    Uint64 legacyTexCoordCalls = 0;
    Uint64 legacyTexEnvCalls = 0;
    Uint64 submissionLogLines = 0;
    Uint64 firstSubmissionSequence = 0;
    Uint64 lastSubmissionSequence = 0;
    Uint64 lastSubmissionThreadHash = 0;
    Uint lastSubmissionEvent = 0;
    Uint lastSubmissionFrontFbo = 0;
    Uint lastSubmissionMode = 0;
    Int lastSubmissionCount = 0;
#endif
    Bool active = false;
};

struct Summary {
    Uint generation = 0;
    Uint64 eventSequence = 0;
    Uint64 windowsOpened = 0;
    Uint64 windowsClosed = 0;
    Uint64 windowOverflow = 0;
    Uint64 supersededWindows = 0;
    Uint64 clearWrites = 0;
    Uint64 nonDrawWrites = 0;
    Uint64 rawDrawsDuringWindows = 0;
    Uint64 rawDrawsExpectedFbo = 0;
    Uint64 rawDrawsOtherFbo = 0;
    Uint64 stateQueries = 0;
    Uint64 stateQueryOverflow = 0;
    Uint64 backendFboMismatches = 0;
    Uint64 backendFboRepairs = 0;
    Uint64 programMismatches = 0;
    Uint64 programRepairs = 0;
    Uint64 clearOnlySessions = 0;
    Uint64 expectedFboSessions = 0;
    Uint64 otherFboSessions = 0;
    Uint64 repairedFboSessions = 0;
    Uint64 repairedProgramSessions = 0;
    Uint64 nonDrawPopulationSessions = 0;
    Uint64 rawLogLines = 0;
    Uint64 sessionLogLines = 0;
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
    Uint64 submissionEvents = 0;
    Uint64 submissionEventsExpectedFbo = 0;
    Uint64 submissionEventsOtherFbo = 0;
    Uint64 legacyPushAttribCalls = 0;
    Uint64 legacyPopAttribCalls = 0;
    Uint64 legacyBeginCalls = 0;
    Uint64 legacyVertexCalls = 0;
    Uint64 legacyEndCalls = 0;
    Uint64 legacyEmptyEnds = 0;
    Uint64 immediateAttempts = 0;
    Uint64 immediateResourceFailures = 0;
    Uint64 immediateDispatches = 0;
    Uint64 frontendDrawCalls = 0;
    Uint64 clientArrayDispatches = 0;
    Uint64 legacyColorCalls = 0;
    Uint64 legacyTexCoordCalls = 0;
    Uint64 legacyTexEnvCalls = 0;
    Uint64 submissionLogLines = 0;
    Uint64 nativeDrawExpectedSessions = 0;
    Uint64 immediateDispatchNoNativeSessions = 0;
    Uint64 immediateResourceFailureSessions = 0;
    Uint64 immediateAttemptNoDispatchSessions = 0;
    Uint64 frontendDrawNoNativeSubmitSessions = 0;
    Uint64 legacyEndEmptyBatchSessions = 0;
    Uint64 legacySequenceIncompleteSessions = 0;
    Uint64 legacyStateOnlyNoSubmitSessions = 0;
    Uint64 noLegacySubmissionSessions = 0;
#endif
};

struct RawDrawResult {
    Bool observed = false;
    Bool matchedExpectedFbo = false;
    Bool shouldLog = false;
    Window window{};
};

struct DetachResult {
    Bool observed = false;
    Bool shouldLog = false;
    Window window{};
};

#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
struct SubmissionEventResult {
    Bool observed = false;
    Bool matchedExpectedFbo = false;
    Bool shouldLog = false;
    Uint64 sequence = 0;
    Uint64 kindHits = 0;
    Uint64 activeWindows = 0;
    Window window{};
};
#endif

inline std::mutex g_mutex;
inline UnorderedMap<Uint64, Window> g_windows;
inline UnorderedMap<Uint, Uint64> g_activeByFbo;
inline std::atomic<Uint64> g_activeWindows{0};
inline Uint64 g_recentActiveLifetime = 0;
inline Summary g_summary;

inline Bool IsClearKind(Uint producerKind) {
    // PZF13 WriteKind: Clear=1, ClearBuffer=2, NamedClear=4.
    return producerKind == 1 || producerKind == 2 || producerKind == 4;
}

inline Bool IsNonDrawPopulationKind(Uint producerKind) {
    // PZF13 WriteKind: Blit=3, NamedBlit=5.
    return producerKind == 3 || producerKind == 5;
}

inline void ResetWindowEpoch(Window& window, Uint generation, Uint64 lifetime,
                             Uint frontTexture, Uint frontFbo, Int attachment,
                             Uint64 clientSequence) {
    const Uint64 rawLogLines = window.rawLogLines;
    window = {};
    window.generation = generation;
    window.lifetime = lifetime;
    window.frontTexture = frontTexture;
    window.frontFbo = frontFbo;
    window.attachment = attachment;
    window.clientSequence = clientSequence;
    window.rawLogLines = rawLogLines;
    window.active = true;
    window.openSequence = ++g_summary.eventSequence;
    window.lastSequence = window.openSequence;
}

inline void RecordProducerWrite(Uint generation, Uint64 lifetime, Uint frontTexture,
                                Uint frontFbo, Int attachment, Uint64 clientSequence,
                                Uint producerKind) {
    if (lifetime == 0 || (!IsClearKind(producerKind) && !IsNonDrawPopulationKind(producerKind))) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_windows.find(lifetime);
    if (found == g_windows.end()) {
        if (g_windows.size() >= kMaxWindows) {
            ++g_summary.windowOverflow;
            return;
        }
        found = g_windows.emplace(lifetime, Window{}).first;
    }
    Window& window = found->second;
    const Bool newEpoch = !window.active || window.clientSequence != clientSequence ||
                          window.frontFbo != frontFbo || window.attachment != attachment;
    if (newEpoch) {
        if (window.active) {
            ++g_summary.supersededWindows;
            auto active = g_activeByFbo.find(window.frontFbo);
            if (active != g_activeByFbo.end() && active->second == lifetime) g_activeByFbo.erase(active);
        } else {
            g_activeWindows.fetch_add(1, std::memory_order_release);
        }
        ResetWindowEpoch(window, generation, lifetime, frontTexture, frontFbo, attachment, clientSequence);
        ++g_summary.windowsOpened;
    }
    g_activeByFbo[frontFbo] = lifetime;
    g_recentActiveLifetime = lifetime;
    window.lastSequence = ++g_summary.eventSequence;
    if (IsClearKind(producerKind)) {
        ++window.clearWrites;
        ++g_summary.clearWrites;
    } else {
        ++window.nonDrawWrites;
        ++g_summary.nonDrawWrites;
    }
}

inline Bool HasActiveWindows() {
    return g_activeWindows.load(std::memory_order_acquire) != 0;
}

inline Bool TryReserveRawStateQuery() {
    if (!HasActiveWindows()) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_summary.stateQueries >= kMaxRawStateQueries) {
        ++g_summary.stateQueryOverflow;
        return false;
    }
    ++g_summary.stateQueries;
    return true;
}

inline Window* ResolveObservedWindowLocked(Uint currentFrontFbo, Bool drawTargetsFrontendFbo,
                                           Bool& matchedExpectedFbo) {
    auto byFbo = g_activeByFbo.find(currentFrontFbo);
    if (drawTargetsFrontendFbo && byFbo != g_activeByFbo.end()) {
        auto found = g_windows.find(byFbo->second);
        if (found != g_windows.end() && found->second.active && found->second.frontFbo == currentFrontFbo) {
            matchedExpectedFbo = true;
            return &found->second;
        }
    }
    matchedExpectedFbo = false;
    auto recent = g_windows.find(g_recentActiveLifetime);
    if (recent != g_windows.end() && recent->second.active) return &recent->second;
    for (auto& [lifetime, window] : g_windows) {
        if (!window.active) continue;
        g_recentActiveLifetime = lifetime;
        return &window;
    }
    return nullptr;
}

inline RawDrawResult RecordRawDraw(Uint currentFrontFbo, Uint expectedBackendFbo,
                                   Int driverFboBefore, Int driverFboAfter,
                                   Uint frontProgram, Uint expectedBackendProgram,
                                   Int driverProgramBefore, Int driverProgramAfter,
                                   Bool stateQueried, Bool backendFboMismatch,
                                   Bool backendFboRepaired, Bool programMismatch,
                                   Bool programRepaired, Bool drawTargetsFrontendFbo,
                                   Uint64 threadHash) {
    RawDrawResult result;
    if (!HasActiveWindows()) return result;
    std::lock_guard<std::mutex> lock(g_mutex);
    Bool matchedExpectedFbo = false;
    Window* window = ResolveObservedWindowLocked(currentFrontFbo, drawTargetsFrontendFbo,
                                                  matchedExpectedFbo);
    if (!window) return result;

    ++g_summary.rawDrawsDuringWindows;
    window->lastSequence = ++g_summary.eventSequence;
    if (matchedExpectedFbo) {
        ++window->rawDrawsExpectedFbo;
        ++g_summary.rawDrawsExpectedFbo;
    } else {
        ++window->rawDrawsOtherFbo;
        ++g_summary.rawDrawsOtherFbo;
    }
    if (stateQueried) ++window->driverStateQueries;
    if (backendFboMismatch) {
        ++window->backendFboMismatches;
        ++g_summary.backendFboMismatches;
    }
    if (backendFboRepaired) {
        ++window->backendFboRepairs;
        ++g_summary.backendFboRepairs;
    }
    if (programMismatch) {
        ++window->programMismatches;
        ++g_summary.programMismatches;
    }
    if (programRepaired) {
        ++window->programRepairs;
        ++g_summary.programRepairs;
    }
    window->lastFrontFbo = currentFrontFbo;
    window->lastExpectedBackendFbo = expectedBackendFbo;
    window->lastDriverFboBefore = driverFboBefore;
    window->lastDriverFboAfter = driverFboAfter;
    window->lastFrontProgram = frontProgram;
    window->lastExpectedBackendProgram = expectedBackendProgram;
    window->lastDriverProgramBefore = driverProgramBefore;
    window->lastDriverProgramAfter = driverProgramAfter;
    window->lastThreadHash = threadHash;

    result.observed = true;
    result.matchedExpectedFbo = matchedExpectedFbo;
    if (g_summary.rawLogLines < kMaxRawLogLines && window->rawLogLines < 2) {
        ++g_summary.rawLogLines;
        ++window->rawLogLines;
        result.shouldLog = true;
    }
    result.window = *window;
    return result;
}

#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
inline Uint64 IncrementSubmissionSummary(SubmissionEvent event) {
    switch (event) {
    case SubmissionEvent::PushAttrib: return ++g_summary.legacyPushAttribCalls;
    case SubmissionEvent::PopAttrib: return ++g_summary.legacyPopAttribCalls;
    case SubmissionEvent::Begin: return ++g_summary.legacyBeginCalls;
    case SubmissionEvent::Vertex: return ++g_summary.legacyVertexCalls;
    case SubmissionEvent::End: return ++g_summary.legacyEndCalls;
    case SubmissionEvent::EmptyEnd: return ++g_summary.legacyEmptyEnds;
    case SubmissionEvent::ImmediateAttempt: return ++g_summary.immediateAttempts;
    case SubmissionEvent::ImmediateResourceFailure: return ++g_summary.immediateResourceFailures;
    case SubmissionEvent::ImmediateDispatch: return ++g_summary.immediateDispatches;
    case SubmissionEvent::FrontendDraw: return ++g_summary.frontendDrawCalls;
    case SubmissionEvent::ClientArrayDispatch: return ++g_summary.clientArrayDispatches;
    case SubmissionEvent::Color: return ++g_summary.legacyColorCalls;
    case SubmissionEvent::TexCoord: return ++g_summary.legacyTexCoordCalls;
    case SubmissionEvent::TexEnv: return ++g_summary.legacyTexEnvCalls;
    }
    return 0;
}

inline void IncrementSubmissionWindow(Window& window, SubmissionEvent event) {
    switch (event) {
    case SubmissionEvent::PushAttrib: ++window.legacyPushAttribCalls; break;
    case SubmissionEvent::PopAttrib: ++window.legacyPopAttribCalls; break;
    case SubmissionEvent::Begin: ++window.legacyBeginCalls; break;
    case SubmissionEvent::Vertex: ++window.legacyVertexCalls; break;
    case SubmissionEvent::End: ++window.legacyEndCalls; break;
    case SubmissionEvent::EmptyEnd: ++window.legacyEmptyEnds; break;
    case SubmissionEvent::ImmediateAttempt: ++window.immediateAttempts; break;
    case SubmissionEvent::ImmediateResourceFailure: ++window.immediateResourceFailures; break;
    case SubmissionEvent::ImmediateDispatch: ++window.immediateDispatches; break;
    case SubmissionEvent::FrontendDraw: ++window.frontendDrawCalls; break;
    case SubmissionEvent::ClientArrayDispatch: ++window.clientArrayDispatches; break;
    case SubmissionEvent::Color: ++window.legacyColorCalls; break;
    case SubmissionEvent::TexCoord: ++window.legacyTexCoordCalls; break;
    case SubmissionEvent::TexEnv: ++window.legacyTexEnvCalls; break;
    }
}

inline Bool IsSubmissionLogEvent(SubmissionEvent event) {
    return event == SubmissionEvent::Begin || event == SubmissionEvent::End ||
           event == SubmissionEvent::EmptyEnd || event == SubmissionEvent::ImmediateAttempt ||
           event == SubmissionEvent::ImmediateResourceFailure ||
           event == SubmissionEvent::ImmediateDispatch || event == SubmissionEvent::FrontendDraw ||
           event == SubmissionEvent::ClientArrayDispatch;
}

inline Bool IsSubmissionMilestone(Uint64 hits) {
    return hits <= 16 || (hits != 0 && (hits & (hits - 1)) == 0);
}

inline SubmissionEventResult RecordSubmissionEvent(SubmissionEvent event, Uint currentFrontFbo,
                                                   GLenum mode, GLsizei count, Uint64 threadHash) {
    SubmissionEventResult result;
    std::lock_guard<std::mutex> lock(g_mutex);
    result.activeWindows = g_activeWindows.load(std::memory_order_acquire);
    result.sequence = ++g_summary.submissionEvents;
    result.kindHits = IncrementSubmissionSummary(event);

    Window* window = nullptr;
    auto byFbo = g_activeByFbo.find(currentFrontFbo);
    if (byFbo != g_activeByFbo.end()) {
        auto found = g_windows.find(byFbo->second);
        if (found != g_windows.end() && found->second.active &&
            found->second.frontFbo == currentFrontFbo) {
            window = &found->second;
        }
    }

    if (!window) {
        if (result.activeWindows != 0) ++g_summary.submissionEventsOtherFbo;
        if (IsSubmissionLogEvent(event) && IsSubmissionMilestone(result.kindHits) &&
            g_summary.submissionLogLines < kMaxSubmissionLogLines) {
            ++g_summary.submissionLogLines;
            result.shouldLog = true;
        }
        return result;
    }

    result.observed = true;
    result.matchedExpectedFbo = true;
    ++g_summary.submissionEventsExpectedFbo;
    ++window->submissionEvents;
    IncrementSubmissionWindow(*window, event);
    window->lastSequence = ++g_summary.eventSequence;
    if (window->firstSubmissionSequence == 0) window->firstSubmissionSequence = result.sequence;
    window->lastSubmissionSequence = result.sequence;
    window->lastSubmissionThreadHash = threadHash;
    window->lastSubmissionEvent = static_cast<Uint>(event);
    window->lastSubmissionFrontFbo = currentFrontFbo;
    window->lastSubmissionMode = mode;
    window->lastSubmissionCount = count;
    if (IsSubmissionLogEvent(event) && g_summary.submissionLogLines < kMaxSubmissionLogLines &&
        window->submissionLogLines < kMaxSubmissionLogLinesPerWindow) {
        ++g_summary.submissionLogLines;
        ++window->submissionLogLines;
        result.shouldLog = true;
    }
    result.window = *window;
    return result;
}

inline const char* SubmissionClassificationName(const Window& window) {
    if (window.rawDrawsExpectedFbo != 0) return "NATIVE_DRAW_TO_EXPECTED_FBO";
    if (window.immediateDispatches != 0) return "IMMEDIATE_DISPATCH_NO_NATIVE_DRAW";
    if (window.immediateResourceFailures != 0) return "IMMEDIATE_RESOURCE_FAILURE";
    if (window.legacyEmptyEnds != 0) return "LEGACY_END_EMPTY_BATCH";
    if (window.immediateAttempts != 0) return "IMMEDIATE_ATTEMPT_NO_DISPATCH";
    if (window.frontendDrawCalls != 0) return "FRONTEND_DRAW_NO_NATIVE_SUBMIT";
    if (window.legacyBeginCalls != 0 || window.legacyVertexCalls != 0 || window.legacyEndCalls != 0)
        return "LEGACY_SEQUENCE_INCOMPLETE";
    if (window.submissionEvents != 0) return "LEGACY_STATE_ONLY_NO_SUBMIT";
    return "NO_LEGACY_SUBMISSION_INSIDE_WINDOW";
}
#endif

inline const char* ClassificationName(const Window& window) {
    if (window.backendFboRepairs != 0) return "BACKEND_FBO_SYNC_REPAIRED";
    if (window.programRepairs != 0) return "PROGRAM_SYNC_REPAIRED";
    if (window.rawDrawsExpectedFbo != 0) return "DRAW_TO_EXPECTED_FBO";
    if (window.nonDrawWrites != 0) return "NON_DRAW_POPULATION_OBSERVED";
    if (window.rawDrawsOtherFbo != 0) return "DRAW_TO_OTHER_FBO_WHILE_WINDOW_ACTIVE";
    return "CLEAR_ONLY_NO_DRAW_SUBMITTED";
}

inline DetachResult RecordDetachment(Uint64 lifetime, Uint frontFbo, Int attachment) {
    DetachResult result;
    if (lifetime == 0 || !HasActiveWindows()) return result;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_windows.find(lifetime);
    if (found == g_windows.end()) return result;
    Window& window = found->second;
    if (!window.active || window.frontFbo != frontFbo || window.attachment != attachment) return result;

    window.closeSequence = ++g_summary.eventSequence;
    window.lastSequence = window.closeSequence;
    window.active = false;
    ++g_summary.windowsClosed;
    if (window.backendFboRepairs != 0) ++g_summary.repairedFboSessions;
    else if (window.programRepairs != 0) ++g_summary.repairedProgramSessions;
    else if (window.rawDrawsExpectedFbo != 0) ++g_summary.expectedFboSessions;
    else if (window.nonDrawWrites != 0) ++g_summary.nonDrawPopulationSessions;
    else if (window.rawDrawsOtherFbo != 0) ++g_summary.otherFboSessions;
    else ++g_summary.clearOnlySessions;
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
    if (window.rawDrawsExpectedFbo != 0) ++g_summary.nativeDrawExpectedSessions;
    else if (window.immediateDispatches != 0) ++g_summary.immediateDispatchNoNativeSessions;
    else if (window.immediateResourceFailures != 0) ++g_summary.immediateResourceFailureSessions;
    else if (window.legacyEmptyEnds != 0) ++g_summary.legacyEndEmptyBatchSessions;
    else if (window.immediateAttempts != 0) ++g_summary.immediateAttemptNoDispatchSessions;
    else if (window.frontendDrawCalls != 0) ++g_summary.frontendDrawNoNativeSubmitSessions;
    else if (window.legacyBeginCalls != 0 || window.legacyVertexCalls != 0 ||
             window.legacyEndCalls != 0)
        ++g_summary.legacySequenceIncompleteSessions;
    else if (window.submissionEvents != 0) ++g_summary.legacyStateOnlyNoSubmitSessions;
    else ++g_summary.noLegacySubmissionSessions;
#endif

    auto active = g_activeByFbo.find(frontFbo);
    if (active != g_activeByFbo.end() && active->second == lifetime) g_activeByFbo.erase(active);
    g_activeWindows.fetch_sub(1, std::memory_order_release);
    if (g_recentActiveLifetime == lifetime) {
        g_recentActiveLifetime = 0;
        for (const auto& [candidateLifetime, candidate] : g_windows) {
            if (!candidate.active) continue;
            g_recentActiveLifetime = candidateLifetime;
            break;
        }
    }
    result.observed = true;
    if (g_summary.sessionLogLines < kMaxSessionLogLines) {
        ++g_summary.sessionLogLines;
        result.shouldLog = true;
    }
    result.window = window;
    return result;
}

inline Summary GetSummary() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_summary;
}

inline void OnContextReady(Uint generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_windows.clear();
    g_activeByFbo.clear();
    g_recentActiveLifetime = 0;
    g_activeWindows.store(0, std::memory_order_release);
    g_summary = {};
    g_summary.generation = generation;
}

} // namespace MobilePZ::PZF14
