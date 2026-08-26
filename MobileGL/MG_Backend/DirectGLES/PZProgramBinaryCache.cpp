// OPT-LAB persistent GLES program-binary cache.

#include "PZProgramBinaryCache.h"
#include "DirectGLES.h"
#include "Managers.h"
#include <PZOptLab.h>
#include <MG_State/GLState/ProgramState/ProgramObject.h>
#include <MG_State/GLState/ProgramState/ShaderObject.h>
#include <MG_Util/BackendLoaders/OpenGL/Loader.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <utility>
#include <vector>

#ifndef MOBILEPZ_OPT_LAB_BUILD_ID
#define MOBILEPZ_OPT_LAB_BUILD_ID "UNSTAMPED"
#endif

namespace MobileGL::MG_Backend::DirectGLES::PZProgramBinaryCache {
    namespace {
        constexpr Uint64 kFnvOffset = 1469598103934665603ull;
        constexpr Uint64 kFnvPrime = 1099511628211ull;
        constexpr Uint32 kSchema = 1;
        constexpr SizeT kMaxEntryBytes = 32u * 1024u * 1024u;
        constexpr SizeT kMaxCacheBytes = 64u * 1024u * 1024u;
        constexpr Uint32 kShaderSourceSchema = 1;
        constexpr SizeT kMaxShaderSourceBytes = 4u * 1024u * 1024u;
        constexpr SizeT kMaxShaderSourceCacheBytes = 32u * 1024u * 1024u;
        constexpr const char* kDefaultCacheDir =
            "/data/user/0/com.zomdroid.mglpz1/files/mglpz-program-cache-v1";
        constexpr const char* kDefaultShaderSourceCacheDir =
            "/data/user/0/com.zomdroid.mglpz1/files/mglpz-essl-cache-v1";

        struct Hasher {
            Uint64 value = kFnvOffset;

            void Add(const void* bytes, SizeT count) {
                const auto* data = static_cast<const Uint8*>(bytes);
                for (SizeT i = 0; i < count; ++i) {
                    value ^= data[i];
                    value *= kFnvPrime;
                }
            }
            template <typename T>
            void AddValue(const T& input) { Add(&input, sizeof(input)); }
            void AddString(const String& input) {
                const Uint64 size = static_cast<Uint64>(input.size());
                AddValue(size);
                if (!input.empty()) Add(input.data(), input.size());
            }
            void AddCString(const char* input) {
                AddString(input ? String(input) : String{});
            }
        };

        struct CacheHeader {
            std::array<char, 8> magic{{'M', 'G', 'L', 'P', 'Z', 'P', 'B', 'C'}};
            Uint32 schema = kSchema;
            Uint32 binaryFormat = 0;
            Uint64 key = 0;
            Uint64 payloadBytes = 0;
            Uint64 payloadHash = 0;
        };

        struct ShaderSourceCacheHeader {
            std::array<char, 8> magic{{'M', 'G', 'L', 'P', 'Z', 'E', 'S', 'C'}};
            Uint32 schema = kShaderSourceSchema;
            Uint32 shaderType = 0;
            Uint64 key = 0;
            Uint64 payloadBytes = 0;
            Uint64 payloadHash = 0;
        };

        const char* CacheDir() {
            const char* configured = std::getenv("MOBILEGL_PZ_PROGRAM_CACHE_DIR");
            return configured && *configured ? configured : kDefaultCacheDir;
        }

        String CachePath(Uint64 key) {
            return std::format("{}/{:016x}.bin", CacheDir(), key);
        }

        const char* ShaderSourceCacheDir() {
            const char* configured = std::getenv("MOBILEGL_PZ_SHADER_SOURCE_CACHE_DIR");
            return configured && *configured ? configured : kDefaultShaderSourceCacheDir;
        }

        Uint64 ShaderSourceKey(Uint64 programKey, Uint stageIndex, GLenum shaderType) {
            Hasher hash;
            hash.AddCString("MGLPZ-ESSL-CACHE-V1");
            hash.AddValue(programKey);
            hash.AddValue(stageIndex);
            hash.AddValue(shaderType);
            return hash.value;
        }

        String ShaderSourceCachePath(Uint64 key) {
            return std::format("{}/{:016x}.essl", ShaderSourceCacheDir(), key);
        }

        Uint64 HashPayload(const void* data, SizeT size) {
            Hasher hasher;
            hasher.Add(data, size);
            return hasher.value;
        }

