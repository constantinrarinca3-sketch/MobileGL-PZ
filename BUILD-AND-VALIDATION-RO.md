# Build și validare — OPT-LAB V3

Data: 2026-08-24

## Identitate

- Build-relevant source ID SHA-256: `28d0a8187851364b874a650c8104e9148fc9b691338c79ed06116f38ac364ab3`
- THINLTO build ID: `2400e9c51ce9c885194163abfe73746f5b583f16`
- THINLTO ELF SHA-256: `faa53418d6020d64598f6d11a95a4a31679192e8aeb2ac528bddde5f72892729`
- NOLTO build ID: `39fe7fcc34ca76562545b8539fc6706f379dba17`
- NOLTO ELF SHA-256: `3cd90367ef92ef908604e1239a4630f44d2ecb936c9b366ee80acce2fe34b47d`
- Benchmark V4 R3 JAR SHA-256: `0493cb85d539b83a17ff24fba2909f63845fb8b52e756ae4995da3a89f459ed3`
- Benchmark V4 R3 PZ mod ZIP SHA-256: `f7fa6fc076950963349cd6f44d04f464ac743cafd69050851bcedae831bbf4b0`

Source ID-ul este calculat din hashurile sortate ale `MobileGL`, `include`,
`3rdparty`, `buildsystem` și `CMakeLists.txt`; nu depinde de cale sau mtime.

## Configurație verificată

- Android NDK r27d `27.3.13750724`, ABI `arm64-v8a`, API 26;
- Release, `MOBILEGL_LOG_LEVEL_FATAL`, trace/census/Tracy dezactivate;
- THINLTO conține `-flto=thin`; controlul NOLTO nu îl conține;
- ambele ELF-uri sunt AArch64 DYN, SONAME `libMobileGLPZ.so`, aliniere LOAD
  `0x4000` (16 KiB) și sunt stripped în pachet;
- markerul nativ schema 3 enumeră exact 002–020/004R și declară vechiul PERF-004 exclus;
- ambele builduri native au trecut compilarea completă și recheck-ul Ninja
  `no work to do` după înghețarea sursei.

## Benchmark V4 R3

- build Java 17 și self-test: PASS;
- 19 hook-uri, toate read-only;
- scanare interzisă pentru `getThreadInfo`, `getStackTrace`, thread de sampler:
  PASS;
- două builduri consecutive ale JAR-ului: byte-identice;
- ZIP-ul de device reconstruit separat: byte-identic;
- comparatorul a trecut un test sintetic cu două rapoarte, inclusiv worst-window,
  p99/p99.9, CPU/wait, GC și heap;
- dovada pentru metricile comune atribuie numai proprietarii care sunt efectiv
  enabled, evitând `UNEXPECTED_HIT` fals la 006/008/009/010.
- metricile `draw_prepare_*` sunt atribuite numai lui 018; 019/020 folosesc
  proof bounded de lucru atins/reținut/evitat.

## Limită explicită

Compilarea și validările structurale sunt PASS. Validarea funcțională/GPU,
fidelitatea vizuală, stabilitatea lungă, temperatura și verdictul de performanță
necesită rularea pe tabletă; pachetul este OPT-LAB, nu stable/promoted.
