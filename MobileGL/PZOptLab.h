// MobileGL-PZ OPT-LAB runtime selector and bounded proof telemetry.
#pragma once

#include <chrono>
#include <cstdint>

namespace MobileGL::PZOptLab {
    enum class Optimization : std::uint32_t {
        Perf002 = 1u << 0,  // program-aware texture sync
        Perf003 = 1u << 1,  // program-aware texture binding
        Perf004R = 1u << 2, // native S3TC passthrough; verified fallback when unsupported
        Perf005 = 1u << 3,  // program-aware sampler binding
        Perf006 = 1u << 4,  // pipeline-drain/readback/staging telemetry
        Perf007 = 1u << 5,  // persistent GLES program-binary cache
        Perf008 = 1u << 6,  // exact application-requested framebuffer invalidate forwarding
        Perf009 = 1u << 7,  // client-attribute/PBO staging telemetry
        Perf010 = 1u << 8,  // GPU-safe dynamic-buffer id swap on respecify
        Perf011 = 1u << 9,  // program-aware image-unit active mask + binding cache
        Perf012 = 1u << 10, // program-aware SSBO binding/writeback mask
        Perf013 = 1u << 11, // incremental/staggered backend-registry garbage collection
        Perf014 = 1u << 12, // program-aware used vertex-buffer mask
        Perf015 = 1u << 13, // O(1)-amortized UBO frame-mark retirement
        Perf016 = 1u << 14, // persistent final ESSL shader-source cache
        Perf017 = 1u << 15, // renderbuffer binding shadow
        Perf018 = 1u << 16, // sampled native PrepareForDraw stage telemetry (diagnostic only)
        Perf019 = 1u << 17, // current generic vertex-attribute driver shadow
        Perf020 = 1u << 18, // texture draw-sync mutation epoch
        Perf021A = 1u << 19, // native texture-binding shadow epoch
        Perf021B = 1u << 20, // native sampler-binding shadow epoch
        Perf021C = 1u << 21, // normal-UBO replay memo
        Perf021D = 1u << 22, // fused SSBO bind/write worklist
        Perf021E = 1u << 23, // experimental persistent PBO texture-upload ring
        Perf022A = 1u << 24, // direct TexSubImage unpack into the CPU shadow
        Perf022B = 1u << 25, // direct TexImage unpack into preallocated CPU shadow
        Perf022C = 1u << 26, // per-texture legacy auto-mipmap latch
        Perf022D = 1u << 27, // contained dirty-region metadata fast path
        Perf022E = 1u << 28, // DSA TextureSubImage2D region-dirty propagation
    };

    enum class Event : std::uint8_t {
        FinishFallback,
        FinishReadback,
        FenceWait,
        AppReadback,
        NativeReadback,
        ClientAttribUpload,
        FboInvalidateCall,
        FboInvalidateForward,
        ProgramCacheLookup,
        ProgramCacheHit,
        ProgramCacheMiss,
        ProgramCacheStore,
        BufferRespecify,
        BufferIdSwap,
        ImageUnitBinding,
        S3tcUpload,
        UboRingAllocate,
        UboRingGrow,
        UboRingPressure,
        UboRingHighWater,
        TextureUpload,
        ShaderSourceCacheLookup,
        ShaderSourceCacheHit,
        ShaderSourceCacheMiss,
        ShaderSourceCacheStore,
        DrawPrepareTotal,
        DrawPrepareSetup,
        DrawPrepareBufferSync,
        DrawPrepareVaoSync,
        DrawPrepareTextureSync,
        DrawPrepareFboSync,
        DrawPrepareProgramSync,
        DrawPrepareRenderState,
        DrawPrepareFboBind,
        DrawPrepareVaoBind,
        DrawPrepareCurrentAttribs,
        DrawPrepareTextureBind,
        DrawPrepareProgramResources,
        DrawPrepareXfbStart,
        PboUploadStaged,
        PboUploadFallback,
        PboUploadRingGrow,
        PboUploadRingHighWater,
        Count,
    };

    void Initialize();
    bool Enabled(Optimization optimization);
    bool ConfigValid();
    const char* ActiveSet();
    const char* RequestedSet();
    const char* BuildVariant();

    // PERF-018 samples one draw out of 64, up to the bounded proof limit. The
    // caller performs no clock reads when this returns false.
    bool BeginDrawPrepareSample();

    // `touched` is baseline work and `eligible` is work retained by the path.
    void RecordPath(Optimization optimization, std::uint32_t touched, std::uint32_t eligible, bool fallback = false);

    // `amount` is bytes/items/pixels as appropriate. Duration is wall time.
    void RecordEvent(Event event, std::uint64_t amount = 1, std::uint64_t durationNs = 0, bool failure = false);

    class ScopedEvent final {
    public:
        explicit ScopedEvent(Event event, std::uint64_t amount = 1)
            : m_event(event), m_amount(amount), m_start(std::chrono::steady_clock::now()) {}
        ~ScopedEvent();

        ScopedEvent(const ScopedEvent&) = delete;
        ScopedEvent& operator=(const ScopedEvent&) = delete;
        void MarkFailure() { m_failure = true; }

    private:
        Event m_event;
        std::uint64_t m_amount;
        std::chrono::steady_clock::time_point m_start;
        bool m_failure = false;
    };

    void EmitIncompleteSummaries();
} // namespace MobileGL::PZOptLab
