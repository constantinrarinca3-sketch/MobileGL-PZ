// MobilePZ PZF10 - bounded RGBA8 copy-texture repair provenance.
// Replaces the affected DirectGLES color-copy operation with an equivalent
// readback + sub-image upload and keeps the frontend texture shadow coherent.
#pragma once

#include <Includes.h>

namespace MobilePZ::PZF10 {

using MobileGL::Bool;
using MobileGL::Int;
using MobileGL::SizeT;
using MobileGL::Uint;
using MobileGL::Uint8;
using MobileGL::Uint64;
using MobileGL::UnorderedMap;

constexpr Uint64 kMaxStagedBytes = 4ull * 1024ull * 1024ull;

enum class CopyKind : Uint8 { None = 0, Image2D, SubImage2D };

inline const char* CopyKindName(CopyKind kind) {
    switch (kind) {
    case CopyKind::Image2D: return "COPY_TEX_IMAGE_2D";
    case CopyKind::SubImage2D: return "COPY_TEX_SUB_IMAGE_2D";
    default: return "NONE";
    }
}

struct PixelStats {
    Bool valid = false;
    Uint64 bytes = 0;
    Uint64 pixels = 0;
    Uint64 zeroRgba = 0;
    Uint64 rgbNonzero = 0;
    Uint64 alphaNonzero = 0;
    Uint alphaMin = 0;
    Uint alphaMax = 0;
    Uint64 hash = 1469598103934665603ull;
};

inline PixelStats AnalyzeRGBA8(const void* data, SizeT size) {
    PixelStats stats;
    if (!data || size == 0 || (size & 3u) != 0 || static_cast<Uint64>(size) > kMaxStagedBytes) return stats;
    stats.valid = true;
    stats.bytes = static_cast<Uint64>(size);
    stats.pixels = static_cast<Uint64>(size / 4u);
    stats.alphaMin = 255;
    const auto* rgba = static_cast<const Uint8*>(data);
    for (SizeT i = 0; i < size; i += 4) {
        const Uint8 r = rgba[i + 0];
        const Uint8 g = rgba[i + 1];
        const Uint8 b = rgba[i + 2];
        const Uint8 a = rgba[i + 3];
        stats.zeroRgba += (r | g | b | a) == 0 ? 1 : 0;
        stats.rgbNonzero += (r | g | b) != 0 ? 1 : 0;
        stats.alphaNonzero += a != 0 ? 1 : 0;
        stats.alphaMin = std::min<Uint>(stats.alphaMin, a);
        stats.alphaMax = std::max<Uint>(stats.alphaMax, a);
        for (SizeT channel = 0; channel < 4; ++channel) {
            stats.hash ^= static_cast<Uint64>(rgba[i + channel]);
            stats.hash *= 1099511628211ull;
        }
    }
    return stats;
}

struct Record {
    Uint64 lifetime = 0;
    Uint frontTexture = 0;
    Uint nativeTexture = 0;
    Uint64 sequence = 0;
    Uint64 imageCalls = 0;
    Uint64 subImageCalls = 0;
    CopyKind lastKind = CopyKind::None;
    Int level = 0;
    Int srcX = 0;
    Int srcY = 0;
    Int dstX = 0;
    Int dstY = 0;
    Int width = 0;
    Int height = 0;
    Uint readFbo = 0;
    Uint readBuffer = 0;
    Uint readFboStatus = 0;
    Uint readError = 0;
    Uint uploadError = 0;
    Bool eligible = false;
    Bool staged = false;
    Bool shadowUpdated = false;
    PixelStats source;
};

struct Summary {
    Uint contextGeneration = 0;
    Uint64 calls = 0;
    Uint64 imageCalls = 0;
    Uint64 subImageCalls = 0;
    Uint64 eligible = 0;
    Uint64 staged = 0;
    Uint64 nativeFallback = 0;
    Uint64 readErrors = 0;
    Uint64 uploadErrors = 0;
    Uint64 sourceNontransparent = 0;
    Uint64 sourceRgbAlphaZero = 0;
    Uint64 sourceAllZero = 0;
    Uint64 shadowUpdated = 0;
    Uint64 modelLookups = 0;
    Uint64 modelMatches = 0;
    Uint64 modelMissing = 0;
    Uint64 modelLogs = 0;
};

inline std::mutex g_mutex;
inline UnorderedMap<Uint64, Record> g_records;
inline UnorderedMap<Uint64, Uint64> g_reportedSequence;
inline Summary g_summary;
inline Uint64 g_sequence = 0;

inline void RecordCopy(Uint64 lifetime, Uint frontTexture, Uint nativeTexture, CopyKind kind,
                       Int level, Int srcX, Int srcY, Int dstX, Int dstY, Int width, Int height,
                       Uint readFbo, Uint readBuffer, Uint readFboStatus, Bool eligible, Bool staged,
                       Bool shadowUpdated, Uint readError, Uint uploadError, const PixelStats& source) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Record& record = g_records[lifetime];
    record.lifetime = lifetime;
    record.frontTexture = frontTexture;
    record.nativeTexture = nativeTexture;
    record.sequence = ++g_sequence;
    record.lastKind = kind;
    record.level = level;
    record.srcX = srcX;
    record.srcY = srcY;
    record.dstX = dstX;
    record.dstY = dstY;
    record.width = width;
    record.height = height;
    record.readFbo = readFbo;
    record.readBuffer = readBuffer;
    record.readFboStatus = readFboStatus;
    record.eligible = eligible;
    record.staged = staged;
    record.shadowUpdated = shadowUpdated;
    record.readError = readError;
    record.uploadError = uploadError;
    record.source = source;
    ++g_summary.calls;
    if (kind == CopyKind::Image2D) {
        ++record.imageCalls;
        ++g_summary.imageCalls;
    } else if (kind == CopyKind::SubImage2D) {
        ++record.subImageCalls;
        ++g_summary.subImageCalls;
    }
    if (eligible) ++g_summary.eligible;
    if (staged) ++g_summary.staged;
    else ++g_summary.nativeFallback;
    if (readError != 0) ++g_summary.readErrors;
    if (uploadError != 0) ++g_summary.uploadErrors;
    if (shadowUpdated) ++g_summary.shadowUpdated;
    if (source.valid) {
        if (source.zeroRgba == source.pixels) ++g_summary.sourceAllZero;
        else if (source.alphaNonzero == 0 && source.rgbNonzero != 0) ++g_summary.sourceRgbAlphaZero;
        else if (source.alphaNonzero != 0) ++g_summary.sourceNontransparent;
    }
}

// 0 = unchanged since the prior model log, 1 = matched copy, 2 = no copy record.
inline Int TakeModelSnapshot(Uint64 lifetime, Record& out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_summary.modelLookups;
    const auto found = g_records.find(lifetime);
    const Uint64 sequence = found == g_records.end() ? 0 : found->second.sequence;
    const auto reported = g_reportedSequence.find(lifetime);
    if (reported != g_reportedSequence.end() && reported->second == sequence) return 0;
    g_reportedSequence[lifetime] = sequence;
    ++g_summary.modelLogs;
    if (found == g_records.end()) {
        ++g_summary.modelMissing;
        return 2;
    }
    ++g_summary.modelMatches;
    out = found->second;
    return 1;
}

inline void OnContextReady(Uint generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_summary.contextGeneration = generation;
    g_reportedSequence.clear();
}

inline Summary GetSummary() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_summary;
}

} // namespace MobilePZ::PZF10
