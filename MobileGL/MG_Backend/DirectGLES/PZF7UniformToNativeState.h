// MobilePZ PZF7 - GL-independent bounded uniform-to-native correlation state.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace MobilePZ::PZF7 {

enum class Family : std::uint8_t {
    Other = 0,
    Skinned,
    StaticModel,
};

enum class MatrixRole : std::uint8_t {
    Other = 0,
    Mvp,
    Palette,
    Transform,
};

enum class MatrixRoute : std::uint8_t {
    Uniform = 0,
    ProgramUniform,
};

enum class MatrixOutcome : std::uint8_t {
    InvalidLocation = 0,
    RejectedOpaque,
    NoElement,
    Buffered,
    ShadowChanged,
    ShadowUnchanged,
};

enum class UboPath : std::uint8_t {
    None = 0,
    Ring,
    BufferSubData,
};

inline const char* FamilyName(Family family) {
    switch (family) {
    case Family::Skinned: return "SKINNED";
    case Family::StaticModel: return "STATIC_MODEL";
    default: return "OTHER";
    }
}

inline const char* MatrixRoleName(MatrixRole role) {
    switch (role) {
    case MatrixRole::Mvp: return "MVP";
    case MatrixRole::Palette: return "PALETTE";
    case MatrixRole::Transform: return "TRANSFORM";
    default: return "OTHER";
    }
}

inline const char* MatrixRouteName(MatrixRoute route) {
    return route == MatrixRoute::ProgramUniform ? "program_uniform" : "uniform";
}

inline const char* MatrixOutcomeName(MatrixOutcome outcome) {
    switch (outcome) {
    case MatrixOutcome::InvalidLocation: return "invalid_location";
    case MatrixOutcome::RejectedOpaque: return "rejected_opaque";
    case MatrixOutcome::NoElement: return "no_element";
    case MatrixOutcome::Buffered: return "buffered_or_equal";
    case MatrixOutcome::ShadowChanged: return "shadow_changed";
    case MatrixOutcome::ShadowUnchanged: return "shadow_unchanged";
    default: return "unknown";
    }
}

inline const char* UboPathName(UboPath path) {
    switch (path) {
    case UboPath::Ring: return "ring";
    case UboPath::BufferSubData: return "buffer_sub_data";
    default: return "none";
    }
}

