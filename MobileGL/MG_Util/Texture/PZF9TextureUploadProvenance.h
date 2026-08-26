// MobilePZ PZF9 - bounded model-texture upload provenance.
// Diagnostic only: observes CPU bytes already read by the normal upload path.
#pragma once

#include <Includes.h>

namespace MobilePZ::PZF9 {

using MobileGL::Bool;
using MobileGL::Int;
using MobileGL::SizeT;
using MobileGL::Uint;
using MobileGL::Uint8;
using MobileGL::Uint64;
using MobileGL::UnorderedMap;

constexpr Uint64 kMaxObservedBytesPerStage = 16ull * 1024ull * 1024ull;

struct ByteStats {
    Bool valid = false;
    Uint64 bytes = 0;
    Uint64 zero = 0;
    Uint64 nonzero = 0;
    Uint64 hash = 1469598103934665603ull;
};

inline void Accumulate(ByteStats& stats, const Uint8* bytes, SizeT size) {
    if (!bytes || size == 0) return;
    stats.valid = true;
    stats.bytes += static_cast<Uint64>(size);
    for (SizeT i = 0; i < size; ++i) {
        const Uint8 value = bytes[i];
        stats.zero += value == 0 ? 1 : 0;
        stats.nonzero += value != 0 ? 1 : 0;
        stats.hash ^= static_cast<Uint64>(value);
        stats.hash *= 1099511628211ull;
    }
}

inline ByteStats AnalyzeBytes(const void* data, SizeT size) {
    ByteStats stats;
    if (static_cast<Uint64>(size) > kMaxObservedBytesPerStage) return stats;
    Accumulate(stats, static_cast<const Uint8*>(data), size);
    return stats;
}

// Hash only the actual client texels consumed by the normal unpack path. Padding and
// skipped rows remain outside the observation, matching ProcessTexturePixelsDataUnpack.
inline ByteStats AnalyzeUnpackSource(const void* data, Int width, Int height, Int depth,
                                     SizeT pixelSize, Int alignment, Int rowLength,
                                     Int imageHeight, Int skipPixels, Int skipRows,
                                     Int skipImages) {
    ByteStats stats;
    if (!data || width <= 0 || height <= 0 || depth <= 0 || pixelSize == 0) return stats;
    const SizeT effectiveWidth = static_cast<SizeT>(rowLength > 0 ? rowLength : width);
    const SizeT effectiveHeight = static_cast<SizeT>(imageHeight > 0 ? imageHeight : height);
    const SizeT safeAlignment = static_cast<SizeT>(alignment > 0 ? alignment : 1);
    if (effectiveWidth > std::numeric_limits<SizeT>::max() / pixelSize) return stats;
    const SizeT rowBytes = effectiveWidth * pixelSize;
    if (rowBytes > std::numeric_limits<SizeT>::max() - (safeAlignment - 1)) return stats;
    const SizeT rowStride = ((rowBytes + safeAlignment - 1) / safeAlignment) * safeAlignment;
    if (static_cast<SizeT>(width) > std::numeric_limits<SizeT>::max() / pixelSize) return stats;
    const SizeT usedRowBytes = static_cast<SizeT>(width) * pixelSize;
    if (usedRowBytes > 0 && static_cast<SizeT>(height) >
        static_cast<SizeT>(kMaxObservedBytesPerStage) / usedRowBytes) return stats;
    const SizeT usedImageBytes = usedRowBytes * static_cast<SizeT>(height);
    if (usedImageBytes > 0 && static_cast<SizeT>(depth) >
        static_cast<SizeT>(kMaxObservedBytesPerStage) / usedImageBytes) return stats;
    if (effectiveHeight > 0 && rowStride > std::numeric_limits<SizeT>::max() / effectiveHeight) return stats;
    const SizeT imageStride = effectiveHeight * rowStride;
    const Uint8* base = static_cast<const Uint8*>(data);
    base += static_cast<SizeT>(std::max(skipImages, 0)) * imageStride;
    base += static_cast<SizeT>(std::max(skipRows, 0)) * rowStride;
    base += static_cast<SizeT>(std::max(skipPixels, 0)) * pixelSize;
    for (Int z = 0; z < depth; ++z) {
        const Uint8* image = base + static_cast<SizeT>(z) * imageStride;
        for (Int y = 0; y < height; ++y) {
            Accumulate(stats, image + static_cast<SizeT>(y) * rowStride, usedRowBytes);
        }
    }
    return stats;
}

enum class UploadKind : Uint8 { None = 0, Image2D, SubImage2D, NullImage2D };

inline const char* UploadKindName(UploadKind kind) {
    switch (kind) {
    case UploadKind::Image2D: return "TEX_IMAGE_2D";
    case UploadKind::SubImage2D: return "TEX_SUB_IMAGE_2D";
    case UploadKind::NullImage2D: return "TEX_IMAGE_2D_NULL";
    default: return "NONE";
    }
}

struct Record {
    Uint64 lifetime = 0;
    Uint frontTexture = 0;
    Uint64 clientSequence = 0;
    Uint64 backendSequence = 0;
    Uint64 imageCalls = 0;
    Uint64 subImageCalls = 0;
    Uint64 nullImageCalls = 0;
    Uint64 sourceNonzeroCalls = 0;
    Uint64 sourceAllZeroCalls = 0;
    Uint64 processedNonzeroCalls = 0;
    Uint64 processedAllZeroCalls = 0;
    Uint64 sourceNonzeroProcessedZero = 0;
    UploadKind lastKind = UploadKind::None;
    Int level = 0;
    Int xoffset = 0;
    Int yoffset = 0;
    Int width = 0;
    Int height = 0;
    Uint internalFormat = 0;
    Uint inputFormat = 0;
    Uint inputType = 0;
    Bool pbo = false;
    ByteStats source;
    ByteStats processed;
    Bool backendSeen = false;
    Uint nativeTexture = 0;
    Uint backendFormat = 0;
    Uint backendType = 0;
    Uint backendConversion = 0;
    ByteStats backendShadow;
    ByteStats driverPayload;
};

struct Summary {
    Uint contextGeneration = 0;
    Uint64 records = 0;
    Uint64 clientCalls = 0;
    Uint64 imageCalls = 0;
    Uint64 subImageCalls = 0;
    Uint64 nullImageCalls = 0;
    Uint64 sourceNonzeroCalls = 0;
    Uint64 sourceAllZeroCalls = 0;
    Uint64 processedNonzeroCalls = 0;
    Uint64 processedAllZeroCalls = 0;
    Uint64 sourceNonzeroProcessedZero = 0;
    Uint64 backendObservations = 0;
    Uint64 driverPayloadNonzero = 0;
    Uint64 driverPayloadAllZero = 0;
    Uint64 modelLookups = 0;
    Uint64 modelMatches = 0;
    Uint64 modelMissing = 0;
    Uint64 modelLogs = 0;
};

inline std::mutex g_mutex;
inline UnorderedMap<Uint64, Record> g_records;
inline UnorderedMap<Uint64, Uint64> g_reportedClientSequence;
inline Summary g_summary;
inline Uint64 g_sequence = 0;

inline void RecordClientUpload(Uint64 lifetime, Uint frontTexture, UploadKind kind,
                               Int level, Int xoffset, Int yoffset, Int width, Int height,
                               Uint internalFormat, Uint inputFormat, Uint inputType, Bool pbo,
                               const ByteStats& source, const ByteStats& processed) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record& record = g_records[lifetime];
    record.lifetime = lifetime;
    record.frontTexture = frontTexture;
    record.clientSequence = ++g_sequence;
    record.lastKind = kind;
    record.level = level;
    record.xoffset = xoffset;
    record.yoffset = yoffset;
    record.width = width;
    record.height = height;
    record.internalFormat = internalFormat;
    record.inputFormat = inputFormat;
    record.inputType = inputType;
    record.pbo = pbo;
    record.source = source;
    record.processed = processed;
    ++g_summary.clientCalls;
    if (kind == UploadKind::Image2D) {
        ++record.imageCalls;
        ++g_summary.imageCalls;
    } else if (kind == UploadKind::SubImage2D) {
        ++record.subImageCalls;
        ++g_summary.subImageCalls;
    } else if (kind == UploadKind::NullImage2D) {
        ++record.nullImageCalls;
        ++g_summary.nullImageCalls;
    }
    if (source.valid) {
        if (source.nonzero != 0) {
            ++record.sourceNonzeroCalls;
            ++g_summary.sourceNonzeroCalls;
        } else {
            ++record.sourceAllZeroCalls;
            ++g_summary.sourceAllZeroCalls;
        }
    }
    if (processed.valid) {
        if (processed.nonzero != 0) {
            ++record.processedNonzeroCalls;
            ++g_summary.processedNonzeroCalls;
        } else {
            ++record.processedAllZeroCalls;
            ++g_summary.processedAllZeroCalls;
        }
    }
    if (source.valid && source.nonzero != 0 && processed.valid && processed.nonzero == 0) {
        ++record.sourceNonzeroProcessedZero;
        ++g_summary.sourceNonzeroProcessedZero;
    }
    g_summary.records = static_cast<Uint64>(g_records.size());
}

