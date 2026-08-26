# Audit și decizii — OPT-LAB V3 022

Data: 2026-08-25

## Punctul de plecare păstrat

Nu s-a reluat auditul V3 și nu s-a reconstruit sursa de la zero. `022` este o
ramură separată pornită exact din `021 RENDERER PACK`.

Rezultatele care au decis ruta:

- `019` și `020` au fost cele mai bune teste anterioare, cu `019` cel mai bun
  individual după observația utilizatorului;
- `021A`, `021B` și `021D` au fost exercitate; `021C` nu a avut hit;
- `021E` a avut zero uploaduri eligibile, deși Benchmark V4 a raportat peste
  5.000 apeluri `TEXTURE_UPLOAD` și aproximativ 1,4–1,6 s cumulat;
- `program_sync` este dominat de primul eșantion rece; după acesta costul a
  fost aproximativ 67–72 ns/draw și nu justifică încă un memo;
- FBO rămâne închis: nu există dovadă de cost FBO steady-state, iar ruta veche
  a produs flicker;
- patchul JVM cu back-pressure nu intră în acest build; pe telefon a întârziat
  chunkurile și a produs zone negre la viteză mare.

## Suprafața 022

| ID | Suprafață | Invariant |
|---|---|---|
| `022A` | `TexSubImage1D/2D/3D` și DSA 2D | scrie numai regiunea cerută în shadow; fallback exact dacă layoutul nu este eligibil |
| `022B` | `TexImage1D/2D/3D` | scrie direct numai după alocarea completă a nivelului; fallback exact |
| `022C` | `GL_GENERATE_MIPMAP` | aceeași valoare și aceleași puncte de apel, dar stocare per obiect/lifetime |
| `022D` | `MipmapStorage::MarkDirtyRegion` | early-return numai când noua regiune este integral conținută în union-ul deja pending |
| `022E` | DSA `TextureSubImage2D` | date identice; dirty full-level devine dirty subregion numai când toggle-ul este activ |

## Verificarea echivalenței

Harness-ul diferențial compilează ruta veche direct din sursa `021` și ruta
directă din `022`, apoi compară byte-for-byte rezultatele. Cazurile acoperă:

1. RGBA8 tight;
2. `RowLength` + `Alignment` + `SkipRows` + `SkipPixels`;
3. conversie Red → RGBA8;
4. volum 3D cu `ImageHeight` și `SkipImages`;
5. `SwapBytes` pe R16UI;
6. BGRA packed 8:8:8:8 REV;
7. depth float → Depth24Stencil8;
8. bitmap `LSBFirst`.

În buildul direct, fiecare octet din padding, prefix și suffix este santinelă.
Rezultat: ieșiri identice cu `021`, zero scrieri în afara rândurilor, ASan și
UBSan PASS. Stride-ul prea scurt și BPP-ul greșit sunt respinse fără mutarea
destinației.

Comandă reproductibilă:

```text
bash scripts/verify-pixelstore-direct-unpack.sh
```

## Verificarea Android/ELF

- Android arm64-v8a, API 26, NDK r27d / Clang 18.0.4;
- Release ThinLTO;
- Build ID: `7eff62cc7ca8ab6fcf82a0f017966b6a6d5ee6e5`;
- source ID: `c63202c01d21b897a0802578f383ed2c5f46cdf09fcf86ccf95b6146d3d0d91f`;
- SONAME: `libMobileGLPZ.so`;
- LOAD alignment: `0x4000`;
- exporturile dinamice sunt identice cu `021` (10.411 intrări în comparația
  normalizată);
- nicio dependență `libc++_shared.so`;
- selector schema: `5`.

Biblioteca de producție stripped și hashurile finale sunt în manifestul de
release.

## Surse primare folosite

- OpenGL ES 3.2 specification:
  https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf
- ANGLE `Context.cpp`, rutarea și validarea uploadurilor:
  https://chromium.googlesource.com/angle/angle/+/3605b399e094948159fe0a3578c5cec537a4e336/src/libANGLE/Context.cpp
- ANGLE `TextureGL.cpp`, frontend/backend texture upload:
  https://chromium.googlesource.com/angle/angle/+/0d0fb43f34eeea20bf78089ca2a4e1f2831cffe5/src/libANGLE/renderer/gl/TextureGL.cpp
- `EXT_buffer_storage`, folosit pentru a verifica de ce `021E` trebuie să
  rămână separat și strict eligibil:
  https://registry.khronos.org/OpenGL/extensions/EXT/EXT_buffer_storage.txt

## Pragul de oprire MobileGL

După testul `022`, headroom-ul MobileGL este considerat consumat dacă setul
complet nu mai aduce cel puțin aproximativ 3% repetabil în worst-window 1% low
sau dacă telemetria nativă rămâne sub aproximativ 1–1,5 ms/frame și fără spike
nativ dominant. În acel punct, `WORLD_STREAM_CHUNK`, `CHUNK_MAP_UPDATE` și
`CHUNK_GRID_LOAD` se atacă în APK/PZ/JVM; nu prin falsificarea rendererului și
nu prin reactivarea FBO.
