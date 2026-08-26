# PERF-002 — program-aware texture synchronization

## Hypothesis

Project Zomboid keeps texture/sampler bindings alive across many draws, while each shader uses only a subset of the touched units. On a steady-state draw, MobileGL's texture preparation still validates and probes every bound texture in every touched unit.

## Single variable

`MOBILEPZ_PERF002_PROGRAM_AWARE_TEXTURE_SYNC=ON` makes draw/compute texture preparation defer per-texture validation and sync work for texture units that are not referenced by a sampler uniform in the current linked program.

The sampled-unit mask is cached per frontend program and invalidated by program lifetime, relink/backend-state version, or sampler-unit assignment changes. FBO attachment synchronization and all non-draw callers retain the full path. A dirty texture skipped by one program remains in the complete work list and is synchronized when a later program samples its unit.

## Test isolation

- Comparator: PERF-001 A baseline (`MOBILEGL_ENABLE_LTO=OFF`).
- Candidate C: PERF-002 enabled, `MOBILEGL_ENABLE_LTO=OFF`.
- PZF23D4 correctness fix remains enabled.
- ThinLTO remains disabled so this experiment measures one source change.
- Tracy and all diagnostic capture macros remain disabled.
- Runtime log level: FATAL.

## Status

Host build/static verification can prove packaging, ABI and configuration. Device performance and visual correctness remain pending until the A/C benchmark protocol is completed with the render-only V3 recorder.