inline void RecordBackendUpload(Uint64 lifetime, Uint frontTexture, Uint nativeTexture,
                                Uint backendFormat, Uint backendType, Uint conversion,
                                const ByteStats& shadow, const ByteStats& driverPayload) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record& record = g_records[lifetime];
    record.lifetime = lifetime;
    record.frontTexture = frontTexture;
    record.backendSequence = ++g_sequence;
    record.backendSeen = true;
    record.nativeTexture = nativeTexture;
    record.backendFormat = backendFormat;
    record.backendType = backendType;
    record.backendConversion = conversion;
    record.backendShadow = shadow;
    record.driverPayload = driverPayload;
    ++g_summary.backendObservations;
    if (driverPayload.valid) {
        if (driverPayload.nonzero != 0) ++g_summary.driverPayloadNonzero;
        else ++g_summary.driverPayloadAllZero;
    }
    g_summary.records = static_cast<Uint64>(g_records.size());
}

// 0 = already reported this exact upload state, 1 = record, 2 = no upload record.
inline Int TakeModelSnapshot(Uint64 lifetime, Record& out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_summary.modelLookups;
    const auto found = g_records.find(lifetime);
    const Uint64 sequence = found == g_records.end() ? 0 : found->second.clientSequence;
    const auto reported = g_reportedClientSequence.find(lifetime);
    if (reported != g_reportedClientSequence.end() && reported->second == sequence) return 0;
    g_reportedClientSequence[lifetime] = sequence;
    ++g_summary.modelLogs;
    if (found == g_records.end()) {
        ++g_summary.modelMissing;
        return 2;
    }
    ++g_summary.modelMatches;
    out = found->second;
    return 1;
}

// Read-only lookup for later diagnostics which need to classify a texture before a
// draw. Unlike TakeModelSnapshot this does not consume/report a sequence and does not
// alter the PZF9 summary. The copy keeps the mutex out of every GL query/repair path.
inline Bool PeekRecord(Uint64 lifetime, Record& out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto found = g_records.find(lifetime);
    if (found == g_records.end()) return false;
    out = found->second;
    return true;
}

inline void OnContextReady(Uint generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_summary.contextGeneration = generation;
    g_reportedClientSequence.clear();
}

inline Summary GetSummary() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_summary.records = static_cast<Uint64>(g_records.size());
    return g_summary;
}

} // namespace MobilePZ::PZF9
