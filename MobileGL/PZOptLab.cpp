// MobileGL-PZ OPT-LAB runtime selector and bounded proof telemetry.

#include "PZOptLab.h"
#include "Includes.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>

#ifndef MOBILEPZ_OPT_LAB_BUILD_VARIANT
#define MOBILEPZ_OPT_LAB_BUILD_VARIANT "UNKNOWN"
#endif
#ifndef MOBILEPZ_OPT_LAB_BUILD_ID
#define MOBILEPZ_OPT_LAB_BUILD_ID "UNSTAMPED"
#endif

namespace MobileGL::PZOptLab {
    namespace {
        constexpr std::array<Optimization, 29> kOptimizations = {
            Optimization::Perf002, Optimization::Perf003, Optimization::Perf004R, Optimization::Perf005,
            Optimization::Perf006, Optimization::Perf007, Optimization::Perf008,  Optimization::Perf009,
            Optimization::Perf010, Optimization::Perf011, Optimization::Perf012,  Optimization::Perf013,
            Optimization::Perf014, Optimization::Perf015, Optimization::Perf016,  Optimization::Perf017,
            Optimization::Perf018, Optimization::Perf019, Optimization::Perf020, Optimization::Perf021A,
            Optimization::Perf021B, Optimization::Perf021C, Optimization::Perf021D, Optimization::Perf021E,
            Optimization::Perf022A, Optimization::Perf022B, Optimization::Perf022C, Optimization::Perf022D,
            Optimization::Perf022E,
        };
        constexpr std::uint32_t Bit(Optimization value) {
            return static_cast<std::uint32_t>(value);
        }
        constexpr std::uint32_t kAll =
            Bit(Optimization::Perf002) | Bit(Optimization::Perf003) | Bit(Optimization::Perf004R) |
            Bit(Optimization::Perf005) | Bit(Optimization::Perf006) | Bit(Optimization::Perf007) |
            Bit(Optimization::Perf008) | Bit(Optimization::Perf009) | Bit(Optimization::Perf010) |
            Bit(Optimization::Perf011) | Bit(Optimization::Perf012) | Bit(Optimization::Perf013) |
            Bit(Optimization::Perf014) | Bit(Optimization::Perf015) | Bit(Optimization::Perf016) |
            Bit(Optimization::Perf017) | Bit(Optimization::Perf018) | Bit(Optimization::Perf019) |
            Bit(Optimization::Perf020) | Bit(Optimization::Perf021A) | Bit(Optimization::Perf021B) |
            Bit(Optimization::Perf021C) | Bit(Optimization::Perf021D) | Bit(Optimization::Perf021E) |
            Bit(Optimization::Perf022A) | Bit(Optimization::Perf022B) | Bit(Optimization::Perf022C) |
            Bit(Optimization::Perf022D) | Bit(Optimization::Perf022E);
        constexpr std::uint32_t kAccepted = Bit(Optimization::Perf002) | Bit(Optimization::Perf003);
        constexpr std::uint32_t kTelemetry =
            Bit(Optimization::Perf006) | Bit(Optimization::Perf009) | Bit(Optimization::Perf018);
        constexpr std::uint32_t kSafe = kAccepted | Bit(Optimization::Perf005) | Bit(Optimization::Perf007) |
                                        Bit(Optimization::Perf011) | Bit(Optimization::Perf013) |
                                        Bit(Optimization::Perf014) | Bit(Optimization::Perf015) |
                                        Bit(Optimization::Perf016) | Bit(Optimization::Perf017) |
                                        Bit(Optimization::Perf019) | Bit(Optimization::Perf020) |
                                        Bit(Optimization::Perf021A) | Bit(Optimization::Perf021B) |
                                        Bit(Optimization::Perf021C) | Bit(Optimization::Perf021D) |
                                        Bit(Optimization::Perf022A) | Bit(Optimization::Perf022B) |
                                        Bit(Optimization::Perf022C) | Bit(Optimization::Perf022D) |
                                        Bit(Optimization::Perf022E);
        // Proof must establish that a path ran, but it must stop touching atomics
        // during the measured steady state. Two thousand samples are ample for a
        // lab verdict and normally complete during benchmark warm-up.
        constexpr std::uint32_t kProofCallLimit = 2048;
        constexpr const char* kDefaultSelectorFile = "/data/user/0/com.zomdroid.mglpz1/files/mglpz-opt-set.txt";
        constexpr const char* kDefaultProofFile = "/data/user/0/com.zomdroid.mglpz1/files/mglpz-opt-proof.log";

        struct PathCounter {
            std::atomic<std::uint32_t> calls{0};
            std::atomic<std::uint64_t> touched{0};
            std::atomic<std::uint64_t> eligible{0};
            std::atomic<std::uint64_t> fallback{0};
            std::atomic<bool> summaryWritten{false};
        };

