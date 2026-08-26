# PERF-003 — program-aware texture binding

## Hypothesis

PERF-002 already avoids texture synchronization work on units the current
shader cannot sample. The binding stage still compares the complete native
binding shadow and, on a miss, resolves every touched unit even when most units
are unreachable from the current program.

## Single variable

`MOBILEPZ_PERF003_PROGRAM_AWARE_TEXTURE_BINDING=ON` changes only the texture
binding stage:

- the shadow memo compares only rows selected by the current program's sampler
  mask;
- a miss resolves/binds only those sampled units;
- an unsampled unit is deferred until a later program actually samples it.

PERF-003 requires PERF-002 because it reuses the cached, lifecycle-safe sampled
unit mask. Non-draw paths retain the full walk. Sampler-object binding is not
changed in this experiment, so the variable remains narrow.

## Comparator

- C: PERF-002 ON, PERF-003 OFF, ThinLTO OFF.
- D: PERF-002 ON, PERF-003 ON, ThinLTO OFF.
- PZF23D4 remains ON in both.
- Tracy, diagnostics and debug logging remain OFF/FATAL.

The C build must reproduce the published PERF-002 candidate byte-for-byte when
the fixed PERF-002 Build ID is used. This proves the new source is dormant with
PERF-003 disabled.

## Correctness boundary

The full key still covers context identity, complete frontend binding epoch,
program lifetime, relink/sampler-unit version, sampling-resolution generation
and backend-context generation. A sampled row changed by uploads or another
writer is restored before the draw. Rows the program cannot observe are allowed
to remain stale only until a program that samples them is prepared.

## Status

Host build/static verification can validate isolation and ABI. Device
performance and visual correctness require the C/D protocol with recorder V3.
