// MobileGL - MobileGL/MG_Util/PZV1/PZV1QuadTargetTracker.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once

#include <Includes.h>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_set>

namespace MobilePZ::V1 {
    // Production replacement for the PZF9/PZF13/PZF14/PZF15 diagnostic chain.
    //
    // The accepted R2 fix needs one fact at DrawRangeElements: whether the current
    // frontend FBO is inside the clear-to-detach lifetime of a small, level-zero,
    // RGBA8 Texture2D that was allocated with NULL data.  The old diagnostics
    // derived that fact while also taking locks and updating millions of counters
    // on the draw path.  This tracker keeps only the required state:
    //
    //   * NULL-allocation membership is queried only at a framebuffer clear;
    //   * active FBO lookup is one acquire load per exact quad4 draw;
    //   * a completed target window publishes one single-use texture-lifetime
    //     handoff for its later depth-disabled compositor quad;
    //   * no GL query, event counter, repair, or per-draw log is performed.
    //
    // Frontend FBO names are normally small.  A direct table avoids hashing and
    // makes the hot lookup deterministic.  Names outside the table are preserved
    // (never translated) rather than risking a false-positive rendering mutation.
    inline constexpr MobileGL::SizeT kTrackedFboSlots = 4096;
    inline constexpr MobileGL::SizeT kCompositeTextureSlots = 4096;

    struct ActiveFboSlot {
        std::atomic<MobileGL::Uint64> lifetime{0};
        std::atomic<MobileGL::Int> attachment{-1};
        std::atomic<MobileGL::Bool> quadWritten{false};
    };

    inline std::array<ActiveFboSlot, kTrackedFboSlots> g_activeFbos{};
    // Direct-mapped and exact-value checked: a collision can only discard an
    // older handoff (false negative), never admit an unrelated texture.
    inline std::array<std::atomic<MobileGL::Uint64>, kCompositeTextureSlots>
        g_compositeTextureLifetimes{};
    inline std::mutex g_nullTextureMutex;
    inline std::unordered_set<MobileGL::Uint64> g_nullTextureLifetimes;

    inline MobileGL::SizeT CompositeTextureSlot(MobileGL::Uint64 lifetime) {
        static_assert((kCompositeTextureSlots & (kCompositeTextureSlots - 1)) == 0);
        return static_cast<MobileGL::SizeT>(lifetime) & (kCompositeTextureSlots - 1);
    }

    inline void ForgetCompositeTexture(MobileGL::Uint64 lifetime) {
        if (lifetime == 0) return;
        auto& slot = g_compositeTextureLifetimes[CompositeTextureSlot(lifetime)];
        MobileGL::Uint64 expected = lifetime;
        slot.compare_exchange_strong(expected, 0, std::memory_order_acq_rel,
                                     std::memory_order_relaxed);
    }

    inline void RecordNullAllocation(MobileGL::Uint64 lifetime) {
        if (lifetime == 0) return;
        // A new storage definition starts a new producer epoch even when the
        // frontend texture object (and therefore its lifetime id) is reused.
        ForgetCompositeTexture(lifetime);
        std::lock_guard<std::mutex> lock(g_nullTextureMutex);
        g_nullTextureLifetimes.insert(lifetime);
    }

    inline void RecordDefinedAllocation(MobileGL::Uint64 lifetime) {
        if (lifetime == 0) return;
        ForgetCompositeTexture(lifetime);
        {
            std::lock_guard<std::mutex> lock(g_nullTextureMutex);
            g_nullTextureLifetimes.erase(lifetime);
        }
        // A base-level upload can replace a NULL allocation while the texture
        // is still attached.  Population changes are cold-path operations, so
        // close any matching window here instead of adding a lock to each draw.
        for (MobileGL::SizeT fbo = 1; fbo < kTrackedFboSlots; ++fbo) {
            auto& slot = g_activeFbos[fbo];
            if (slot.lifetime.load(std::memory_order_acquire) == lifetime) {
                slot.lifetime.store(0, std::memory_order_release);
                slot.attachment.store(-1, std::memory_order_relaxed);
                slot.quadWritten.store(false, std::memory_order_relaxed);
            }
        }
    }

    inline MobileGL::Bool WasNullAllocated(MobileGL::Uint64 lifetime) {
        if (lifetime == 0) return false;
        std::lock_guard<std::mutex> lock(g_nullTextureMutex);
        return g_nullTextureLifetimes.contains(lifetime);
    }

