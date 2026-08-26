# Stable-base audit — 2026-08-20

## Post-audit EGL lifecycle repair

The original audit below describes the supplied stable-base binary and its source at
the time of that build. A later source-only repair now extends P28M across Android's
windowless background interval. No replacement Android binary or device test was
produced, as requested.

Static control-flow inspection found that
`BackendObject_DirectGLES::OnEGLSurfaceReleased` unconditionally called
`DestroyEGLContext()`. This bypassed P28M whenever Android destroyed the old window
before creating the resumed window. The hook now calls
`DirectGLES::ReleaseSurface()`, which detaches and destroys only the native surface
while preserving the display, config, GLES context and context generation. Explicit
`eglTerminate`/resource teardown still destroys the whole context, and a native
surface-release failure still uses a bounded full-teardown fallback.

The current `BackendObject_DirectGLES.cpp` therefore intentionally differs from the
exact P28M reference hash recorded below. That old hash remains provenance for the
pre-repair stable-base candidate, not a claim about the modified file.

## Inputs and provenance

- Supplied candidate archive SHA-256:
  `9a5b04a4a08bb540bf796d668b667751cffcb479111a5926a5c52987788006da`
- The candidate's original `SOURCE-FILES-SHA256.txt` verified without errors before
  any edits.
- Accepted P28 checkpoint SHA-256:
  `8ff20551f47d2acd0be171330cd57d3f92c31d06987aa3e46576419307c4387d`
- Exact P28M source-delta archive SHA-256:
  `bd5ed40418b4f2774c48eae48866facf5968fbb61525ae6c3a23b5cbe06dfb34`
- P28H diagnostic-delta archive SHA-256 (used only to identify and remove its
  witness code):
  `22fb9b49d0af24c4c79991d9dfebce0f04c216bc2a8f5ded6049eeef8e853d76`

## Root cause corrected

The candidate reintroduced `DestroyEGLContext()` plus `ResetEGLRuntimeState()` in
both DirectGLES wrapper surface-creation methods. This guaranteed that P28M's inner
surface-only path saw no live context to preserve. Both blocks were removed.

Before the post-audit lifecycle repair, the corrected
`BackendObject_DirectGLES.cpp` was byte-identical to the exact P28M reference file,
SHA-256:
`2ec651d5b8fc74b9f3baee8543fbac4c10553b618dfac68c17c727bc2ba53232`.

## Included production behavior

- P7: exact patch reverse-check passed.
- P15: snapshot/restore implementation and `RestoreClientAttribStateFrom` route are
  present; later PZCompat edits prevent a whole-patch reverse-check.
- P21A: exact patch reverse-check passed.
- P28: EBO lifetime key (`m_syncedIndexBufferLifetimeId`) and identity-dirty path are
  present; later manager edits prevent a whole-patch reverse-check.
- P28M: exact outer wrapper file plus the inner bounded preserve/fallback route.
- P28V: native VAO binding-cache invalidation after successful `MakeCurrent`.
- PZF1: lexical `clamp`/`max`/`min` builtin-shadow rename under the V1 build flag.
- PZF16 R1/R2: exact four-index `DrawRangeElements(GL_QUADS)` translation only for
  the tracked target or depth-enabled world path, with active-program or legacy
  client-array routing.

## Excluded diagnostics

- P28H draw, transition and present sampler: removed.
- PZF one-shot quad route logger: removed.
- CP2, SL1, NG/PZF2 and PZF3–PZF16 standalone switches: rejected by CMake whenever
  `MOBILEPZ_V1_CANDIDATE=ON`.
- The build helper pins every legacy/test switch to `OFF`.

Guarded historical sources remain in the tree for provenance, but cannot enter a V1
translation unit because their compile definitions are forbidden. This avoids the
millions of counters, readbacks and trace events observed in the PZF16R2 diagnostic
binary while preserving the minimum selector state required by the accepted R2 fix.

## Original baseline verification status

Passed locally:

- source-manifest verification for the untouched input;
- archive path and symlink safety inspection;
- shell syntax checks for build and verification helpers;
- stable-base invariant verifier;
- exact P28M wrapper file comparison;
- absence scans for P28H/V1 hot-path telemetry;
- source-level lineage checks listed above;
- two independent Android arm64-v8a Release builds with NDK r27d, API 26 and
  `c++_static`;
- deterministic source-manifest ELF Build ID and byte-identical stripped output;
- ELF AArch64/SONAME/dependency/16 KiB LOAD-alignment checks;
- export-surface comparison against PZF16R2 and a negative CMake gate test that
  rejected a forced legacy PZF3 switch.

Not run in this environment:

- EGL lifecycle and rendering tests on a device.

Those Android build results apply to the original supplied baseline binary, not to
the later source-only lifecycle repair. The modified source remains a static-audited
candidate until an Android build and device lifecycle run are explicitly requested.