        struct EventCounter {
            std::atomic<std::uint32_t> calls{0};
            std::atomic<std::uint64_t> amount{0};
            std::atomic<std::uint64_t> maxAmount{0};
            std::atomic<std::uint64_t> durationNs{0};
            std::atomic<std::uint64_t> failures{0};
            std::atomic<bool> summaryWritten{false};
        };

        std::once_flag g_initializeOnce;
        std::atomic<bool> g_initialized{false};
        std::uint32_t g_mask = 0;
        std::uint32_t g_autoAddedMask = 0;
        bool g_valid = true;
        std::string g_requested = "ALL_OFF";
        std::string g_resolved = "none";
        std::string g_source = "default";
        std::string g_error = "none";
        std::string g_proofPath = kDefaultProofFile;
        std::mutex g_outputMutex;
        std::array<PathCounter, kOptimizations.size()> g_pathCounters;
        std::array<EventCounter, static_cast<std::size_t>(Event::Count)> g_eventCounters;

        const char* IdName(Optimization optimization) {
            switch (optimization) {
            case Optimization::Perf002:
                return "002";
            case Optimization::Perf003:
                return "003";
            case Optimization::Perf004R:
                return "004R";
            case Optimization::Perf005:
                return "005";
            case Optimization::Perf006:
                return "006";
            case Optimization::Perf007:
                return "007";
            case Optimization::Perf008:
                return "008";
            case Optimization::Perf009:
                return "009";
            case Optimization::Perf010:
                return "010";
            case Optimization::Perf011:
                return "011";
            case Optimization::Perf012:
                return "012";
            case Optimization::Perf013:
                return "013";
            case Optimization::Perf014:
                return "014";
            case Optimization::Perf015:
                return "015";
            case Optimization::Perf016:
                return "016";
            case Optimization::Perf017:
                return "017";
            case Optimization::Perf018:
                return "018";
            case Optimization::Perf019:
                return "019";
            case Optimization::Perf020:
                return "020";
            case Optimization::Perf021A:
                return "021A";
            case Optimization::Perf021B:
                return "021B";
            case Optimization::Perf021C:
                return "021C";
            case Optimization::Perf021D:
                return "021D";
            case Optimization::Perf021E:
                return "021E";
            case Optimization::Perf022A:
                return "022A";
            case Optimization::Perf022B:
                return "022B";
            case Optimization::Perf022C:
                return "022C";
            case Optimization::Perf022D:
                return "022D";
            case Optimization::Perf022E:
                return "022E";
            }
            return "unknown";
        }

        const char* EventName(Event event) {
            switch (event) {
            case Event::FinishFallback:
                return "finish_fallback";
            case Event::FinishReadback:
                return "finish_readback";
            case Event::FenceWait:
                return "fence_wait";
            case Event::AppReadback:
                return "app_readback";
            case Event::NativeReadback:
                return "native_readback";
            case Event::ClientAttribUpload:
                return "client_attrib_upload";
            case Event::FboInvalidateCall:
                return "fbo_invalidate_call";
            case Event::FboInvalidateForward:
                return "fbo_invalidate_forward";
            case Event::ProgramCacheLookup:
                return "program_cache_lookup";
            case Event::ProgramCacheHit:
                return "program_cache_hit";
            case Event::ProgramCacheMiss:
                return "program_cache_miss";
            case Event::ProgramCacheStore:
                return "program_cache_store";
            case Event::BufferRespecify:
                return "buffer_respecify";
            case Event::BufferIdSwap:
                return "buffer_id_swap";
            case Event::ImageUnitBinding:
                return "image_unit_binding";
            case Event::S3tcUpload:
                return "s3tc_upload";
            case Event::UboRingAllocate:
                return "ubo_ring_allocate";
            case Event::UboRingGrow:
                return "ubo_ring_grow";
            case Event::UboRingPressure:
                return "ubo_ring_pressure";
            case Event::UboRingHighWater:
                return "ubo_ring_high_water";
            case Event::TextureUpload:
                return "texture_upload";
            case Event::ShaderSourceCacheLookup:
                return "shader_source_cache_lookup";
            case Event::ShaderSourceCacheHit:
                return "shader_source_cache_hit";
            case Event::ShaderSourceCacheMiss:
                return "shader_source_cache_miss";
            case Event::ShaderSourceCacheStore:
                return "shader_source_cache_store";
            case Event::DrawPrepareTotal:
                return "draw_prepare_total";
            case Event::DrawPrepareSetup:
                return "draw_prepare_setup";
            case Event::DrawPrepareBufferSync:
                return "draw_prepare_buffer_sync";
            case Event::DrawPrepareVaoSync:
                return "draw_prepare_vao_sync";
            case Event::DrawPrepareTextureSync:
                return "draw_prepare_texture_sync";
            case Event::DrawPrepareFboSync:
                return "draw_prepare_fbo_sync";
            case Event::DrawPrepareProgramSync:
                return "draw_prepare_program_sync";
            case Event::DrawPrepareRenderState:
                return "draw_prepare_render_state";
            case Event::DrawPrepareFboBind:
                return "draw_prepare_fbo_bind";
            case Event::DrawPrepareVaoBind:
                return "draw_prepare_vao_bind";
            case Event::DrawPrepareCurrentAttribs:
                return "draw_prepare_current_attribs";
            case Event::DrawPrepareTextureBind:
                return "draw_prepare_texture_bind";
            case Event::DrawPrepareProgramResources:
                return "draw_prepare_program_resources";
            case Event::DrawPrepareXfbStart:
                return "draw_prepare_xfb_start";
            case Event::PboUploadStaged:
                return "pbo_upload_staged";
            case Event::PboUploadFallback:
                return "pbo_upload_fallback";
            case Event::PboUploadRingGrow:
                return "pbo_upload_ring_grow";
            case Event::PboUploadRingHighWater:
                return "pbo_upload_ring_high_water";
            case Event::Count:
                break;
            }
            return "unknown";
        }