    inline void ActivateFramebuffer(MobileGL::Uint fbo, MobileGL::Uint64 lifetime,
                                    MobileGL::Int attachment) {
        if (fbo == 0 || fbo >= kTrackedFboSlots || lifetime == 0) return;
        // A new clear/population window supersedes an unconsumed token from the
        // preceding frame of the same reusable render-target texture.
        ForgetCompositeTexture(lifetime);
        auto& slot = g_activeFbos[fbo];
        slot.attachment.store(attachment, std::memory_order_relaxed);
        slot.quadWritten.store(false, std::memory_order_relaxed);
        slot.lifetime.store(lifetime, std::memory_order_release);
    }

    inline void MarkFramebufferQuadWritten(MobileGL::Uint fbo) {
        if (fbo == 0 || fbo >= kTrackedFboSlots) return;
        auto& slot = g_activeFbos[fbo];
        if (slot.lifetime.load(std::memory_order_acquire) != 0) {
            slot.quadWritten.store(true, std::memory_order_release);
        }
    }

    inline void DeactivateFramebuffer(MobileGL::Uint fbo, MobileGL::Uint64 lifetime,
                                      MobileGL::Int attachment) {
        if (fbo == 0 || fbo >= kTrackedFboSlots || lifetime == 0) return;
        auto& slot = g_activeFbos[fbo];
        if (slot.lifetime.load(std::memory_order_acquire) != lifetime ||
            slot.attachment.load(std::memory_order_relaxed) != attachment) {
            return;
        }
        slot.lifetime.store(0, std::memory_order_release);
        slot.attachment.store(-1, std::memory_order_relaxed);
        slot.quadWritten.store(false, std::memory_order_relaxed);
    }

    // Close a proven clear -> translated quad -> detach producer window and
    // make its exact texture lifetime eligible for one compositor draw.  The
    // token is deliberately not published by generic invalidation paths.
    inline void PublishAndDeactivateFramebuffer(MobileGL::Uint fbo,
                                                MobileGL::Uint64 lifetime,
                                                MobileGL::Int attachment) {
        if (fbo == 0 || fbo >= kTrackedFboSlots || lifetime == 0) return;
        auto& slot = g_activeFbos[fbo];
        if (slot.lifetime.load(std::memory_order_acquire) != lifetime ||
            slot.attachment.load(std::memory_order_relaxed) != attachment) {
            return;
        }
        const MobileGL::Bool publish =
            slot.quadWritten.exchange(false, std::memory_order_acq_rel);
        slot.lifetime.store(0, std::memory_order_release);
        slot.attachment.store(-1, std::memory_order_relaxed);
        if (publish) {
            g_compositeTextureLifetimes[CompositeTextureSlot(lifetime)].store(
                lifetime, std::memory_order_release);
        }
    }

    inline void DeactivateFramebuffer(MobileGL::Uint fbo) {
        if (fbo == 0 || fbo >= kTrackedFboSlots) return;
        auto& slot = g_activeFbos[fbo];
        slot.lifetime.store(0, std::memory_order_release);
        slot.attachment.store(-1, std::memory_order_relaxed);
        slot.quadWritten.store(false, std::memory_order_relaxed);
    }

    inline MobileGL::Bool IsActiveFramebuffer(MobileGL::Uint fbo) {
        return fbo != 0 && fbo < kTrackedFboSlots &&
               g_activeFbos[fbo].lifetime.load(std::memory_order_acquire) != 0;
    }

    inline MobileGL::Bool TryConsumeCompositeTexture(MobileGL::Uint64 lifetime) {
        if (lifetime == 0) return false;
        auto& slot = g_compositeTextureLifetimes[CompositeTextureSlot(lifetime)];
        MobileGL::Uint64 expected = lifetime;
        return slot.compare_exchange_strong(expected, 0, std::memory_order_acq_rel,
                                            std::memory_order_relaxed);
    }

    inline void Reset() {
        for (auto& slot : g_activeFbos) {
            slot.lifetime.store(0, std::memory_order_relaxed);
            slot.attachment.store(-1, std::memory_order_relaxed);
            slot.quadWritten.store(false, std::memory_order_relaxed);
        }
        for (auto& lifetime : g_compositeTextureLifetimes) {
            lifetime.store(0, std::memory_order_relaxed);
        }
        std::lock_guard<std::mutex> lock(g_nullTextureMutex);
        g_nullTextureLifetimes.clear();
    }
} // namespace MobilePZ::V1
