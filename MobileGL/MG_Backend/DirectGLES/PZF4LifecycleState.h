// MobilePZ PZF4R1 - GL-independent lifecycle and selection gate.
// This header deliberately owns no GL objects and issues no GL calls, so the
// destroy-before-first-draw transition can be smoke-tested on the host.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace MobilePZ::PZF4R1 {

class LifecycleState {
public:
    static constexpr std::size_t kFamilyCount = 5;
    static constexpr std::size_t kMaxSignatures = 160;
    static constexpr std::array<std::uint64_t, kFamilyCount> kFamilyLimits = {
        64, 40, 44, 8, 4
    };

    enum class RejectReason : std::uint8_t {
        None = 0,
        ContextNotReady,
        CountZero,
        InstancesZero,
        DuplicateSignature,
        FamilyLimit,
        GlobalLimit,
    };

    struct BeginDecision {
        bool allowed = false;
        bool firstInGeneration = false;
        RejectReason reject = RejectReason::None;
    };

    struct ReserveDecision {
        bool selected = false;
        RejectReason reject = RejectReason::None;
        std::uint64_t drawId = 0;
    };

    // Returns true only when a new/rearmed context epoch was installed. A
    // repeated MakeCurrent for the same live generation must not reset bounds.
    bool OnContextReady(std::uint32_t generation) {
        if (m_contextReady && m_contextGeneration == generation) return false;
        m_contextReady = true;
        m_contextGeneration = generation;
        m_firstBeginLogged = false;
        m_signatureCount = 0;
        m_contextFamilySelected.fill(0);
        ++m_contextReadyEvents;
        return true;
    }

    void OnContextDestroyed() {
        m_contextReady = false;
        ++m_contextDestroyedEvents;
    }

    bool ContextReadyFor(std::uint32_t generation) const {
        return m_contextReady && m_contextGeneration == generation;
    }

    BeginDecision Begin(std::int64_t count, std::int64_t instances) {
        ++m_beginCalls;
        const bool first = m_contextReady && !m_firstBeginLogged;
        if (first) m_firstBeginLogged = true;

        if (!m_contextReady) {
            ++m_rejectContextNotReady;
            return {false, false, RejectReason::ContextNotReady};
        }
        if (count <= 0) {
            ++m_rejectCountZero;
            return {false, first, RejectReason::CountZero};
        }
        if (instances <= 0) {
            ++m_rejectInstancesZero;
            return {false, first, RejectReason::InstancesZero};
        }
        return {true, first, RejectReason::None};
    }

    ReserveDecision Reserve(std::size_t familyIndex, std::uint64_t signature) {
        if (familyIndex >= kFamilyCount) familyIndex = kFamilyCount - 1;
        if (m_signatureCount >= kMaxSignatures) {
            ++m_rejectGlobalLimit;
            return {false, RejectReason::GlobalLimit, 0};
        }
        if (m_contextFamilySelected[familyIndex] >= kFamilyLimits[familyIndex]) {
            ++m_rejectFamilyLimit;
            return {false, RejectReason::FamilyLimit, 0};
        }
        for (std::size_t i = 0; i < m_signatureCount; ++i) {
            if (m_signatures[i] == signature) {
                ++m_rejectDuplicateSignature;
                return {false, RejectReason::DuplicateSignature, 0};
            }
        }

        m_signatures[m_signatureCount++] = signature;
        ++m_contextFamilySelected[familyIndex];
        ++m_familySelected[familyIndex];
        const std::uint64_t drawId = ++m_selected;
        return {true, RejectReason::None, drawId};
    }

    bool ContextReady() const { return m_contextReady; }
    std::uint32_t ContextGeneration() const { return m_contextGeneration; }
    std::uint64_t ContextReadyEvents() const { return m_contextReadyEvents; }
    std::uint64_t ContextDestroyedEvents() const { return m_contextDestroyedEvents; }
    std::uint64_t BeginCalls() const { return m_beginCalls; }
    std::uint64_t RejectContextNotReady() const { return m_rejectContextNotReady; }
    std::uint64_t RejectCountZero() const { return m_rejectCountZero; }
    std::uint64_t RejectInstancesZero() const { return m_rejectInstancesZero; }
    std::uint64_t RejectDuplicateSignature() const { return m_rejectDuplicateSignature; }
    std::uint64_t RejectFamilyLimit() const { return m_rejectFamilyLimit; }
    std::uint64_t RejectGlobalLimit() const { return m_rejectGlobalLimit; }
    std::uint64_t Selected() const { return m_selected; }
    std::size_t SignatureCount() const { return m_signatureCount; }
    std::uint64_t FamilySelected(std::size_t index) const {
        return index < kFamilyCount ? m_familySelected[index] : 0;
    }

private:
    bool m_contextReady = false;
    bool m_firstBeginLogged = false;
    std::uint32_t m_contextGeneration = 0;
    std::array<std::uint64_t, kMaxSignatures> m_signatures{};
    std::size_t m_signatureCount = 0;
    std::array<std::uint64_t, kFamilyCount> m_contextFamilySelected{};
    std::array<std::uint64_t, kFamilyCount> m_familySelected{};
    std::uint64_t m_contextReadyEvents = 0;
    std::uint64_t m_contextDestroyedEvents = 0;
    std::uint64_t m_beginCalls = 0;
    std::uint64_t m_rejectContextNotReady = 0;
    std::uint64_t m_rejectCountZero = 0;
    std::uint64_t m_rejectInstancesZero = 0;
    std::uint64_t m_rejectDuplicateSignature = 0;
    std::uint64_t m_rejectFamilyLimit = 0;
    std::uint64_t m_rejectGlobalLimit = 0;
    std::uint64_t m_selected = 0;
};

} // namespace MobilePZ::PZF4R1