        std::size_t OptIndex(Optimization optimization) {
            for (std::size_t i = 0; i < kOptimizations.size(); ++i) {
                if (kOptimizations[i] == optimization) return i;
            }
            return 0;
        }

        std::string Trim(std::string value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return {};
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        std::string Lower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        std::string ReadSelectorFile(const char* path) {
            if (path == nullptr || *path == '\0') return {};
            FILE* file = std::fopen(path, "r");
            if (!file) return {};
            std::array<char, 4097> buffer{};
            const std::size_t count = std::fread(buffer.data(), 1, buffer.size() - 1, file);
            std::fclose(file);
            buffer[count] = '\0';
            std::string value = Trim(buffer.data());
            const auto equals = value.find('=');
            if (equals != std::string::npos && value.find('\n') == std::string::npos) {
                value = Trim(value.substr(equals + 1));
            }
            return value;
        }

        std::string MaskToString(std::uint32_t mask) {
            if (mask == 0) return "none";
            std::string result;
            for (Optimization optimization : kOptimizations) {
                if ((mask & Bit(optimization)) == 0) continue;
                if (!result.empty()) result += ',';
                result += IdName(optimization);
            }
            return result;
        }

        bool TokenToOptimization(std::string token, Optimization& out) {
            token = Lower(Trim(std::move(token)));
            if (token.rfind("perf-", 0) == 0)
                token.erase(0, 5);
            else if (token.rfind("perf", 0) == 0)
                token.erase(0, 4);
            if (token == "2" || token == "02" || token == "002" || token == "texture-sync") {
                out = Optimization::Perf002;
            } else if (token == "3" || token == "03" || token == "003" || token == "texture-bind") {
                out = Optimization::Perf003;
            } else if (token == "4" || token == "04" || token == "004" || token == "004r" || token == "s3tc" ||
                       token == "native-s3tc") {
                out = Optimization::Perf004R;
            } else if (token == "5" || token == "05" || token == "005" || token == "sampler-bind") {
                out = Optimization::Perf005;
            } else if (token == "6" || token == "06" || token == "006" || token == "drain-telemetry") {
                out = Optimization::Perf006;
            } else if (token == "7" || token == "07" || token == "007" || token == "program-cache") {
                out = Optimization::Perf007;
            } else if (token == "8" || token == "08" || token == "008" || token == "fbo-invalidate") {
                out = Optimization::Perf008;
            } else if (token == "9" || token == "09" || token == "009" || token == "staging-telemetry") {
                out = Optimization::Perf009;
            } else if (token == "10" || token == "010" || token == "buffer-swap") {
                out = Optimization::Perf010;
            } else if (token == "11" || token == "011" || token == "image-mask") {
                out = Optimization::Perf011;
            } else if (token == "12" || token == "012" || token == "ssbo-mask") {
                out = Optimization::Perf012;
            } else if (token == "13" || token == "013" || token == "incremental-gc" || token == "registry-gc") {
                out = Optimization::Perf013;
            } else if (token == "14" || token == "014" || token == "vertex-mask" || token == "used-attrib-mask") {
                out = Optimization::Perf014;
            } else if (token == "15" || token == "015" || token == "ubo-mark-queue") {
                out = Optimization::Perf015;
            } else if (token == "16" || token == "016" || token == "essl-cache" || token == "shader-source-cache") {
                out = Optimization::Perf016;
            } else if (token == "17" || token == "017" || token == "renderbuffer-shadow" || token == "rbo-shadow") {
                out = Optimization::Perf017;
            } else if (token == "18" || token == "018" || token == "draw-telemetry" || token == "prepare-telemetry") {
                out = Optimization::Perf018;
            } else if (token == "19" || token == "019" || token == "attrib-shadow" ||
                       token == "current-attrib-shadow") {
                out = Optimization::Perf019;
            } else if (token == "20" || token == "020" || token == "texture-clean-epoch" ||
                       token == "texture-draw-epoch") {
                out = Optimization::Perf020;
            } else if (token == "21a" || token == "021a" || token == "texture-shadow-epoch") {
                out = Optimization::Perf021A;
            } else if (token == "21b" || token == "021b" || token == "sampler-shadow-epoch") {
                out = Optimization::Perf021B;
            } else if (token == "21c" || token == "021c" || token == "ubo-replay-memo") {
                out = Optimization::Perf021C;
            } else if (token == "21d" || token == "021d" || token == "ssbo-fused-worklist") {
                out = Optimization::Perf021D;
            } else if (token == "21e" || token == "021e" || token == "pbo-upload-ring") {
                out = Optimization::Perf021E;
            } else if (token == "22a" || token == "022a" || token == "direct-subimage-shadow") {
                out = Optimization::Perf022A;
            } else if (token == "22b" || token == "022b" || token == "direct-image-shadow") {
                out = Optimization::Perf022B;
            } else if (token == "22c" || token == "022c" || token == "auto-mipmap-latch") {
                out = Optimization::Perf022C;
            } else if (token == "22d" || token == "022d" || token == "contained-dirty-fastpath") {
                out = Optimization::Perf022D;
            } else if (token == "22e" || token == "022e" || token == "dsa-subimage-region") {
                out = Optimization::Perf022E;
            } else {
                return false;
            }
            return true;
        }

        bool ParsePreset(const std::string& raw, std::uint32_t& mask) {
            const std::string preset = Lower(Trim(raw));
            if (preset.empty() || preset == "all_off" || preset == "off" || preset == "none" || preset == "0") {
                mask = 0;
            } else if (preset == "cumulative_accepted" || preset == "accepted") {
                mask = kAccepted;
            } else if (preset == "telemetry_only" || preset == "telemetry") {
                mask = kTelemetry;
            } else if (preset == "all_safe_on" || preset == "all_perf_on" || preset == "safe" || preset == "perf") {
                mask = kSafe;
            } else if (preset == "all_experimental_on" || preset == "all") {
                mask = kAll;
            } else {
                return false;
            }
            return true;
        }

        bool ParseSelector(const std::string& raw, std::uint32_t& mask, std::string& error) {
            mask = 0;
            std::string lowered = Lower(Trim(raw));
            if (ParsePreset(lowered, mask)) return true;
            for (char& ch : lowered) {
                if (ch == ';' || std::isspace(static_cast<unsigned char>(ch))) ch = ',';
            }
            std::size_t cursor = 0;
            while (cursor <= lowered.size()) {
                const std::size_t comma = lowered.find(',', cursor);
                std::string token =
                    Trim(lowered.substr(cursor, comma == std::string::npos ? std::string::npos : comma - cursor));
                if (!token.empty()) {
                    bool disable = token.front() == '-';
                    if (disable || token.front() == '+') token.erase(0, 1);
                    Optimization optimization{};
                    if (!TokenToOptimization(token, optimization)) {
                        error = "unknown_token:" + token;
                        return false;
                    }
                    if (disable)
                        mask &= ~Bit(optimization);
                    else
                        mask |= Bit(optimization);
                }
                if (comma == std::string::npos) break;
                cursor = comma + 1;
            }
            return true;
        }

        bool ParseBool(const char* raw, bool& value) {
            if (!raw || !*raw) return false;
            const std::string text = Lower(Trim(raw));
            if (text == "1" || text == "on" || text == "true" || text == "yes") {
                value = true;
                return true;
            }
            if (text == "0" || text == "off" || text == "false" || text == "no") {
                value = false;
                return true;
            }
            return false;
        }

        void AppendProof(const std::string& line) {
            std::lock_guard<std::mutex> lock(g_outputMutex);
            MGLOG_F("%s", line.c_str());
            FILE* file = std::fopen(g_proofPath.c_str(), "a");
            if (!file) return;
            std::fputs(line.c_str(), file);
            std::fputc('\n', file);
            std::fflush(file);
            std::fclose(file);
        }

        void ResolveDependencies(std::uint32_t& mask) {
            const std::uint32_t before = mask;
            if ((mask & Bit(Optimization::Perf003)) != 0) mask |= Bit(Optimization::Perf002);
            if ((mask & Bit(Optimization::Perf005)) != 0) {
                mask |= Bit(Optimization::Perf002) | Bit(Optimization::Perf003);
            }
            g_autoAddedMask = mask & ~before;
        }

        bool EventEnabled(Event event) {
            switch (event) {
            case Event::FinishFallback:
            case Event::FinishReadback:
            case Event::FenceWait:
            case Event::AppReadback:
            case Event::NativeReadback:
            case Event::UboRingAllocate:
            case Event::UboRingGrow:
            case Event::UboRingPressure:
            case Event::UboRingHighWater:
                return Enabled(Optimization::Perf006);
            case Event::ClientAttribUpload:
            case Event::TextureUpload:
                return Enabled(Optimization::Perf006) || Enabled(Optimization::Perf009);
            case Event::FboInvalidateCall:
                return Enabled(Optimization::Perf006) || Enabled(Optimization::Perf008);
            case Event::FboInvalidateForward:
                return Enabled(Optimization::Perf008);
            case Event::ProgramCacheLookup:
            case Event::ProgramCacheHit:
            case Event::ProgramCacheMiss:
            case Event::ProgramCacheStore:
                return Enabled(Optimization::Perf007);
            case Event::BufferRespecify:
            case Event::BufferIdSwap:
                return Enabled(Optimization::Perf006) || Enabled(Optimization::Perf010);
            case Event::ImageUnitBinding:
                return Enabled(Optimization::Perf011);
            case Event::S3tcUpload:
                return Enabled(Optimization::Perf004R);
            case Event::ShaderSourceCacheLookup:
            case Event::ShaderSourceCacheHit:
            case Event::ShaderSourceCacheMiss:
            case Event::ShaderSourceCacheStore:
                return Enabled(Optimization::Perf016);
            case Event::DrawPrepareTotal:
            case Event::DrawPrepareSetup:
            case Event::DrawPrepareBufferSync:
            case Event::DrawPrepareVaoSync:
            case Event::DrawPrepareTextureSync:
            case Event::DrawPrepareFboSync:
            case Event::DrawPrepareProgramSync:
            case Event::DrawPrepareRenderState:
            case Event::DrawPrepareFboBind:
            case Event::DrawPrepareVaoBind:
            case Event::DrawPrepareCurrentAttribs:
            case Event::DrawPrepareTextureBind:
            case Event::DrawPrepareProgramResources:
            case Event::DrawPrepareXfbStart:
                return Enabled(Optimization::Perf018);
            case Event::PboUploadStaged:
            case Event::PboUploadFallback:
            case Event::PboUploadRingGrow:
            case Event::PboUploadRingHighWater:
                return Enabled(Optimization::Perf021E);
            case Event::Count:
                return false;
            }
            return false;
        }

        bool EventBelongsTo(Event event, Optimization optimization) {
            switch (optimization) {
            case Optimization::Perf004R:
                return event == Event::S3tcUpload;
            case Optimization::Perf006:
                return event == Event::FinishFallback || event == Event::FinishReadback || event == Event::FenceWait ||
                       event == Event::AppReadback || event == Event::NativeReadback ||
                       event == Event::ClientAttribUpload || event == Event::FboInvalidateCall ||
                       event == Event::BufferRespecify || event == Event::UboRingAllocate ||
                       event == Event::UboRingGrow || event == Event::UboRingPressure ||
                       event == Event::UboRingHighWater || event == Event::TextureUpload;
            case Optimization::Perf007:
                return event >= Event::ProgramCacheLookup && event <= Event::ProgramCacheStore;
            case Optimization::Perf008:
                return event == Event::FboInvalidateCall || event == Event::FboInvalidateForward;
            case Optimization::Perf009:
                return event == Event::ClientAttribUpload || event == Event::TextureUpload;
            case Optimization::Perf010:
                return event == Event::BufferRespecify || event == Event::BufferIdSwap;
            case Optimization::Perf011:
                return event == Event::ImageUnitBinding;
            case Optimization::Perf012:
                return false;
            case Optimization::Perf013:
            case Optimization::Perf014:
            case Optimization::Perf015:
                return false;
            case Optimization::Perf016:
                return event >= Event::ShaderSourceCacheLookup && event <= Event::ShaderSourceCacheStore;
            case Optimization::Perf017:
                return false;
            case Optimization::Perf018:
                return event >= Event::DrawPrepareTotal && event <= Event::DrawPrepareXfbStart;
            case Optimization::Perf019:
            case Optimization::Perf020:
            case Optimization::Perf021A:
            case Optimization::Perf021B:
            case Optimization::Perf021C:
            case Optimization::Perf021D:
            case Optimization::Perf022A:
            case Optimization::Perf022B:
            case Optimization::Perf022C:
            case Optimization::Perf022D:
            case Optimization::Perf022E:
                return false;
            case Optimization::Perf021E:
                return event >= Event::PboUploadStaged && event <= Event::PboUploadRingHighWater;
            case Optimization::Perf002:
            case Optimization::Perf003:
            case Optimization::Perf005:
                return false;
            }
            return false;
        }

        void WritePathSummary(Optimization optimization, PathCounter& counter, bool bounded) {
            bool expected = false;
            if (!counter.summaryWritten.compare_exchange_strong(expected, true)) return;
            const std::uint64_t touched = counter.touched.load(std::memory_order_relaxed);
            const std::uint64_t eligible = counter.eligible.load(std::memory_order_relaxed);
            const std::uint64_t avoided = touched >= eligible ? touched - eligible : 0;
            AppendProof(std::format("MGLPZ_OPT_SUMMARY schema=5 id={} calls={} touched={} eligible={} "
                                    "work_avoided={} fallback_calls={} bounded={} status=ACTIVE_AND_EXERCISED",
                                    IdName(optimization), counter.calls.load(std::memory_order_relaxed), touched,
                                    eligible, avoided, counter.fallback.load(std::memory_order_relaxed),
                                    bounded ? 1 : 0));
        }

        void WriteEventSummary(Event event, EventCounter& counter, bool bounded) {
            bool expected = false;
            if (!counter.summaryWritten.compare_exchange_strong(expected, true)) return;
            AppendProof(std::format("MGLPZ_METRIC_SUMMARY schema=5 event={} calls={} amount={} max_amount={} "
                                    "duration_ns={} failures={} bounded={}",
                                    EventName(event), counter.calls.load(std::memory_order_relaxed),
                                    counter.amount.load(std::memory_order_relaxed),
                                    counter.maxAmount.load(std::memory_order_relaxed),
                                    counter.durationNs.load(std::memory_order_relaxed),
                                    counter.failures.load(std::memory_order_relaxed), bounded ? 1 : 0));
        }
    } // namespace