        Bool EnsureDirectory() {
            const char* path = CacheDir();
            if (::mkdir(path, 0700) == 0 || errno == EEXIST) return true;
            return false;
        }

        Bool EnsureShaderSourceDirectory() {
            const char* path = ShaderSourceCacheDir();
            return ::mkdir(path, 0700) == 0 || errno == EEXIST;
        }

        void PruneShaderSourceCache() {
            DIR* dir = ::opendir(ShaderSourceCacheDir());
            if (!dir) return;
            struct Entry { String path; time_t modified = 0; SizeT size = 0; };
            Vector<Entry> entries;
            SizeT total = 0;
            while (dirent* item = ::readdir(dir)) {
                const String name = item->d_name;
                if (!name.ends_with(".essl")) continue;
                Entry entry;
                entry.path = String(ShaderSourceCacheDir()) + "/" + name;
                struct stat info{};
                if (::stat(entry.path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) continue;
                entry.modified = info.st_mtime;
                entry.size = static_cast<SizeT>(
                    std::max<off_t>(info.st_size, static_cast<off_t>(0)));
                total += entry.size;
                entries.push_back(std::move(entry));
            }
            ::closedir(dir);
            if (total <= kMaxShaderSourceCacheBytes) return;
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
                return a.modified < b.modified;
            });
            for (const Entry& entry : entries) {
                if (total <= kMaxShaderSourceCacheBytes) break;
                if (::unlink(entry.path.c_str()) == 0) total -= std::min(total, entry.size);
            }
        }

