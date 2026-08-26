#!/usr/bin/env bash
set -euo pipefail

SOURCE_ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ELF="${1:-}"

fail() { printf 'VERIFY_021=FAIL reason=%s\n' "$*" >&2; exit 1; }
need_source() {
    local pattern="$1" file="$2"
    grep -Fq -- "$pattern" "$SOURCE_ROOT/$file" || fail "missing:$file:$pattern"
}

need_source 'Perf021A = 1u << 19' 'MobileGL/PZOptLab.h'
need_source 'Perf021B = 1u << 20' 'MobileGL/PZOptLab.h'
need_source 'Perf021C = 1u << 21' 'MobileGL/PZOptLab.h'
need_source 'Perf021D = 1u << 22' 'MobileGL/PZOptLab.h'
need_source 'Perf021E = 1u << 23' 'MobileGL/PZOptLab.h'
need_source 'MOBILEGL_PZ_OPT_021A' 'MobileGL/PZOptLab.cpp'
need_source 'MOBILEGL_PZ_OPT_021B' 'MobileGL/PZOptLab.cpp'
need_source 'MOBILEGL_PZ_OPT_021C' 'MobileGL/PZOptLab.cpp'
need_source 'MOBILEGL_PZ_OPT_021D' 'MobileGL/PZOptLab.cpp'
need_source 'MOBILEGL_PZ_OPT_021E' 'MobileGL/PZOptLab.cpp'
need_source 'schema=4 build=OPT-LAB-V3-021' 'MobileGL/PZOptLab.cpp'
need_source 'Bit(Optimization::Perf021C) | Bit(Optimization::Perf021D);' 'MobileGL/PZOptLab.cpp'
need_source 'TextureUploadRingStage' 'MobileGL/MG_Backend/DirectGLES/Managers.cpp'
need_source 'SupportsPersistentMapping' 'MobileGL/MG_Backend/DirectGLES/Managers.cpp'
need_source 'return false; // fail-open: caller uses the client pointer, never waits' \
    'MobileGL/MG_Backend/DirectGLES/Managers.cpp'

# The experimental block itself must remain non-blocking. Existing V3 UBO/readback
# paths elsewhere in the file legitimately contain waits and are outside 021E.
PBO_BLOCK="$(sed -n '/SizeT TextureUploadRingAlignUp/,/} \/\/ namespace BufferImpl/p' \
    "$SOURCE_ROOT/MobileGL/MG_Backend/DirectGLES/Managers.cpp")"
if printf '%s\n' "$PBO_BLOCK" | grep -Eq 'glFinish|glClientWaitSync|glWaitSync'; then
    fail 'blocking-call-in-021E'
fi

if [[ -n "$ELF" ]]; then
    test -f "$ELF" || fail "elf-not-found:$ELF"
    command -v readelf >/dev/null 2>&1 || fail 'readelf-missing'
    readelf -h "$ELF" | grep -Fq 'Machine:                           AArch64' || \
        fail 'elf-not-aarch64'
    readelf -d "$ELF" | grep -Fq '[libMobileGLPZ.so]' || fail 'soname-mismatch'
    strings "$ELF" | grep -Fq 'OPT-LAB-V3-021' || fail 'elf-marker-missing'
    strings "$ELF" | grep -Fq 'MOBILEGL_PZ_OPT_021E' || fail 'elf-selector-missing'
fi

printf 'VERIFY_021=PASS source=%s%s\n' "$SOURCE_ROOT" \
    "${ELF:+ elf=$ELF sha256=$(sha256sum "$ELF" | awk '{print $1}')}"
