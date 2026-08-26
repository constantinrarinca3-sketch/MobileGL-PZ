# MobileGL-PZ Performance baseline derived from PZF23D4

Date: 2026-08-24  
Role: source shared by both sides of `PERF-001 ThinLTO A/B`

## Provenance

This tree is derived directly from
`MobileGL-PZ-STABLE-PZF23D4-ALPHA-DEPTH-FIX-SOURCE-2026-08-24`.
It preserves the complete MAP FIX V2 / V1 production line and the device-validated
PZF23D4 legacy alpha-test repair.

The render-bug stable source remains immutable. This copy is the independent
Performance baseline.

## Baseline-only cleanup shared by A and B

- Runtime log level is `FATAL` for benchmark builds.
- PZF23D4 trace counters and INFO logging are compiled out at that level.
- The PZF23D4 shader rewrite and live alpha-state uniform uploads remain enabled.
- D2, D3 and all unrelated diagnostic experiments remain disabled.

This cleanup is identical in A and B and is not the measured variable.

## PERF-001 variable

- `A_BASELINE`: `MOBILEGL_ENABLE_LTO=OFF`
- `B_THINLTO`: `MOBILEGL_ENABLE_LTO=ON`

Every other configured option and every source byte are shared. The experiment
tests whether the already-supported ThinLTO path improves Project Zomboid
performance without changing rendering semantics.

Build with:

```bash
bash scripts/build-mobilegl-pz-perf-001-android.sh A_BASELINE /absolute/build/a
bash scripts/build-mobilegl-pz-perf-001-android.sh B_THINLTO /absolute/build/b
```
