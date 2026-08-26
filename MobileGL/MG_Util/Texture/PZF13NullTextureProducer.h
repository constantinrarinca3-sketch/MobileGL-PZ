// MobilePZ PZF13 - bounded producer lineage for null-allocated model textures.
//
// PZF12 proved that the CPU_ZERO_GPU_ZERO model inputs were never observed as
// draw targets. PZF13 keeps a small lifetime-keyed history across every remaining
// population route: framebuffer attachment/status, draw/clear/blit writes, direct
// texture writes and image bindings. The DirectGLES write gate may only repair a
// stale native framebuffer attachment; matching paths remain observation-only.
#pragma once

#include <Includes.h>

namespace MobilePZ::PZF13 {

using MobileGL::Bool;
using MobileGL::Int;
using MobileGL::Uint;
using MobileGL::Uint64;
using MobileGL::UnorderedMap;

constexpr MobileGL::SizeT kMaxRecords = 8192;

enum class WriteKind : MobileGL::Uint8 {
    Draw = 0,
    Clear,
    ClearBuffer,
    Blit,
    NamedClear,
    NamedBlit,
    DirectClear,
    DirectUpload,
};

inline const char* WriteKindName(WriteKind kind) {
    switch (kind) {
    case WriteKind::Draw: return "DRAW";
    case WriteKind::Clear: return "CLEAR";
    case WriteKind::ClearBuffer: return "CLEAR_BUFFER";
    case WriteKind::Blit: return "BLIT";
    case WriteKind::NamedClear: return "NAMED_CLEAR";
    case WriteKind::NamedBlit: return "NAMED_BLIT";
    case WriteKind::DirectClear: return "DIRECT_CLEAR";
    case WriteKind::DirectUpload: return "DIRECT_UPLOAD";
    default: return "UNKNOWN";
    }
}

struct Record {
    Uint64 lifetime = 0;
    Uint frontTexture = 0;
    Int width = 0;
    Int height = 0;
    Uint64 nullAllocations = 0;
    Uint64 attachCalls = 0;
    Uint64 detachCalls = 0;
    Uint64 statusChecks = 0;
    Uint64 statusComplete = 0;
    Uint64 statusIncomplete = 0;
    Uint64 drawWrites = 0;
    Uint64 clearWrites = 0;
    Uint64 clearBufferWrites = 0;
    Uint64 blitWrites = 0;
    Uint64 namedClearWrites = 0;
    Uint64 namedBlitWrites = 0;
    Uint64 directClearWrites = 0;
    Uint64 directUploadWrites = 0;
    Uint64 imageWriteBinds = 0;
    Uint64 identityMatches = 0;
    Uint64 identityMismatches = 0;
    Uint64 repairsSucceeded = 0;
    Uint64 repairsFailed = 0;
    Uint64 consumers = 0;
    Uint64 producerVersion = 0;
    Uint64 reportedProducerVersion = ~Uint64{0};
    Uint lastFbo = 0;
    Int lastAttachment = -1;
    Uint lastStatus = 0;
    WriteKind lastWrite = WriteKind::Draw;
    Bool currentlyAttached = false;
};

struct Summary {
    Uint generation = 0;
    Uint64 records = 0;
    Uint64 recordOverflow = 0;
    Uint64 nullAllocations = 0;
    Uint64 attaches = 0;
    Uint64 detaches = 0;
    Uint64 statusChecks = 0;
    Uint64 framebufferWrites = 0;
    Uint64 directWrites = 0;
    Uint64 imageWriteBinds = 0;
    Uint64 identityQueries = 0;
    Uint64 identityMatches = 0;
    Uint64 identityMismatches = 0;
    Uint64 repairsSucceeded = 0;
    Uint64 repairsFailed = 0;
    Uint64 consumers = 0;
    Uint64 consumerLogs = 0;
};

inline std::mutex g_mutex;
inline UnorderedMap<Uint64, Record> g_records;
inline Summary g_summary;

inline Record* FindOrCreateLocked(Uint64 lifetime, Uint frontTexture) {
    if (lifetime == 0) return nullptr;
    auto found = g_records.find(lifetime);
    if (found != g_records.end()) {
        if (frontTexture != 0) found->second.frontTexture = frontTexture;
        return &found->second;
    }
    if (g_records.size() >= kMaxRecords) {
        ++g_summary.recordOverflow;
        return nullptr;
    }
    Record& record = g_records[lifetime];
    record.lifetime = lifetime;
    record.frontTexture = frontTexture;
    g_summary.records = static_cast<Uint64>(g_records.size());
    return &record;
}

inline void RecordNullAllocation(Uint64 lifetime, Uint frontTexture, Int width, Int height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* record = FindOrCreateLocked(lifetime, frontTexture);
    if (!record) return;
    // TexImage2D(NULL) invalidates any content provenance from an earlier
    // allocation of this same mutable texture object.  Keep only the live
    // attachment identity: redefining storage does not detach the texture.
    record->attachCalls = 0;
    record->detachCalls = 0;
    record->statusChecks = 0;
    record->statusComplete = 0;
    record->statusIncomplete = 0;
    record->drawWrites = 0;
    record->clearWrites = 0;
    record->clearBufferWrites = 0;
    record->blitWrites = 0;
    record->namedClearWrites = 0;
    record->namedBlitWrites = 0;
    record->directClearWrites = 0;
    record->directUploadWrites = 0;
    record->imageWriteBinds = 0;
    record->identityMatches = 0;
    record->identityMismatches = 0;
    record->repairsSucceeded = 0;
    record->repairsFailed = 0;
    record->lastStatus = 0;
    record->lastWrite = WriteKind::Draw;
    record->width = width;
    record->height = height;
    ++record->nullAllocations;
    ++g_summary.nullAllocations;
    // Every NULL allocation starts a new producer epoch, so the next model
    // consumer emits a fresh verdict even when the frontend name is reused.
    ++record->producerVersion;
}

inline void RecordAttachment(Uint64 lifetime, Uint frontTexture, Uint fbo, Int attachment) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* record = FindOrCreateLocked(lifetime, frontTexture);
    if (!record) return;
    ++record->attachCalls;
    record->currentlyAttached = true;
    record->lastFbo = fbo;
    record->lastAttachment = attachment;
    ++record->producerVersion;
    ++g_summary.attaches;
}