    void Initialize() {
        if (g_initialized.load(std::memory_order_acquire)) return;
        std::call_once(g_initializeOnce, [] {
            const char* proofPath = std::getenv("MOBILEGL_PZ_PROOF_FILE");
            if (!proofPath || !*proofPath) proofPath = std::getenv("MGLPZ_PROOF_FILE");
            if (proofPath && *proofPath) g_proofPath = proofPath;

            std::uint32_t parsedMask = 0;
            const char* preset = std::getenv("MOBILEGL_PZ_OPT_PRESET");
            if (!preset || !*preset) preset = std::getenv("MGLPZ_OPT_PRESET");
            if (preset && *preset) {
                g_requested = Trim(preset);
                g_source = "preset_env";
                if (!ParsePreset(preset, parsedMask)) {
                    g_valid = false;
                    g_error = "unknown_preset:" + Trim(preset);
                }
            }

            const char* selector = std::getenv("MOBILEGL_PZ_OPT_SET");
            if (!selector || !*selector) selector = std::getenv("MGLPZ_OPT_SET");
            std::string sidecar;
            if ((!selector || !*selector) && (!preset || !*preset)) {
                const char* configuredPath = std::getenv("MOBILEGL_PZ_OPT_FILE");
                const char* selectorPath = configuredPath && *configuredPath ? configuredPath : kDefaultSelectorFile;
                sidecar = ReadSelectorFile(selectorPath);
                if (!sidecar.empty()) selector = sidecar.c_str();
            }
            if (selector && *selector) {
                g_requested = Trim(selector);
                g_source = sidecar.empty() ? "set_env" : "sidecar";
                std::string parseError;
                std::uint32_t selectorMask = 0;
                if (!ParseSelector(selector, selectorMask, parseError)) {
                    g_valid = false;
                    g_error = parseError;
                } else {
                    parsedMask = selectorMask;
                }
            }

            constexpr std::array<std::pair<const char*, Optimization>, 29> variables = {{
                {"MOBILEGL_PZ_OPT_002", Optimization::Perf002},   {"MOBILEGL_PZ_OPT_003", Optimization::Perf003},
                {"MOBILEGL_PZ_OPT_004R", Optimization::Perf004R}, {"MOBILEGL_PZ_OPT_005", Optimization::Perf005},
                {"MOBILEGL_PZ_OPT_006", Optimization::Perf006},   {"MOBILEGL_PZ_OPT_007", Optimization::Perf007},
                {"MOBILEGL_PZ_OPT_008", Optimization::Perf008},   {"MOBILEGL_PZ_OPT_009", Optimization::Perf009},
                {"MOBILEGL_PZ_OPT_010", Optimization::Perf010},   {"MOBILEGL_PZ_OPT_011", Optimization::Perf011},
                {"MOBILEGL_PZ_OPT_012", Optimization::Perf012},   {"MOBILEGL_PZ_OPT_013", Optimization::Perf013},
                {"MOBILEGL_PZ_OPT_014", Optimization::Perf014},   {"MOBILEGL_PZ_OPT_015", Optimization::Perf015},
                {"MOBILEGL_PZ_OPT_016", Optimization::Perf016},   {"MOBILEGL_PZ_OPT_017", Optimization::Perf017},
                {"MOBILEGL_PZ_OPT_018", Optimization::Perf018},   {"MOBILEGL_PZ_OPT_019", Optimization::Perf019},
                {"MOBILEGL_PZ_OPT_020", Optimization::Perf020},
                {"MOBILEGL_PZ_OPT_021A", Optimization::Perf021A},
                {"MOBILEGL_PZ_OPT_021B", Optimization::Perf021B},
                {"MOBILEGL_PZ_OPT_021C", Optimization::Perf021C},
                {"MOBILEGL_PZ_OPT_021D", Optimization::Perf021D},
                {"MOBILEGL_PZ_OPT_021E", Optimization::Perf021E},
                {"MOBILEGL_PZ_OPT_022A", Optimization::Perf022A},
                {"MOBILEGL_PZ_OPT_022B", Optimization::Perf022B},
                {"MOBILEGL_PZ_OPT_022C", Optimization::Perf022C},
                {"MOBILEGL_PZ_OPT_022D", Optimization::Perf022D},
                {"MOBILEGL_PZ_OPT_022E", Optimization::Perf022E},
            }};
            for (const auto& [name, optimization] : variables) {
                const char* raw = std::getenv(name);
                if (!raw || !*raw) continue;
                bool value = false;
                if (!ParseBool(raw, value)) {
                    g_valid = false;
                    g_error = std::string("invalid_boolean:") + name;
                    break;
                }
                g_source += "+individual";
                if (value)
                    parsedMask |= Bit(optimization);
                else
                    parsedMask &= ~Bit(optimization);
            }

            if (g_valid) ResolveDependencies(parsedMask);
            g_mask = g_valid ? parsedMask : 0;
            g_resolved = MaskToString(g_mask);

            {
                std::lock_guard<std::mutex> lock(g_outputMutex);
                FILE* file = std::fopen(g_proofPath.c_str(), "w");
                if (file) std::fclose(file);
            }
            AppendProof(
                std::format("MGLPZ_OPT_CONFIG schema=5 build=OPT-LAB-V3-022 build_id={} variant={} requested={} "
                            "resolved={} auto_added={} valid={} source={} error={} "
                            "supported=002,003,004R,005,006,007,008,009,010,011,012,013,014,015,016,017,018,019,020,021A,021B,021C,021D,021E,022A,022B,022C,022D,022E "
                            "old_perf004=excluded",
                            MOBILEPZ_OPT_LAB_BUILD_ID, MOBILEPZ_OPT_LAB_BUILD_VARIANT, g_requested, g_resolved,
                            MaskToString(g_autoAddedMask), g_valid ? 1 : 0, g_source, g_error));
            if (!g_valid) {
                AppendProof("MGLPZ_OPT_FAIL_CLOSED schema=5 resolved=none reason=" + g_error);
            }
            g_initialized.store(true, std::memory_order_release);
        });
    }