        void PruneCache() {
            DIR* dir = ::opendir(CacheDir());
            if (!dir) return;
            struct Entry { String path; time_t modified = 0; SizeT size = 0; };
            Vector<Entry> entries;
            SizeT total = 0;
            while (dirent* item = ::readdir(dir)) {
                const String name = item->d_name;
                if (name.size() < 5 || name.ends_with(".bin") == false) continue;
                Entry entry;
                entry.path = String(CacheDir()) + "/" + name;
                struct stat info{};
                if (::stat(entry.path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) continue;
                entry.modified = info.st_mtime;
                entry.size = static_cast<SizeT>(
                    std::max<off_t>(info.st_size, static_cast<off_t>(0)));
                total += entry.size;
                entries.push_back(std::move(entry));
            }
            ::closedir(dir);
            if (total <= kMaxCacheBytes) return;
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
                return a.modified < b.modified;
            });
            for (const Entry& entry : entries) {
                if (total <= kMaxCacheBytes) break;
                if (::unlink(entry.path.c_str()) == 0) total -= std::min(total, entry.size);
            }
        }
    } // namespace

    Uint64 ComputeKey(const MG_State::GLState::ProgramObject& program,
                      const MG_External::GLESCapabilities& capabilities,
                      Uint32 snormClampMask,
                      Uint32 unormClampMask,
                      Uint fragColorBroadcastCount,
                      Uint esslVersion) {
        Hasher hash;
        hash.AddCString("MGLPZ-PROGRAM-CACHE-V1");
        hash.AddCString(MOBILEPZ_OPT_LAB_BUILD_ID);
        hash.AddString(capabilities.GLESVendorString);
        hash.AddString(capabilities.GLESRendererString);
        hash.AddString(capabilities.GLESVersionString);
        hash.AddString(capabilities.GLESShadingLanguageVersionString);
        hash.AddValue(esslVersion);
        hash.AddValue(snormClampMask);
        hash.AddValue(unormClampMask);
        hash.AddValue(fragColorBroadcastCount);
        hash.AddValue(capabilities.TextureBufferSupport);
        hash.AddValue(capabilities.SupportsNoperspectiveInterpolation);
        hash.AddValue(capabilities.IndirectDrawInstanceIdIncludesBaseInstance);

        const auto& shaders = program.GetAttachedShaders();
        const auto& spirvs = program.GetGeneratedSpirv();
        const SizeT count = std::min(shaders.size(), spirvs.size());
        hash.AddValue(count);
        for (SizeT i = 0; i < count; ++i) {
            const auto stage = shaders[i]->GetShaderStage();
            hash.AddValue(stage);
            const auto& spirv = spirvs[i];
            const Uint64 byteCount = static_cast<Uint64>(spirv.size() * sizeof(spirv[0]));
            hash.AddValue(byteCount);
            if (!spirv.empty()) hash.Add(spirv.data(), static_cast<SizeT>(byteCount));
        }

        const auto& xfb = program.GetTransformFeedbackVaryings();
        hash.AddValue(program.GetTransformFeedbackBufferMode());
        hash.AddValue(xfb.size());
        for (const auto& varying : xfb) hash.AddString(varying.name);

        Vector<std::pair<String, Int>> storageOverrides;
        for (const auto& [name, binding] : program.GetShaderStorageBlockBindingOverrides()) {
            storageOverrides.push_back({name, binding});
        }
        std::sort(storageOverrides.begin(), storageOverrides.end(), [](const auto& a, const auto& b) {
            return a.first < b.first || (a.first == b.first && a.second < b.second);
        });
        hash.AddValue(storageOverrides.size());
        for (const auto& [name, binding] : storageOverrides) {
            hash.AddString(name);
            hash.AddValue(binding);
        }

        const Uint maxLocation = program.GetMaxUniformLocation();
        for (Uint location = 0; location <= maxLocation; ++location) {
            if (!program.IsValidUniformLocation(static_cast<Int>(location))) continue;
            const GLenum type = program.GetUniformType(location);
            if (!IsImageUniformType(type)) continue;
            hash.AddValue(location);
            hash.AddValue(type);
            hash.AddValue(program.GetUniformSamplerOrImageUnitIndex(location));
        }
        return hash.value;
    }

    Bool TryLoad(Uint backendProgram, Uint64 key) {
        const auto started = std::chrono::steady_clock::now();
        PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheLookup);
        if (!g_GLESFuncs.glProgramBinary || !g_GLESFuncs.glGetProgramiv) {
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheMiss, 0, 0, true);
            return false;
        }

        const String path = CachePath(key);
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) {
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheMiss);
            return false;
        }
        CacheHeader header;
        const Bool headerRead = std::fread(&header, 1, sizeof(header), file) == sizeof(header);
        const CacheHeader expected;
        const Bool validHeader = headerRead && header.magic == expected.magic &&
                                 header.schema == kSchema && header.key == key &&
                                 header.payloadBytes > 0 && header.payloadBytes <= kMaxEntryBytes;
        Vector<Uint8> payload;
        if (validHeader) {
            payload.resize(static_cast<SizeT>(header.payloadBytes));
        }
        const Bool payloadRead = validHeader &&
            std::fread(payload.data(), 1, payload.size(), file) == payload.size();
        std::fclose(file);
        if (!payloadRead || HashPayload(payload.data(), payload.size()) != header.payloadHash) {
            std::remove(path.c_str());
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheMiss, 0, 0, true);
            return false;
        }

        g_GLESFuncs.glProgramBinary(backendProgram, header.binaryFormat, payload.data(),
                                    static_cast<GLsizei>(payload.size()));
        GLint status = GL_FALSE;
        g_GLESFuncs.glGetProgramiv(backendProgram, GL_LINK_STATUS, &status);
        const auto elapsed = static_cast<Uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
        if (status != GL_TRUE) {
            std::remove(path.c_str());
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheMiss, payload.size(), elapsed, true);
            return false;
        }
        ::utime(path.c_str(), nullptr);
        PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheHit, payload.size(), elapsed);
        return true;
    }

    void PrepareForSourceLink(Uint backendProgram) {
        if (g_GLESFuncs.glProgramParameteri) {
            g_GLESFuncs.glProgramParameteri(backendProgram, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        }
    }

    void Store(Uint backendProgram, Uint64 key) {
        if (!g_GLESFuncs.glGetProgramBinary || !g_GLESFuncs.glGetProgramiv || !EnsureDirectory()) {
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheStore, 0, 0, true);
            return;
        }
        GLint length = 0;
        g_GLESFuncs.glGetProgramiv(backendProgram, GL_PROGRAM_BINARY_LENGTH, &length);
        if (length <= 0 || static_cast<SizeT>(length) > kMaxEntryBytes) {
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheStore, 0, 0, true);
            return;
        }
        Vector<Uint8> payload(static_cast<SizeT>(length));
        GLsizei actual = 0;
        GLenum format = 0;
        g_GLESFuncs.glGetProgramBinary(backendProgram, length, &actual, &format, payload.data());
        if (actual <= 0 || actual > length || format == 0) {
            PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheStore, 0, 0, true);
            return;
        }
        payload.resize(static_cast<SizeT>(actual));
        CacheHeader header;
        header.binaryFormat = format;
        header.key = key;
        header.payloadBytes = payload.size();
        header.payloadHash = HashPayload(payload.data(), payload.size());

        const String path = CachePath(key);
        const String temporary = path + ".tmp";
        FILE* file = std::fopen(temporary.c_str(), "wb");
        Bool success = file != nullptr;
        if (file) {
            success = std::fwrite(&header, 1, sizeof(header), file) == sizeof(header) &&
                      std::fwrite(payload.data(), 1, payload.size(), file) == payload.size() &&
                      std::fflush(file) == 0;
            const int fd = ::fileno(file);
            if (success && fd >= 0) success = ::fsync(fd) == 0;
            std::fclose(file);
        }
        if (success) success = std::rename(temporary.c_str(), path.c_str()) == 0;
        if (!success) std::remove(temporary.c_str());
        PZOptLab::RecordEvent(PZOptLab::Event::ProgramCacheStore, payload.size(), 0, !success);
        if (success) PruneCache();
    }

    Bool TryLoadShaderSource(Uint64 programKey, Uint stageIndex,
                             GLenum shaderType, String& source) {
        const auto started = std::chrono::steady_clock::now();
        PZOptLab::RecordEvent(PZOptLab::Event::ShaderSourceCacheLookup);
        const Uint64 key = ShaderSourceKey(programKey, stageIndex, shaderType);
        const String path = ShaderSourceCachePath(key);
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) {
            PZOptLab::RecordEvent(PZOptLab::Event::ShaderSourceCacheMiss);
            return false;
        }
        ShaderSourceCacheHeader header;
        const Bool headerRead = std::fread(&header, 1, sizeof(header), file) == sizeof(header);
        const ShaderSourceCacheHeader expected;
        const Bool validHeader = headerRead && header.magic == expected.magic &&
                                 header.schema == kShaderSourceSchema &&
                                 header.shaderType == shaderType && header.key == key &&
                                 header.payloadBytes > 0 &&
                                 header.payloadBytes <= kMaxShaderSourceBytes;
        String payload;
        if (validHeader) payload.resize(static_cast<SizeT>(header.payloadBytes));
        const Bool payloadRead = validHeader &&
            std::fread(payload.data(), 1, payload.size(), file) == payload.size();
        std::fclose(file);
        const Bool validPayload = payloadRead &&
            HashPayload(payload.data(), payload.size()) == header.payloadHash &&
            payload.find('\0') == String::npos;
        if (!validPayload) {
            std::remove(path.c_str());
            PZOptLab::RecordEvent(PZOptLab::Event::ShaderSourceCacheMiss, 0, 0, true);
            return false;
        }
        source = std::move(payload);
        ::utime(path.c_str(), nullptr);
        const auto elapsed = static_cast<Uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
        PZOptLab::RecordEvent(PZOptLab::Event::ShaderSourceCacheHit, source.size(), elapsed);
        return true;
    }

    void StoreShaderSource(Uint64 programKey, Uint stageIndex,
                           GLenum shaderType, const String& source) {
        if (source.empty() || source.size() > kMaxShaderSourceBytes ||
            !EnsureShaderSourceDirectory()) {
            PZOptLab::RecordEvent(PZOptLab::Event::ShaderSourceCacheStore,
                                  source.size(), 0, true);
            return;
        }
        const Uint64 key = ShaderSourceKey(programKey, stageIndex, shaderType);
        ShaderSourceCacheHeader header;
        header.shaderType = shaderType;
        header.key = key;
        header.payloadBytes = source.size();
        header.payloadHash = HashPayload(source.data(), source.size());

        const String path = ShaderSourceCachePath(key);
        const String temporary = std::format("{}.{}.tmp", path, static_cast<long>(::getpid()));
        FILE* file = std::fopen(temporary.c_str(), "wb");
        Bool success = file != nullptr;
        if (file) {
            success = std::fwrite(&header, 1, sizeof(header), file) == sizeof(header) &&
                      std::fwrite(source.data(), 1, source.size(), file) == source.size() &&
                      std::fflush(file) == 0;
            const int fd = ::fileno(file);
            if (success && fd >= 0) success = ::fsync(fd) == 0;
            std::fclose(file);
        }
        if (success) success = std::rename(temporary.c_str(), path.c_str()) == 0;
        if (!success) std::remove(temporary.c_str());
        PZOptLab::RecordEvent(PZOptLab::Event::ShaderSourceCacheStore,
                              source.size(), 0, !success);
        if (success) PruneShaderSourceCache();
    }
} // namespace MobileGL::MG_Backend::DirectGLES::PZProgramBinaryCache
