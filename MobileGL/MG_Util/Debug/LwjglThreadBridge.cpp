// MobileGL-PZCompat bridge for ZomDroid's split EGL/Java render threads.
//
// LWJGL 3.4.1 stores the active GLCapabilities address array in
// JNIEnv->reserved3. ZomDroid creates/drives EGL on an Android Thread-* but
// GL.createCapabilities() runs on zomdroid-main. Without a populated
// reserved3 on the Android thread, the first generated LWJGL OpenGL JNI call
// enters functionMissingAbort instead of MobileGL.

#include "LwjglThreadBridge.h"

#if defined(__ANDROID__) && defined(MOBILEGL_PZCOMPAT_LWJGL_THREAD_BRIDGE)

#include <android/log.h>
#include <jni.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace MobileGL::MG_Util::Debug::LwjglThreadBridge {
    namespace {
        // Exact global PointerBuffer index from the supplied LWJGL 3.4.1
        // GLCapabilities.class. Its constructor assigns glGetString from
        // PointerBuffer.get(111). The value 37 used by P5 was only the
        // function's position inside check_GL11's local name list.
        constexpr size_t kGlGetStringAddressIndex = 111;

        // The exact embedded JRE is newer than Android's jni.h. LWJGL 3.4.1
        // copies 237 pointer slots for JNI 24 (4 reserved + 233 functions).
        constexpr size_t kJniNativeInterfacePointerCount = 237;

        std::atomic<JavaVM*> g_lwjglVm = nullptr;
        std::atomic<const JNINativeInterface*> g_sharedJniTable = nullptr;
        std::atomic<void*> g_mobileGlGetString = nullptr;
        std::atomic<unsigned> g_diagnosticFlags = 0;

        enum DiagnosticFlag : unsigned {
            VmUnresolved = 1U << 0,
            EnvUnavailable = 1U << 1,
            FunctionsUnavailable = 1U << 2,
            CapabilitiesUnavailable = 1U << 3,
            CapabilitySlotMismatch = 1U << 4,
        };

        void LogDiagnosticOnce(DiagnosticFlag flag, const char* stage, const char* origin,
                               const void* actual = nullptr, const void* expected = nullptr) noexcept {
            if ((g_diagnosticFlags.fetch_or(flag, std::memory_order_relaxed) & flag) != 0) {
                return;
            }
            __android_log_print(ANDROID_LOG_INFO, "MobileGL",
                                "PZCOMPAT_LWJGL_BRIDGE waiting stage=%s tid=%ld origin=%s slot=%zu actual=%p expected=%p",
                                stage, static_cast<long>(syscall(SYS_gettid)), origin ? origin : "unknown",
                                kGlGetStringAddressIndex, actual, expected);
        }

        JavaVM* ResolveLwjglVm() noexcept {
            if (JavaVM* cached = g_lwjglVm.load(std::memory_order_acquire)) {
                return cached;
            }

            // liblwjgl.so exports its JavaVM* as `jvm`. Resolve on the exact
            // DSO handle first so a generic symbol of the same name elsewhere
            // in the process cannot be selected. RTLD_DEFAULT is only the
            // namespace fallback.
            void* lwjglHandle = dlopen("liblwjgl.so", RTLD_NOW | RTLD_NOLOAD);
            auto** slot = lwjglHandle ? static_cast<JavaVM**>(dlsym(lwjglHandle, "jvm")) : nullptr;
            if (!slot) {
                slot = static_cast<JavaVM**>(dlsym(RTLD_DEFAULT, "jvm"));
            }

            JavaVM* vm = slot ? *slot : nullptr;
            if (lwjglHandle) {
                dlclose(lwjglHandle);
            }
            if (vm) {
                g_lwjglVm.store(vm, std::memory_order_release);
            }
            return vm;
        }

        JNIEnv* CurrentLwjglEnv(const char* origin) noexcept {
            JavaVM* vm = ResolveLwjglVm();
            if (!vm) {
                LogDiagnosticOnce(VmUnresolved, "vm-unresolved", origin);
                return nullptr;
            }
            JNIEnv* env = nullptr;
            const jint result = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (result != JNI_OK || !env) {
                LogDiagnosticOnce(EnvUnavailable, "env-unavailable", origin,
                                  reinterpret_cast<const void*>(static_cast<intptr_t>(result)));
                return nullptr;
            }
            return env;
        }

        void* ResolveMobileGlGetString() noexcept {
            if (void* cached = g_mobileGlGetString.load(std::memory_order_acquire)) {
                return cached;
            }

            // Taking &glGetString directly would leave a preemptible ELF
            // relocation. If libGLES was loaded first, that relocation may
            // resolve to Qualcomm's symbol instead of this wrapper. Resolve
            // against our own DSO handle so the comparison is unambiguous.
            Dl_info selfInfo {};
            if (dladdr(reinterpret_cast<void*>(&ObserveCurrentThread), &selfInfo) == 0 || !selfInfo.dli_fname) {
                return nullptr;
            }
            void* self = dlopen(selfInfo.dli_fname, RTLD_NOW | RTLD_NOLOAD);
            if (!self) {
                return nullptr;
            }
            void* address = dlsym(self, "glGetString");
            dlclose(self);
            if (address) {
                g_mobileGlGetString.store(address, std::memory_order_release);
            }
            return address;
        }

        bool IsMobileGlCapabilities(const void* capabilities) noexcept {
            if (!capabilities) {
                return false;
            }
            auto* addresses = static_cast<void* const*>(capabilities);
            void* mobileGlGetString = ResolveMobileGlGetString();
            return mobileGlGetString && addresses[kGlGetStringAddressIndex] == mobileGlGetString;
        }

        bool IsZomDroidEglThread() noexcept {
            char name[64] = {};
            if (pthread_getname_np(pthread_self(), name, sizeof(name)) != 0) {
                return false;
            }
            // MobileGL formats log headers as "[<OS> <pthread>/...]". The
            // observed "[Android Thread-4/...]" therefore means the actual
            // pthread name is "Thread-4", not "Android Thread-4".
            return std::strncmp(name, "Thread-", sizeof("Thread-") - 1) == 0;
        }

        void Capture(JNIEnv* env, const char* origin) noexcept {
            const JNINativeInterface* current = env->functions;
            if (!current) {
                LogDiagnosticOnce(FunctionsUnavailable, "functions-null", origin);
                return;
            }

            const void* capabilities = current->reserved3;
            if (!capabilities) {
                LogDiagnosticOnce(CapabilitiesUnavailable, "capabilities-null", origin);
                return;
            }

            void* expectedAddress = ResolveMobileGlGetString();
            auto* addresses = static_cast<void* const*>(capabilities);
            if (!expectedAddress || addresses[kGlGetStringAddressIndex] != expectedAddress) {
                LogDiagnosticOnce(CapabilitySlotMismatch, "slot-mismatch", origin,
                                  addresses[kGlGetStringAddressIndex], expectedAddress);
                return;
            }

            if (g_sharedJniTable.load(std::memory_order_acquire)) {
                return;
            }

            const size_t bytes = kJniNativeInterfacePointerCount * sizeof(void*);
            auto* copy = static_cast<JNINativeInterface*>(std::malloc(bytes));
            if (!copy) {
                return;
            }
            std::memcpy(copy, current, bytes);

            const JNINativeInterface* expected = nullptr;
            if (!g_sharedJniTable.compare_exchange_strong(expected, copy, std::memory_order_release,
                                                          std::memory_order_acquire)) {
                std::free(copy);
                return;
            }

            __android_log_print(ANDROID_LOG_INFO, "MobileGL",
                                "PZCOMPAT_LWJGL_BRIDGE captured tid=%ld origin=%s caps=%p",
                                static_cast<long>(syscall(SYS_gettid)), origin ? origin : "unknown", capabilities);
        }

        void Install(JNIEnv* env, const JNINativeInterface* shared, const char* origin) noexcept {
            if (!IsZomDroidEglThread() || env->functions == shared) {
                return;
            }

            const void* currentCapabilities = env->functions ? env->functions->reserved3 : nullptr;
            if (IsMobileGlCapabilities(currentCapabilities)) {
                return;
            }

            // This is the same HotSpot JNIEnv-table injection used internally
            // by LWJGL ThreadLocalUtil.setCapabilities(). The private copy is
            // process-lifetime storage, so it cannot be invalidated when the
            // Java render thread clears its own ThreadLocal capabilities.
            env->functions = shared;

            __android_log_print(ANDROID_LOG_INFO, "MobileGL",
                                "PZCOMPAT_LWJGL_BRIDGE installed tid=%ld origin=%s caps=%p",
                                static_cast<long>(syscall(SYS_gettid)), origin ? origin : "unknown", shared->reserved3);
        }
    }

    void ObserveCurrentThread(const char* origin) noexcept {
        JNIEnv* env = CurrentLwjglEnv(origin);
        if (!env || !env->functions) {
            return;
        }

        if (const JNINativeInterface* shared = g_sharedJniTable.load(std::memory_order_acquire)) {
            Install(env, shared, origin);
            return;
        }
        Capture(env, origin);
    }
}

#else

namespace MobileGL::MG_Util::Debug::LwjglThreadBridge {
    void ObserveCurrentThread(const char*) noexcept {}
}

#endif