    bool Enabled(Optimization optimization) {
#ifdef MOBILEPZ_OPT_LAB
        Initialize();
        return (g_mask & Bit(optimization)) != 0;
#else
        std::uint32_t compiledMask = 0;
#ifdef MOBILEPZ_PERF002_PROGRAM_AWARE_TEXTURE_SYNC
        compiledMask |= Bit(Optimization::Perf002);
#endif
#ifdef MOBILEPZ_PERF003_PROGRAM_AWARE_TEXTURE_BINDING
        compiledMask |= Bit(Optimization::Perf003);
#endif
#ifdef MOBILEPZ_PERF005_PROGRAM_AWARE_SAMPLER_BINDING
        compiledMask |= Bit(Optimization::Perf005);
#endif
        return (compiledMask & Bit(optimization)) != 0;
#endif
    }

    bool ConfigValid() {
        Initialize();
        return g_valid;
    }
    const char* ActiveSet() {
        Initialize();
        return g_resolved.c_str();
    }
    const char* RequestedSet() {
        Initialize();
        return g_requested.c_str();
    }
    const char* BuildVariant() {
        return MOBILEPZ_OPT_LAB_BUILD_VARIANT;
    }

    bool BeginDrawPrepareSample() {
#ifdef MOBILEPZ_OPT_LAB
        if (!Enabled(Optimization::Perf018)) return false;
        constexpr std::uint32_t kStride = 64;
        thread_local std::uint32_t drawOrdinal = 0;
        thread_local std::uint32_t sampleCount = 0;
        const bool selected = (drawOrdinal++ % kStride) == 0;
        if (!selected || sampleCount >= kProofCallLimit) return false;
        ++sampleCount;
        return true;
#else
        return false;
#endif
    }

