# Audit și decizii — OPT-LAB V3 021

Data: 2026-08-25

## Decizie de rută

1. `021-FBO` a fost respins: probele P0/P1 nu arată un bottleneck FBO
   steady-state. Reactivarea soluției FBO care a produs flicker este interzisă.
2. Patch-ul JVM WORLD-001 a fost respins: back-pressure-ul a redus
   disponibilitatea chunkurilor și a produs zone negre la viteză mare.
3. `019` rămâne cel mai bun candidat individual din testarea utilizatorului;
   `019+020` este controlul cumulat de lucru pentru 021.
4. `018` rămâne diagnostic. `018+019+020` nu este configurație FPS.
5. Pachetul 021 consumă într-un singur ELF headroom-ul MobileGL rămas, cu
   atribuirea păstrată prin cinci toggle-uri separate.

## Suprafața modificată

| ID | Mecanism | Invariant de corectitudine |
|---|---|---|
| `021A` | generație pe shadow-ul texturilor | orice schimbare reală de rând avansează epoch-ul; la schimbare rulează comparația veche fail-open |
| `021B` | generație pe shadow-ul samplerelor | aceeași regulă ca la texturi; scrub/delete/recreate invalidează epoch-ul |
| `021C` | memo UBO normal | cheia include program, lifetime/link, versiuni de bloc/backend/context și generațiile frontend/native; bindingul global 0 este exclus |
| `021D` | parcurgere SSBO fuzionată | păstrează bindurile originale și deduplică numai `MarkGpuWritten`, care este idempotent |
| `021E` | staging PBO persistent/coherent | reutilizare numai după watermark-ul fence; la presiune grow sau fallback direct, niciodată wait |

Nu a fost acceptată pista de „program validation memo”: primul spike de program
este rece, iar memoizarea propusă nu reduce lucru dominant repetat.

## Guardrail-uri 021E

- necesită capabilitatea parsată `EXT_buffer_storage` și entrypoint-urile de
  storage/map/fence;
- storage și map folosesc `WRITE | PERSISTENT | COHERENT`;
- memcpy-ul și comanda GL sunt emise pe același thread;
- offsetul este aliniat la 16 octeți și footprint-ul este contiguu;
- starea unpack este canonicalizată în jurul uploadului și bindingul PBO revine
  la zero pe fiecare ieșire;
- ringurile înlocuite rămân vii până la serialul de frame finalizat;
- limită: 8 MiB inițial, 64 MiB maxim, prag upload 64 KiB;
- compressed/S3TC și dirty subrects nu intră în această cale;
- nu există `glFinish`, `glClientWaitSync` sau `glWaitSync` adăugat de 021E.

Conform `EXT_buffer_storage`, mappingul persistent și coherent face write-urile
CPU vizibile comenzilor GL ulterioare fără flush/barrier suplimentar. Pentru PBO,
argumentul `data` devine byte offset cât timp `GL_PIXEL_UNPACK_BUFFER` este
legat; de aceea calea restrânsă și restaurarea bindingului sunt obligatorii.

Surse primare verificate:

- Khronos `EXT_buffer_storage`: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_buffer_storage.txt
- OpenGL ES 3.2 specification: https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf
- ANGLE `BufferGL.cpp`, revizie fixă: https://chromium.googlesource.com/angle/angle/+/4b61de64bab8c25584724295f3b1f8f0fab01b38/src/libANGLE/renderer/gl/BufferGL.cpp
- ANGLE `PBOExtensionTest.cpp`, revizie fixă: https://chromium.googlesource.com/angle/angle/+/4b61de64bab8c25584724295f3b1f8f0fab01b38/src/tests/gl_tests/PBOExtensionTest.cpp

## Verificare host și Android

- syntax-only C++23: toate cele șapte unități modificate au trecut cu aceleași
  macro-uri de build ca varianta Android;
- build Android arm64-v8a ThinLTO: NDK r27d / Clang 18.0.4, CMake 3.22.1,
  Ninja 1.13;
- rezultat: ELF64 AArch64 `ET_DYN`, SONAME `libMobileGLPZ.so`;
- toate segmentele LOAD au aliniere 16 KiB;
- setul de simboluri exportate este identic cu V3: 8.844 simboluri globale
  definite în fiecare;
- fără dependență `libc++_shared.so`;
- markerul, schema 4 și toate ID-urile/variabilele 021 sunt prezente în ELF.

Identitatea finală și hashurile artefactului se află în manifestul pachetului.

## Pragul de oprire MobileGL

După matricea 021, nu mai adăugăm optimizări renderer speculative dacă:

- timpul MobileGL din fereastra grea este cel mult aproximativ 10% din
  frametime sau cel mult 1–1,5 ms/frame;
- nu mai există spike nativ dominant repetabil;
- câștigul safe combinat este sub 3% ori se pierde în variația termică;
- bottleneckurile rămase sunt `WORLD_STREAM_CHUNK`, `CHUNK_MAP_UPDATE` și
  `CHUNK_GRID_LOAD`.

Atunci următoarea rută corectă este STREAM-CORE în ZomDroid/PZ, nu încă un
patch FBO și nu reducerea agresivă a cozii care produce chunkuri negre.
