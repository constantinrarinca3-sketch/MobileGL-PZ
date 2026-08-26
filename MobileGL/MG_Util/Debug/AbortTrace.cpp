// MobileGL-PZCompat diagnostic-only SIGABRT tracing.
// Keep this module independent of renderer state: P4-TRACE must differ from P3
// only in observability, not in GL/EGL behavior.

#include "AbortTrace.h"

#if defined(__ANDROID__) && defined(MOBILEGL_PZCOMPAT_ABORT_TRACE)

#include <android/log.h>

#include <cerrno>
#include <cinttypes>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

namespace MobileGL::MG_Util::Debug::AbortTrace {
    namespace {
        constexpr char kTraceName[] = "mglpz-p4-trace.txt";
        constexpr char kTraceVersion[] = "MGLPZ-P4-TRACE v1";
        constexpr size_t kMaxFrames = 64;
        constexpr uint64_t kMaxFrameStep = 16ULL * 1024ULL * 1024ULL;

        alignas(64) char g_tracePath[PATH_MAX] = "mglpz-p4-trace.txt";
        volatile sig_atomic_t g_installed = 0;
        volatile sig_atomic_t g_handlingAbort = 0;
        volatile sig_atomic_t g_inTerminate = 0;

        void WriteAll(int fd, const char* data, size_t size) {
            while (size > 0) {
                const ssize_t written = write(fd, data, size);
                if (written > 0) {
                    data += written;
                    size -= static_cast<size_t>(written);
                    continue;
                }
                if (written < 0 && errno == EINTR) {
                    continue;
                }
                break;
            }
        }

        void WriteLiteral(int fd, const char* text) {
            size_t length = 0;
            while (text[length] != '\0') {
                ++length;
            }
            WriteAll(fd, text, length);
        }

        size_t AppendText(char* out, size_t capacity, size_t offset, const char* text) {
            while (*text && offset < capacity) {
                out[offset++] = *text++;
            }
            return offset;
        }

