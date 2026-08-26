# MobileGL-PZ OPT-LAB V3 — 021 RENDERER PACK

Data: 2026-08-25  
Țintă: Project Zomboid 42.20 prin ZomDroid, Android arm64-v8a

## Verdictul care a decis pachetul

Analiza `P0_FBO_WORLD` și `P1_FBO_WORLD` a închis pista FBO: nu există un
cost FBO steady-state care să justifice un patch `021-FBO`. Primul eșantion
rece de program a fost un spike de inițializare, nu un bottleneck repetat.

În zona aglomerată, telemetria Benchmark V4 a rămas dominată de lucru
PZ/JVM (`WORLD_STREAM_CHUNK`, `CHUNK_MAP_UPDATE`, `CHUNK_GRID_LOAD`). Acest
pachet nu modifică jocul, APK-ul sau coada WORLD. El consumă numai headroom-ul
rendererului care mai poate fi atacat corect în MobileGL, înainte de trecerea
la STREAM-CORE.

## Optimizările 021

| ID | Optimizare | Statut | Preset `ALL_SAFE_ON` |
|---|---|---|---|
| `021A` | epoch pentru shadow-ul bindingurilor native de texturi | safe candidat | inclus |
| `021B` | epoch pentru shadow-ul bindingurilor native de samplere | safe candidat | inclus |
| `021C` | memo pentru replay-ul UBO normal, separat de UBO global | safe candidat | inclus |
| `021D` | worklist SSBO fuzionat: bind + mark GPU-written într-o singură parcurgere | safe candidat | inclus |
| `021E` | ring PBO persistent/coherent pentru uploaduri mari de texturi | experimental | **nu** |

Toate cele cinci sunt în același ELF și au toggle individual. Nu este nevoie
de câte un build pentru fiecare test.

## Control runtime

Prin launcher:

```text
MOBILEGL_PZ_OPT_SET=019,020,021A
MOBILEGL_PZ_OPT_SET=019,020,021B
MOBILEGL_PZ_OPT_SET=019,020,021C
MOBILEGL_PZ_OPT_SET=019,020,021D
MOBILEGL_PZ_OPT_SET=019,020,021E
MOBILEGL_PZ_OPT_SET=019,020,021A,021B,021C,021D
```

Sau individual peste un preset:

```text
MOBILEGL_PZ_OPT_021A=1
MOBILEGL_PZ_OPT_021B=1
MOBILEGL_PZ_OPT_021C=1
MOBILEGL_PZ_OPT_021D=1
MOBILEGL_PZ_OPT_021E=1
```

Selecția se citește o singură dată per proces. Oprirea completă și repornirea
aplicației sunt obligatorii după fiecare schimbare.

## Guardrail-uri

- `018` rămâne numai diagnostic și nu se combină cu verdictul FPS.
- `021E` se testează separat. Nu este activat de `ALL_SAFE_ON`.
- `021E` acceptă numai uploaduri necomprimate, contigue și full-level de cel
  puțin 64 KiB; subrecturile strided/scatter și S3TC rămân pe calea originală.
- Ringul PBO nu așteaptă și nu cheamă `glFinish`; la lipsă de spațiu,
  capabilitate sau mapare, uploadul revine la pointerul client original.
- Niciun hook Java/JVM, FBO sau WORLD nu este inclus.

Matricea exactă este în `OPT-LAB-V3-021-DEVICE-TEST-RO.md`, iar deciziile și
verificările de build sunt în `OPT-LAB-V3-021-AUDIT-RO.md`.