    void RecordPath(Optimization optimization, std::uint32_t touched, std::uint32_t eligible, bool fallback) {
#ifdef MOBILEPZ_OPT_LAB
        if (!Enabled(optimization)) return;
        PathCounter& counter = g_pathCounters[OptIndex(optimization)];
        if (counter.summaryWritten.load(std::memory_order_relaxed)) return;
        const std::uint32_t previous = counter.calls.fetch_add(1, std::memory_order_relaxed);
        if (previous >= kProofCallLimit) return;
        counter.touched.fetch_add(touched, std::memory_order_relaxed);
        counter.eligible.fetch_add(std::min(touched, eligible), std::memory_order_relaxed);
        if (fallback) counter.fallback.fetch_add(1, std::memory_order_relaxed);
        if (previous == 0) {
            AppendProof(std::format("MGLPZ_OPT_HIT schema=5 id={} status=ACTIVE_AND_EXERCISED first_touched={} "
                                    "first_eligible={} fallback={}",
                                    IdName(optimization), touched, std::min(touched, eligible), fallback ? 1 : 0));
        }
        if (previous + 1 == kProofCallLimit) WritePathSummary(optimization, counter, true);
#else
        (void)optimization;
        (void)touched;
        (void)eligible;
        (void)fallback;
#endif
    }

