# PERF-005 — program-aware sampler binding

## Hypothesis

PERF-002 and PERF-003 already restrict texture synchronization and native
texture binding to units sampled by the linked draw/compute program.
`BindCurrentUnitSamplers()` still compares, restores, and snapshots every row
through `maxTouchedUnit`. PERF-005 removes that remaining full sampler-object
walk from linked program preparation.

## A/B base

Both variants enable:

- PZF23D4 visual baseline;
- PERF-001 ThinLTO;
- PERF-002 program-aware texture sync;
- PERF-003 program-aware texture binding;
- bounded PERF-005 telemetry for the first 4096 sampler-walk calls;
- FATAL-only runtime logging and no Tracy/diagnostic families.

Variant A keeps the original full sampler walk. Variant B additionally enables
`MOBILEPZ_PERF005_PROGRAM_AWARE_SAMPLER_BINDING`.

PERF-004 is excluded.

## Single variable

For a linked draw/compute program, B:

1. selects a small memo by program identity;
2. validates lifecycle, relink/sampler assignment, frontend binding epoch and
   backend-context generation;
3. iterates set bits in the existing `sampledUnits` mask;
4. compares, restores, and snapshots only those sampler-cache rows.

No-program and non-draw callers keep the complete walk. A later program always
restores the rows it can observe before its program-specific sampler pass.

## Correctness boundaries

- Program switching is covered by per-program memo selection plus lifetime and
  backend-state versions.
- Sampler delete/rebind is covered by the unit-binding epoch and row shadow.
- Raw-depth substitution is covered by the sampled row comparison: the next
  preparation first restores the frontend sampler, then the program pass
  reapplies the required raw-depth sampler.
- Relink and sampler-uniform unit edits move the backend-state version and the
  sampled mask.
- Backend GLES context recreation moves `g_backendContextGeneration`.
- A backend sampler that does not exist yet is not latched: the later program
  pass changes the row when it creates/binds the object, reopening the memo.

## Bounded evidence

Each A/B binary emits one `MOBILEPZ_PERF005_ACTIVE` identity line and, after
4096 calls, one `MOBILEPZ_PERF005_SUMMARY` line containing memo hits/misses,
full/program walks, touched/eligible/skipped units, compared rows, rebound
units, snapshotted rows and elapsed nanoseconds. Counters stop permanently after
the summary. The collector rejects a missing or mismatched identity marker.

These counters prove route use; FPS and frametime verdicts still come only from
the render-only V3 recorder on the device.