        size_t AppendUnsigned(char* out, size_t capacity, size_t offset, uint64_t value) {
            char reverse[32];
            size_t count = 0;
            do {
                reverse[count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value && count < sizeof(reverse));
            while (count > 0 && offset < capacity) {
                out[offset++] = reverse[--count];
            }
            return offset;
        }

        size_t AppendSigned(char* out, size_t capacity, size_t offset, int64_t value) {
            if (value < 0) {
                if (offset < capacity) {
                    out[offset++] = '-';
                }
                const uint64_t magnitude = static_cast<uint64_t>(-(value + 1)) + 1;
                return AppendUnsigned(out, capacity, offset, magnitude);
            }
            return AppendUnsigned(out, capacity, offset, static_cast<uint64_t>(value));
        }

        size_t AppendHex(char* out, size_t capacity, size_t offset, uint64_t value) {
            static constexpr char digits[] = "0123456789abcdef";
            char reverse[16];
            size_t count = 0;
            do {
                reverse[count++] = digits[value & 0xfU];
                value >>= 4U;
            } while (value && count < sizeof(reverse));
            offset = AppendText(out, capacity, offset, "0x");
            while (count > 0 && offset < capacity) {
                out[offset++] = reverse[--count];
            }
            return offset;
        }

        void WriteSignedField(int fd, const char* name, int64_t value) {
            char line[128];
            size_t offset = AppendText(line, sizeof(line), 0, name);
            if (offset < sizeof(line)) {
                line[offset++] = '=';
            }
            offset = AppendSigned(line, sizeof(line), offset, value);
            if (offset < sizeof(line)) {
                line[offset++] = '\n';
            }
            WriteAll(fd, line, offset);
        }

        void WriteHexField(int fd, const char* name, uint64_t value) {
            char line[128];
            size_t offset = AppendText(line, sizeof(line), 0, name);
            if (offset < sizeof(line)) {
                line[offset++] = '=';
            }
            offset = AppendHex(line, sizeof(line), offset, value);
            if (offset < sizeof(line)) {
                line[offset++] = '\n';
            }
            WriteAll(fd, line, offset);
        }

        void WriteIndexedHexField(int fd, const char* prefix, uint64_t index, uint64_t value) {
            char line[128];
            size_t offset = AppendText(line, sizeof(line), 0, prefix);
            offset = AppendUnsigned(line, sizeof(line), offset, index);
            if (offset < sizeof(line)) {
                line[offset++] = '=';
            }
            offset = AppendHex(line, sizeof(line), offset, value);
            if (offset < sizeof(line)) {
                line[offset++] = '\n';
            }
            WriteAll(fd, line, offset);
        }

        int OpenTrace(int flags) {
            return open(g_tracePath, flags | O_CLOEXEC, 0600);
        }

        void CopyProcFile(int outputFd, const char* path, const char* heading) {
            const int inputFd = open(path, O_RDONLY | O_CLOEXEC);
            WriteLiteral(outputFd, heading);
            if (inputFd < 0) {
                WriteSignedField(outputFd, "read_error", errno);
                return;
            }

            char buffer[4096];
            while (true) {
                const ssize_t count = read(inputFd, buffer, sizeof(buffer));
                if (count > 0) {
                    WriteAll(outputFd, buffer, static_cast<size_t>(count));
                    continue;
                }
                if (count < 0 && errno == EINTR) {
                    continue;
                }
                break;
            }
            close(inputFd);
            WriteLiteral(outputFd, "\n");
        }

        void WriteFramePointerChain(int fd, pid_t pid, uint64_t firstFramePointer) {
            WriteLiteral(fd, "-- aarch64-frame-pointer-chain --\n");
            uint64_t framePointer = firstFramePointer;
            for (size_t frameIndex = 0; frameIndex < kMaxFrames; ++frameIndex) {
                if (framePointer == 0 || (framePointer & 0xfU) != 0) {
                    WriteSignedField(fd, "frame_stop_alignment", static_cast<int64_t>(frameIndex));
                    break;
                }

                uint64_t frame[2] = {};
                iovec local = {frame, sizeof(frame)};
                iovec remote = {reinterpret_cast<void*>(static_cast<uintptr_t>(framePointer)), sizeof(frame)};
                const ssize_t count = process_vm_readv(pid, &local, 1, &remote, 1, 0);
                if (count != static_cast<ssize_t>(sizeof(frame))) {
                    WriteSignedField(fd, "frame_read_error", errno);
                    break;
                }

                WriteIndexedHexField(fd, "frame", frameIndex, frame[1]);
                if (frame[0] <= framePointer || frame[0] - framePointer > kMaxFrameStep) {
                    WriteHexField(fd, "frame_stop_next_fp", frame[0]);
                    break;
                }
                framePointer = frame[0];
            }
        }

        [[noreturn]] void FinishWithOriginalSignal(int signalNumber, pid_t pid, pid_t tid) {
            struct sigaction action {};
            action.sa_handler = SIG_DFL;
            sigemptyset(&action.sa_mask);
            sigaction(signalNumber, &action, nullptr);

            sigset_t unblocked;
            sigemptyset(&unblocked);
            sigaddset(&unblocked, signalNumber);
            sigprocmask(SIG_UNBLOCK, &unblocked, nullptr);

            syscall(SYS_tgkill, pid, tid, signalNumber);
            _exit(128 + signalNumber);
        }

        void AbortHandler(int signalNumber, siginfo_t* info, void* rawContext) {
            const pid_t pid = getpid();
            const pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
            if (__sync_lock_test_and_set(&g_handlingAbort, 1) != 0) {
                FinishWithOriginalSignal(signalNumber, pid, tid);
            }

            const int fd = OpenTrace(O_WRONLY | O_CREAT | O_APPEND);
            if (fd >= 0) {
                WriteLiteral(fd, "\n=== MGLPZ-P4 ABORT EVENT ===\n");
                WriteLiteral(fd, g_inTerminate ? "origin=std_terminate\n" : "origin=signal\n");
                WriteSignedField(fd, "signal", signalNumber);
                WriteSignedField(fd, "pid", pid);
                WriteSignedField(fd, "tid", tid);
                if (info) {
                    WriteSignedField(fd, "si_code", info->si_code);
                    WriteSignedField(fd, "si_pid", info->si_pid);
                    WriteSignedField(fd, "si_uid", info->si_uid);
                    WriteHexField(fd, "si_addr", reinterpret_cast<uint64_t>(info->si_addr));
                }

                timespec timestamp {};
                if (clock_gettime(CLOCK_REALTIME, &timestamp) == 0) {
                    WriteSignedField(fd, "time_sec", timestamp.tv_sec);
                    WriteSignedField(fd, "time_nsec", timestamp.tv_nsec);
                }

#if defined(__aarch64__)
                auto* context = static_cast<ucontext_t*>(rawContext);
                if (context) {
                    WriteHexField(fd, "pc", context->uc_mcontext.pc);
                    WriteHexField(fd, "sp", context->uc_mcontext.sp);
                    WriteHexField(fd, "pstate", context->uc_mcontext.pstate);
                    for (uint64_t registerIndex = 0; registerIndex < 31; ++registerIndex) {
                        WriteIndexedHexField(fd, "x", registerIndex, context->uc_mcontext.regs[registerIndex]);
                    }
                    syscall(SYS_fsync, fd);
                    WriteFramePointerChain(fd, pid, context->uc_mcontext.regs[29]);
                }
#endif

                CopyProcFile(fd, "/proc/self/maps", "-- proc-self-maps --\n");
                WriteLiteral(fd, "=== END MGLPZ-P4 ABORT EVENT ===\n");
                syscall(SYS_fsync, fd);
                close(fd);
            }

            FinishWithOriginalSignal(signalNumber, pid, tid);
        }

        [[noreturn]] void TerminateHandler() noexcept {
            g_inTerminate = 1;
            const int fd = OpenTrace(O_WRONLY | O_CREAT | O_APPEND);
            if (fd >= 0) {
                WriteLiteral(fd, "\n=== MGLPZ-P4 STD TERMINATE ===\n");
                syscall(SYS_fsync, fd);
                close(fd);
            }
            std::abort();
        }

        void ResolveTracePath() {
            const char* explicitPath = std::getenv("MOBILEGL_PZ_TRACE_FILE_PATH");
            if (explicitPath && *explicitPath) {
                std::snprintf(g_tracePath, sizeof(g_tracePath), "%s", explicitPath);
                return;
            }

            const char* logPath = std::getenv("MOBILEGL_LOG_FILE_PATH");
            if (logPath && *logPath) {
                const char* slash = std::strrchr(logPath, '/');
                if (slash) {
                    const size_t directoryLength = static_cast<size_t>(slash - logPath + 1);
                    if (directoryLength + sizeof(kTraceName) <= sizeof(g_tracePath)) {
                        std::memcpy(g_tracePath, logPath, directoryLength);
                        std::memcpy(g_tracePath + directoryLength, kTraceName, sizeof(kTraceName));
                        return;
                    }
                }
            }

            char currentDirectory[PATH_MAX];
            const ssize_t length = readlink("/proc/self/cwd", currentDirectory, sizeof(currentDirectory) - 1);
            if (length > 0) {
                currentDirectory[length] = '\0';
                std::snprintf(g_tracePath, sizeof(g_tracePath), "%s/%s", currentDirectory, kTraceName);
            }
        }
    }

    void Install() {
        if (__sync_lock_test_and_set(&g_installed, 1) != 0) {
            return;
        }

        ResolveTracePath();
        const int fd = OpenTrace(O_WRONLY | O_CREAT | O_TRUNC);
        if (fd >= 0) {
            dprintf(fd, "%s\nstate=armed\npid=%d\ntrace_path=%s\n", kTraceVersion, getpid(), g_tracePath);
            fsync(fd);
            close(fd);
        }

        struct sigaction action {};
        action.sa_sigaction = AbortHandler;
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGABRT);
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
        const int sigactionResult = sigaction(SIGABRT, &action, nullptr);
        std::set_terminate(TerminateHandler);

        __android_log_print(sigactionResult == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                            "MobileGL",
                            "PZCOMPAT_TRACE armed path=%s sigaction=%d errno=%d",
                            g_tracePath,
                            sigactionResult,
                            sigactionResult == 0 ? 0 : errno);
    }
}

#else

namespace MobileGL::MG_Util::Debug::AbortTrace {
    void Install() {}
}

#endif