    void RecordEvent(Event event, std::uint64_t amount, std::uint64_t durationNs, bool failure) {
#ifdef MOBILEPZ_OPT_LAB
        if (event == Event::Count || !EventEnabled(event)) return;
        EventCounter& counter = g_eventCounters[static_cast<std::size_t>(event)];
        if (counter.summaryWritten.load(std::memory_order_relaxed)) return;
        const std::uint32_t previous = counter.calls.fetch_add(1, std::memory_order_relaxed);
        if (previous >= kProofCallLimit) return;
        counter.amount.fetch_add(amount, std::memory_order_relaxed);
        std::uint64_t observedMax = counter.maxAmount.load(std::memory_order_relaxed);
        while (observedMax < amount && !counter.maxAmount.compare_exchange_weak(
                                           observedMax, amount, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
        counter.durationNs.fetch_add(durationNs, std::memory_order_relaxed);
        if (failure) counter.failures.fetch_add(1, std::memory_order_relaxed);
        if (previous == 0) {
            AppendProof(
                std::format("MGLPZ_METRIC_HIT schema=5 event={} first_amount={} first_duration_ns={} failure={}",
                            EventName(event), amount, durationNs, failure ? 1 : 0));
        }
        if (previous + 1 == kProofCallLimit) WriteEventSummary(event, counter, true);
#else
        (void)event;
        (void)amount;
        (void)durationNs;
        (void)failure;
#endif
    }

    ScopedEvent::~ScopedEvent() {
        const auto elapsed = std::chrono::steady_clock::now() - m_start;
        RecordEvent(m_event, m_amount,
                    static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()),
                    m_failure);
    }

    void EmitIncompleteSummaries() {
#ifdef MOBILEPZ_OPT_LAB
        Initialize();
        for (Optimization optimization : kOptimizations) {
            PathCounter& counter = g_pathCounters[OptIndex(optimization)];
            if (Enabled(optimization) && counter.calls.load(std::memory_order_relaxed) > 0) {
                WritePathSummary(optimization, counter, false);
            }
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(Event::Count); ++i) {
            EventCounter& counter = g_eventCounters[i];
            if (counter.calls.load(std::memory_order_relaxed) > 0) {
                WriteEventSummary(static_cast<Event>(i), counter, false);
            }
        }
        for (Optimization optimization : kOptimizations) {
            if (!Enabled(optimization)) continue;
            bool exercised = g_pathCounters[OptIndex(optimization)].calls.load(std::memory_order_relaxed) > 0;
            for (std::size_t i = 0; !exercised && i < static_cast<std::size_t>(Event::Count); ++i) {
                if (EventBelongsTo(static_cast<Event>(i), optimization) &&
                    g_eventCounters[i].calls.load(std::memory_order_relaxed) > 0) {
                    exercised = true;
                }
            }
            if (!exercised) {
                AppendProof(
                    std::format("MGLPZ_OPT_STATUS schema=5 id={} configured=1 exercised=0 status=ACTIVE_NOT_EXERCISED",
                                IdName(optimization)));
            }
        }
#endif
    }
} // namespace MobileGL::PZOptLab