inline void RecordDetachment(Uint64 lifetime, Uint frontTexture, Uint fbo, Int attachment) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* record = FindOrCreateLocked(lifetime, frontTexture);
    if (!record) return;
    ++record->detachCalls;
    record->currentlyAttached = false;
    record->lastFbo = fbo;
    record->lastAttachment = attachment;
    ++record->producerVersion;
    ++g_summary.detaches;
}

inline void RecordStatus(Uint64 lifetime, Uint frontTexture, Uint fbo, Int attachment,
                         Uint status, Bool complete) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* record = FindOrCreateLocked(lifetime, frontTexture);
    if (!record) return;
    ++record->statusChecks;
    if (complete) ++record->statusComplete;
    else ++record->statusIncomplete;
    record->lastFbo = fbo;
    record->lastAttachment = attachment;
    record->lastStatus = status;
    ++record->producerVersion;
    ++g_summary.statusChecks;
}

inline void RecordWrite(Uint64 lifetime, Uint frontTexture, Uint fbo, Int attachment, WriteKind kind) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* record = FindOrCreateLocked(lifetime, frontTexture);
    if (!record) return;
    switch (kind) {
    case WriteKind::Draw: ++record->drawWrites; ++g_summary.framebufferWrites; break;
    case WriteKind::Clear: ++record->clearWrites; ++g_summary.framebufferWrites; break;
    case WriteKind::ClearBuffer: ++record->clearBufferWrites; ++g_summary.framebufferWrites; break;
    case WriteKind::Blit: ++record->blitWrites; ++g_summary.framebufferWrites; break;
    case WriteKind::NamedClear: ++record->namedClearWrites; ++g_summary.framebufferWrites; break;
    case WriteKind::NamedBlit: ++record->namedBlitWrites; ++g_summary.framebufferWrites; break;
    case WriteKind::DirectClear: ++record->directClearWrites; ++g_summary.directWrites; break;
    case WriteKind::DirectUpload: ++record->directUploadWrites; ++g_summary.directWrites; break;
    }
    record->lastFbo = fbo;
    record->lastAttachment = attachment;
    record->lastWrite = kind;
    ++record->producerVersion;
}

inline void RecordImageWriteBind(Uint64 lifetime, Uint frontTexture) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record* record = FindOrCreateLocked(lifetime, frontTexture);
    if (!record) return;
    ++record->imageWriteBinds;
    ++record->producerVersion;
    ++g_summary.imageWriteBinds;
}

inline void RecordIdentity(Uint64 lifetime, Bool mismatch, Bool repaired) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_records.find(lifetime);
    if (found == g_records.end()) return;
    Record& record = found->second;
    ++g_summary.identityQueries;
    if (mismatch) {
        ++record.identityMismatches;
        ++g_summary.identityMismatches;
        if (repaired) {
            ++record.repairsSucceeded;
            ++g_summary.repairsSucceeded;
        } else {
            ++record.repairsFailed;
            ++g_summary.repairsFailed;
        }
    } else {
        ++record.identityMatches;
        ++g_summary.identityMatches;
    }
    ++record.producerVersion;
}

inline Bool TakeConsumerSnapshot(Uint64 lifetime, Record& out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_records.find(lifetime);
    if (found == g_records.end()) return false;
    Record& record = found->second;
    ++record.consumers;
    ++g_summary.consumers;
    if (record.reportedProducerVersion == record.producerVersion) return false;
    record.reportedProducerVersion = record.producerVersion;
    out = record;
    ++g_summary.consumerLogs;
    return true;
}

inline Summary GetSummary() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_summary.records = static_cast<Uint64>(g_records.size());
    return g_summary;
}

inline void OnContextReady(Uint generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_records.clear();
    g_summary = {};
    g_summary.generation = generation;
}

} // namespace MobilePZ::PZF13