inline std::uint64_t HashBytes(const void* data, std::size_t size) {
    if (!data || size == 0) return 0;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

struct MatrixCallInput {
    Family family = Family::Other;
    MatrixRole role = MatrixRole::Other;
    MatrixRoute route = MatrixRoute::Uniform;
    MatrixOutcome outcome = MatrixOutcome::InvalidLocation;
    std::uint32_t program = 0;
    std::uint64_t lifetime = 0;
    std::int32_t location = -1;
    std::int32_t count = 0;
    std::uint32_t type = 0;
    std::uint32_t beforeVersion = 0;
    std::uint32_t afterVersion = 0;
    std::uint64_t effectiveInputHash = 0;
    const char* uniformName = nullptr;
    bool transpose = false;
    bool locationValid = false;
    bool opaque = false;
    bool typeMatch = false;
    bool spirvPendingBefore = false;
    bool spirvPendingAfter = false;
};

struct MatrixCall {
    bool valid = false;
    std::uint32_t contextGeneration = 0;
    std::uint64_t sequence = 0;
    Family family = Family::Other;
    MatrixRole role = MatrixRole::Other;
    MatrixRoute route = MatrixRoute::Uniform;
    MatrixOutcome outcome = MatrixOutcome::InvalidLocation;
    std::uint32_t program = 0;
    std::uint64_t lifetime = 0;
    std::int32_t location = -1;
    std::int32_t count = 0;
    std::uint32_t type = 0;
    std::uint32_t beforeVersion = 0;
    std::uint32_t afterVersion = 0;
    std::uint64_t effectiveInputHash = 0;
    std::array<char, 96> uniformName{};
    bool transpose = false;
    bool locationValid = false;
    bool opaque = false;
    bool typeMatch = false;
    bool spirvPendingBefore = false;
    bool spirvPendingAfter = false;
};

struct MatrixRecordResult {
    bool shouldLog = false;
    MatrixCall call{};
};

struct UboBindInput {
    std::uint32_t program = 0;
    std::uint64_t lifetime = 0;
    std::uint32_t contentVersion = 0;
    const void* frontendData = nullptr;
    std::size_t frontendSize = 0;
    UboPath path = UboPath::None;
    const void* submittedData = nullptr;
    std::size_t submittedSize = 0;
    bool submittedKnown = false;
    bool freshSubmission = false;
    std::uint32_t ringGeneration = 0;
    std::uint64_t frameSerial = 0;
    std::size_t ringOffset = 0;
};

struct UboBinding {
    bool valid = false;
    std::uint32_t contextGeneration = 0;
    std::uint64_t sequence = 0;
    std::uint32_t program = 0;
    std::uint64_t lifetime = 0;
    std::uint32_t frontendVersion = 0;
    std::size_t frontendSize = 0;
    std::uint64_t frontendHash = 0;
    UboPath path = UboPath::None;
    bool submittedKnown = false;
    bool freshSubmission = false;
    std::uint32_t submittedVersion = 0;
    std::size_t submittedSize = 0;
    std::uint64_t submittedHash = 0;
    std::uint32_t ringGeneration = 0;
    std::uint64_t frameSerial = 0;
    std::size_t ringOffset = 0;
};

struct CaptureDecision {
    bool selected = false;
    std::uint32_t capture = 0;
    MatrixCall latestAny{};
    MatrixCall latestMvp{};
    MatrixCall latestPalette{};
    MatrixCall latestTransform{};
    MatrixCall latestLocationZero{};
    UboBinding binding{};
};

struct Summary {
    bool contextReady = false;
    std::uint32_t contextGeneration = 0;
    std::uint64_t presents = 0;
    std::uint64_t matrixCalls = 0;
    std::uint64_t modelMatrixCalls = 0;
    std::uint64_t matrixLocationZero = 0;
    std::uint64_t matrixLocationZeroOpaque = 0;
    std::uint64_t modelMatrixLocationZeroOpaque = 0;
    std::uint64_t matrixMvp = 0;
    std::uint64_t matrixPalette = 0;
    std::uint64_t matrixTransform = 0;
    std::uint64_t outcomeInvalidLocation = 0;
    std::uint64_t outcomeRejectedOpaque = 0;
    std::uint64_t outcomeBuffered = 0;
    std::uint64_t outcomeShadowChanged = 0;
    std::uint64_t outcomeShadowUnchanged = 0;
    std::uint64_t matrixLogs = 0;
    std::uint64_t programOverflow = 0;
    std::uint64_t uboBindings = 0;
    std::uint64_t uboRingBindings = 0;
    std::uint64_t uboBufferSubDataBindings = 0;
    std::uint64_t uboFreshSubmissions = 0;
    std::uint64_t eligibleModelDraws = 0;
    std::uint64_t rejectSamePresent = 0;
    std::uint64_t rejectFamilyLimit = 0;
    std::uint64_t rejectGlobalLimit = 0;
    std::uint64_t captures = 0;
    std::uint64_t skinnedCaptures = 0;
    std::uint64_t staticCaptures = 0;
    std::uint64_t payloadMatch = 0;
    std::uint64_t payloadMismatch = 0;
    std::uint64_t payloadUnknown = 0;
    std::uint64_t payloadVersionMismatch = 0;
    std::uint64_t capturesWithMvpCall = 0;
    std::uint64_t capturesWithPaletteCall = 0;
    std::uint64_t capturesWithTransformCall = 0;
    std::uint64_t capturesWithOpaqueLocationZero = 0;
};

class TraceState {
public:
    static constexpr std::size_t kMaxPrograms = 128;
    static constexpr std::uint32_t kMaxCaptures = 16;
    static constexpr std::uint32_t kMaxCapturesPerFamily = 8;
    static constexpr std::uint64_t kMaxMatrixLogs = 192;
    static constexpr std::uint32_t kMaxMatrixLogsPerProgram = 16;

    bool OnContextReady(std::uint32_t generation) {
        std::lock_guard lock(m_mutex);
        if (m_summary.contextReady && m_summary.contextGeneration == generation) return false;
        const std::uint64_t readyEvents = m_contextReadyEvents + 1;
        const std::uint64_t destroyedEvents = m_contextDestroyedEvents;
        ResetContextLocked();
        m_contextReadyEvents = readyEvents;
        m_contextDestroyedEvents = destroyedEvents;
        m_summary.contextReady = true;
        m_summary.contextGeneration = generation;
        return true;
    }

    void OnContextDestroyed() {
        std::lock_guard lock(m_mutex);
        m_summary.contextReady = false;
        ++m_contextDestroyedEvents;
    }

    void OnPresentBoundary() {
        std::lock_guard lock(m_mutex);
        if (m_summary.contextReady) ++m_summary.presents;
    }

    MatrixRecordResult RecordMatrix(const MatrixCallInput& input) {
        std::lock_guard lock(m_mutex);
        MatrixRecordResult result;
        if (!m_summary.contextReady) return result;

        ++m_summary.matrixCalls;
        if (input.family == Family::Skinned || input.family == Family::StaticModel) {
            ++m_summary.modelMatrixCalls;
        }
        if (input.location == 0) {
            ++m_summary.matrixLocationZero;
            if (input.opaque) {
                ++m_summary.matrixLocationZeroOpaque;
                if (input.family == Family::Skinned || input.family == Family::StaticModel) {
                    ++m_summary.modelMatrixLocationZeroOpaque;
                }
            }
        }
        switch (input.role) {
        case MatrixRole::Mvp: ++m_summary.matrixMvp; break;
        case MatrixRole::Palette: ++m_summary.matrixPalette; break;
        case MatrixRole::Transform: ++m_summary.matrixTransform; break;
        default: break;
        }
        switch (input.outcome) {
        case MatrixOutcome::InvalidLocation: ++m_summary.outcomeInvalidLocation; break;
        case MatrixOutcome::RejectedOpaque: ++m_summary.outcomeRejectedOpaque; break;
        case MatrixOutcome::Buffered: ++m_summary.outcomeBuffered; break;
        case MatrixOutcome::ShadowChanged: ++m_summary.outcomeShadowChanged; break;
        case MatrixOutcome::ShadowUnchanged: ++m_summary.outcomeShadowUnchanged; break;
        default: break;
        }

        ProgramRecord* record = FindOrCreateLocked(input.program, input.lifetime);
        if (!record) return result;
        if (input.family == Family::Skinned || input.family == Family::StaticModel) {
            record->family = input.family;
        }
        MatrixCall call;
        call.valid = true;
        call.contextGeneration = m_summary.contextGeneration;
        call.sequence = ++m_matrixSequence;
        call.family = input.family;
        call.role = input.role;
        call.route = input.route;
        call.outcome = input.outcome;
        call.program = input.program;
        call.lifetime = input.lifetime;
        call.location = input.location;
        call.count = input.count;
        call.type = input.type;
        call.beforeVersion = input.beforeVersion;
        call.afterVersion = input.afterVersion;
        call.effectiveInputHash = input.effectiveInputHash;
        call.transpose = input.transpose;
        call.locationValid = input.locationValid;
        call.opaque = input.opaque;
        call.typeMatch = input.typeMatch;
        call.spirvPendingBefore = input.spirvPendingBefore;
        call.spirvPendingAfter = input.spirvPendingAfter;
        CopyName(call.uniformName, input.uniformName);

        record->latestAny = call;
        if (input.role == MatrixRole::Mvp) record->latestMvp = call;
        if (input.role == MatrixRole::Palette) record->latestPalette = call;
        if (input.role == MatrixRole::Transform) record->latestTransform = call;
        if (input.location == 0) record->latestLocationZero = call;

        const bool interesting = input.family == Family::Skinned ||
                                 input.family == Family::StaticModel || input.location == 0;
        if (interesting && m_summary.matrixLogs < kMaxMatrixLogs &&
            record->matrixLogs < kMaxMatrixLogsPerProgram) {
            ++m_summary.matrixLogs;
            ++record->matrixLogs;
            result.shouldLog = true;
        }
        result.call = call;
        return result;
    }

    void RecordUboBinding(const UboBindInput& input) {
        std::lock_guard lock(m_mutex);
        if (!m_summary.contextReady) return;
        ProgramRecord* record = FindLocked(input.program, input.lifetime);
        if (!record) return;

        // Hashing the whole shadow on every draw would itself become an unbounded
        // timing perturbation. Prepare only the one model candidate that can be
        // captured this present; CaptureModelDraw consumes it moments later.
        if (record->family != Family::Skinned && record->family != Family::StaticModel) return;
        if (m_lastCapturePresent == m_summary.presents || m_contextCaptures >= kMaxCaptures) return;
        const std::size_t familyIndex = record->family == Family::Skinned ? 0 : 1;
        if (m_familyCaptures[familyIndex] >= kMaxCapturesPerFamily) return;

        ++m_summary.uboBindings;
        if (input.path == UboPath::Ring) ++m_summary.uboRingBindings;
        if (input.path == UboPath::BufferSubData) ++m_summary.uboBufferSubDataBindings;
        if (input.freshSubmission) ++m_summary.uboFreshSubmissions;

        UboBinding binding = record->binding;
        const bool preserveSubmitted = binding.valid && binding.path == input.path &&
                                       binding.submittedKnown;
        if (!preserveSubmitted) binding = {};
        binding.valid = true;
        binding.contextGeneration = m_summary.contextGeneration;
        binding.sequence = ++m_uboSequence;
        binding.program = input.program;
        binding.lifetime = input.lifetime;
        binding.frontendVersion = input.contentVersion;
        binding.frontendSize = input.frontendSize;
        binding.frontendHash = HashBytes(input.frontendData, input.frontendSize);
        binding.path = input.path;
        binding.freshSubmission = input.freshSubmission;
        binding.ringGeneration = input.ringGeneration;
        binding.frameSerial = input.frameSerial;
        binding.ringOffset = input.ringOffset;
        if (input.submittedKnown && input.submittedData && input.submittedSize > 0) {
            binding.submittedKnown = true;
            binding.submittedVersion = input.contentVersion;
            binding.submittedSize = input.submittedSize;
            binding.submittedHash = HashBytes(input.submittedData, input.submittedSize);
        }
        record->binding = binding;
    }

    CaptureDecision CaptureModelDraw(Family family, std::uint32_t program,
                                     std::uint64_t lifetime, std::uint64_t presentSerial) {
        std::lock_guard lock(m_mutex);
        CaptureDecision decision;
        if (!m_summary.contextReady ||
            (family != Family::Skinned && family != Family::StaticModel)) {
            return decision;
        }
        ++m_summary.eligibleModelDraws;
        if (m_lastCapturePresent == presentSerial) {
            ++m_summary.rejectSamePresent;
            return decision;
        }
        const std::size_t familyIndex = family == Family::Skinned ? 0 : 1;
        if (m_familyCaptures[familyIndex] >= kMaxCapturesPerFamily) {
            ++m_summary.rejectFamilyLimit;
            return decision;
        }
        if (m_contextCaptures >= kMaxCaptures) {
            ++m_summary.rejectGlobalLimit;
            return decision;
        }

        ProgramRecord* record = FindOrCreateLocked(program, lifetime);
        if (!record) return decision;
        record->family = family;
        m_lastCapturePresent = presentSerial;
        ++m_contextCaptures;
        ++m_familyCaptures[familyIndex];
        ++m_summary.captures;
        if (family == Family::Skinned) ++m_summary.skinnedCaptures;
        else ++m_summary.staticCaptures;

        decision.selected = true;
        decision.capture = m_contextCaptures;
        decision.latestAny = record->latestAny;
        decision.latestMvp = record->latestMvp;
        decision.latestPalette = record->latestPalette;
        decision.latestTransform = record->latestTransform;
        decision.latestLocationZero = record->latestLocationZero;
        decision.binding = record->binding;

        const UboBinding& binding = decision.binding;
        if (!binding.valid || !binding.submittedKnown) {
            ++m_summary.payloadUnknown;
        } else if (binding.frontendVersion != binding.submittedVersion) {
            ++m_summary.payloadVersionMismatch;
            ++m_summary.payloadMismatch;
        } else if (binding.frontendSize == binding.submittedSize &&
                   binding.frontendHash == binding.submittedHash) {
            ++m_summary.payloadMatch;
        } else {
            ++m_summary.payloadMismatch;
        }
        if (decision.latestMvp.valid) ++m_summary.capturesWithMvpCall;
        if (decision.latestPalette.valid) ++m_summary.capturesWithPaletteCall;
        if (decision.latestTransform.valid) ++m_summary.capturesWithTransformCall;
        if (decision.latestLocationZero.valid && decision.latestLocationZero.opaque) {
            ++m_summary.capturesWithOpaqueLocationZero;
        }
        return decision;
    }

    Summary GetSummary() const {
        std::lock_guard lock(m_mutex);
        return m_summary;
    }

    std::uint64_t ContextReadyEvents() const {
        std::lock_guard lock(m_mutex);
        return m_contextReadyEvents;
    }

    std::uint64_t ContextDestroyedEvents() const {
        std::lock_guard lock(m_mutex);
        return m_contextDestroyedEvents;
    }

private:
    struct ProgramRecord {
        bool occupied = false;
        std::uint32_t program = 0;
        std::uint64_t lifetime = 0;
        Family family = Family::Other;
        std::uint32_t matrixLogs = 0;
        MatrixCall latestAny{};
        MatrixCall latestMvp{};
        MatrixCall latestPalette{};
        MatrixCall latestTransform{};
        MatrixCall latestLocationZero{};
        UboBinding binding{};
    };

    template <std::size_t N>
    static void CopyName(std::array<char, N>& destination, const char* source) {
        destination.fill('\0');
        if (!source || N == 0) return;
        std::strncpy(destination.data(), source, N - 1);
        for (std::size_t i = 0; i + 1 < N && destination[i] != '\0'; ++i) {
            char& c = destination[i];
            const unsigned char value = static_cast<unsigned char>(c);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || value < 0x20) c = '_';
        }
        destination[N - 1] = '\0';
    }

    ProgramRecord* FindLocked(std::uint32_t program, std::uint64_t lifetime) {
        for (ProgramRecord& record : m_programs) {
            if (record.occupied && record.program == program && record.lifetime == lifetime) {
                return &record;
            }
        }
        return nullptr;
    }

    ProgramRecord* FindOrCreateLocked(std::uint32_t program, std::uint64_t lifetime) {
        if (ProgramRecord* existing = FindLocked(program, lifetime)) return existing;
        for (ProgramRecord& record : m_programs) {
            if (!record.occupied) {
                record = {};
                record.occupied = true;
                record.program = program;
                record.lifetime = lifetime;
                return &record;
            }
        }
        ++m_summary.programOverflow;
        return nullptr;
    }

    void ResetContextLocked() {
        m_summary = {};
        m_programs = {};
        m_matrixSequence = 0;
        m_uboSequence = 0;
        m_contextCaptures = 0;
        m_familyCaptures = {};
        m_lastCapturePresent = ~std::uint64_t{0};
    }

    mutable std::mutex m_mutex;
    Summary m_summary{};
    std::array<ProgramRecord, kMaxPrograms> m_programs{};
    std::uint64_t m_matrixSequence = 0;
    std::uint64_t m_uboSequence = 0;
    std::uint32_t m_contextCaptures = 0;
    std::array<std::uint32_t, 2> m_familyCaptures{};
    std::uint64_t m_lastCapturePresent = ~std::uint64_t{0};
    std::uint64_t m_contextReadyEvents = 0;
    std::uint64_t m_contextDestroyedEvents = 0;
};

inline TraceState gTraceState;

} // namespace MobilePZ::PZF7
