#!/usr/bin/env bash
set -euo pipefail

SOURCE_ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ELF="${1:-}"

fail() { printf 'VERIFY_BUG002_WQ=FAIL reason=%s\n' "$*" >&2; exit 1; }
need_source() {
    local pattern="$1" file="$2"
    grep -Fq -- "$pattern" "$SOURCE_ROOT/$file" || fail "missing:$file:$pattern"
}

need_source 'option(MOBILEPZ_BUG002_WFX_QUAD_DIAG' 'CMakeLists.txt'
need_source 'MOBILEPZ_BUG002_WFX_QUAD_DIAG=1' 'CMakeLists.txt'
need_source 'MOBILEPZ_PZF16_QUAD4_SUBMISSION_FIX=OFF' \
    'scripts/build-mobilegl-pz-bug002-weather-quad-diag-android.sh'
need_source 'MOBILEPZ_BUG002_WFX_QUAD_DIAG=ON' \
    'scripts/build-mobilegl-pz-bug002-weather-quad-diag-android.sh'
need_source 'MOBILEGL_LOG_ACTIVE_LEVEL=MOBILEGL_LOG_LEVEL_FATAL' \
    'scripts/build-mobilegl-pz-bug002-weather-quad-diag-android.sh'

DIAG_HEADER='MobileGL/MG_Util/PZDiagnostics/BUGWeatherQuadDiag.h'
for marker in BUG002_DIAG_ACTIVE BUG002_PROGRAM_CLASSIFY BUG002_LAYOUT_DECISION \
              BUG002_USE_TO_DRAW BUG002_DRAW_ROUTE BUG002_VISIBLE_STATE \
              BUG003_QUAD_PROGRAM BUG003_USE_TO_DRAW BUG003_QUAD_ROUTE; do
    need_source "$marker" "$DIAG_HEADER"
done
need_source 'ANDROID_LOG_WARN, "MGLPZ-BUGDIAG"' "$DIAG_HEADER"
need_source 'kQuadProgramReserveLimit' "$DIAG_HEADER"
need_source 'ScopedCompatQuad' "$DIAG_HEADER"

if grep -Eq '\bgl(Finish|ReadPixels)[[:space:]]*\(' "$SOURCE_ROOT/$DIAG_HEADER"; then
    fail 'blocking-or-readback-call-in-diagnostic-header'
fi

DRAW_SCOPE_COUNT="$(grep -c 'MOBILEPZ_BUGDIAG_DRAW_SCOPE(' \
    "$SOURCE_ROOT/MobileGL/MG_Impl/GLImpl/Drawing/GL_Drawing.cpp")"
[[ "$DRAW_SCOPE_COUNT" -eq 22 ]] || fail "draw-wrapper-coverage:$DRAW_SCOPE_COUNT/22"

DIRECT_NATIVE_COUNT="$(grep -c 'MOBILEPZ_BUGDIAG_NATIVE(' \
    "$SOURCE_ROOT/MobileGL/MG_Backend/DirectGLES/DirectGLES.cpp")"
[[ "$DIRECT_NATIVE_COUNT" -eq 21 ]] || fail "direct-native-coverage:$DIRECT_NATIVE_COUNT/21"

MULTI_NATIVE_COUNT="$(grep -c 'MOBILEPZ_BUGDIAG_MULTINATIVE(' \
    "$SOURCE_ROOT/MobileGL/MG_Backend/DirectGLES/MultiDraw.cpp")"
[[ "$MULTI_NATIVE_COUNT" -eq 8 ]] || fail "multi-native-coverage:$MULTI_NATIVE_COUNT/8"

for route in 'PZCompat::DrawArrays' 'PZCompat::DrawElements' \
             'PZCompat::DrawRangeElements' '"BeginEnd"'; do
    need_source "$route" 'MobileGL/MG_Impl/GLImpl/PZCompat/PZCompat.cpp'
done

if [[ -n "$ELF" ]]; then
    test -f "$ELF" || fail "elf-not-found:$ELF"
    command -v readelf >/dev/null 2>&1 || fail 'readelf-missing'
    readelf -h "$ELF" | grep -Fq 'Machine:                           AArch64' || fail 'elf-not-aarch64'
    readelf -d "$ELF" | grep -Fq '[libMobileGLPZ.so]' || fail 'soname-mismatch'
    strings "$ELF" | grep -Fq 'BUG002_DIAG_ACTIVE' || fail 'elf-weather-marker-missing'
    strings "$ELF" | grep -Fq 'BUG003_QUAD_ROUTE' || fail 'elf-quad-marker-missing'
    strings "$ELF" | grep -Fq 'MGLPZ-BUGDIAG' || fail 'elf-log-tag-missing'
fi

printf 'VERIFY_BUG002_WQ=PASS source=%s%s\n' "$SOURCE_ROOT" \
    "${ELF:+ elf=$ELF sha256=$(sha256sum "$ELF" | awk '{print $1}')}"
