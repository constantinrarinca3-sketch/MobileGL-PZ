# Istoric: auditul și reparația EGL

> Acesta este documentul checkpointului EGL anterior consolidării finale.
> Starea curentă, inclusiv regresia PZF16→R1 și reparația mapei/preview-ului,
> este în `FINAL-SOURCE-PROVENANCE-2026-08-20.md`.

Data analizei: 2026-08-20  
Pachet analizat: `MobileGL-PZ-V1-STABLE-BASE-SOURCE-2026-08-20`  
Stare: **sursă reparată și verificată static; fără build sau test Android**

## 1. Ce conține sursa

Proiectul este un strat de compatibilitate OpenGL/EGL pentru platforme mobile. El
expune API-uri GL/EGL către joc, menține starea logică desktop OpenGL și traduce
operațiile către un backend nativ.

| Zonă | Rol principal |
|---|---|
| `MobileGL/MG_Impl` | implementările și exporturile GL, EGL, GLX și WGL văzute de aplicație |
| `MobileGL/MG_State` | starea logică EGL/GL, obiectele wrapper și regulile de validare |
| `MobileGL/MG_Backend/DirectGLES` | traducerea către OpenGL ES și administrarea display/context/surface native |
| `MobileGL/MG_Backend/DirectVulkan` | backend-ul alternativ Vulkan |
| `MobileGL/MG_Util` | încărcare driver, transpilation shader, conversii, utilitare și auto-diagnostic |
| `MobileGL/MG_Test`, `MG_IntegrationTest`, `MG_Benchmark` | teste, scenarii și benchmark-uri incluse în arbore, dar neexecutate în această etapă |
| `include` | headerele publice GL/EGL/GLES/KHR |
| `3rdparty` | glslang, SPIRV-Cross/Reflect, DiligentCore, Vulkan headers/utilities, apitrace, Tracy, Asio etc. |
| `android-plugin`, `buildsystem`, `scripts` | integrarea Android, CMake/Gradle și scripturile de build/audit |

Sursa furnizată conține 29.854 fișiere regulate înaintea acestui raport și este
însoțită de un manifest SHA-256 reproducibil.

## 2. Linia de modificări inclusă

Baza declarată în sursă este:

`P7 + P15 + P21A + P28 + P28M + P28V + PZF1 + PZF16R1 + PZF16R2`

Pe scurt:

- P15 restaurează starea client vertex-array după `glPopClientAttrib`;
- P21A păstrează regiunile modificate per layer la texturi 2D-array;
- P28 leagă memoizarea EBO/VAO și de durata de viață a obiectului;
- P28M încearcă să păstreze contextul EGL la schimbarea imediată a suprafeței;
- P28V invalidează cache-ul binding-ului VAO după `MakeCurrent`;
- PZF1 redenumește lexical coliziunile shader cu builtin-urile `clamp`, `max`, `min`;
- PZF16 R1/R2 păstrează numai corecția de producție pentru cazurile exacte
  `DrawRangeElements(GL_QUADS, count=4)` selectate de starea world/depth sau de
  lifetime-ul țintei TextureCombiner.

Această bază diferă de binarele diagnostice PZF16R1/R2: P28H și instrumentarea
PZF2–PZF15 sunt excluse din build-ul V1, iar CMake refuză activarea lor împreună cu
`MOBILEPZ_V1_CANDIDATE`. Rămâne doar starea minimă necesară corecțiilor acceptate.

## 3. Simptomul investigat

Comportamentul raportat este dependent de timp:

1. jocul intră în multitasking;
2. EGL mai rămâne activ pentru scurt timp;
3. după ce Android elimină suprafața ferestrei, contextul nativ este distrus;
4. revenirea foarte rapidă poate evita distrugerea, dar o ședere mai lungă în
   background o declanșează indiferent de meniul în care se află jocul.

Faptul că problema apare în meniu, opțiuni și înainte de intrarea în lume indică o
problemă de lifecycle EGL, nu o anumită rută de randare PZF16.

## 4. Cauza găsită

P28M funcționa numai când o suprafață nouă era creată imediat, în timp ce vechiul
context putea fi mutat direct pe ea. Secvența Android reală are însă o perioadă fără
fereastră între pause/stop și resume.

În acea secvență, apelul wrapper `eglDestroySurface` ajungea la:

`BackendObject_DirectGLES::OnEGLSurfaceReleased()`

iar funcția executa necondiționat:

```cpp
DestroyEGLContext();
```

Prin urmare, distrugerea suprafeței era tratată greșit ca distrugere de context.
Erau pierdute contextul GLES nativ, generația sa și toate obiectele GPU asociate.
Această ramură explică și întârzierea: ea rulează când Android/launcher-ul elimină
efectiv suprafața, nu chiar în momentul apăsării butonului de multitasking.

## 5. Reparația aplicată

A fost adăugată ruta `DirectGLES::ReleaseSurface()`:

1. detașează contextul de pe thread prin `eglMakeCurrent(..., EGL_NO_CONTEXT)`;
2. distruge numai EGLSurface-ul nativ vechi;
3. setează suprafața activă la `EGL_NO_SURFACE`;
4. păstrează `EGLDisplay`, `EGLConfig`, `EGLContext` și generația contextului;
5. lasă ruta P28M existentă să atașeze noul window/pbuffer la resume;
6. folosește teardown complet numai dacă driverul refuză detașarea/distrugerea
   suprafeței sau dacă aplicația cere explicit `eglTerminate`.

Ruta P28M de schimbare imediată a suprafeței folosește acum aceeași funcție, astfel
încât există o singură implementare pentru eliberarea surface-only.

Markerul de pornire a fost ridicat la `schema=3` și declară
`idle_surface_context_preserve=on`. La o eliberare reușită se emite markerul rar de
lifecycle `MOBILEGL_PZ_V1_EGL_IDLE_PRESERVE`; nu s-a adăugat telemetrie pe fiecare
draw sau frame.

## 6. Fișiere modificate

| Fișier | Modificare |
|---|---|
| `MobileGL/MG_Backend/DirectGLES/DirectGLES.h` | declară operația surface-only |
| `MobileGL/MG_Backend/DirectGLES/DirectGLES.cpp` | implementează păstrarea contextului fără fereastră și reutilizează ruta în P28M |
| `MobileGL/MG_Backend/DirectGLES/BackendObject_DirectGLES.cpp` | înlocuiește teardown-ul necondiționat cu release surface-only și fallback limitat |
| `MobileGL/Init.cpp` | marker V1 `schema=3` pentru identificarea sursei reparate |
| `scripts/verify-mobilegl-pz-v1-stable-base.sh` | adaugă invariante statice pentru noua rută |
| `MOBILEGL-PZ-V1-STABLE-BASE.md` | documentează reparația și statutul fără build Android |
| `STABLE-BASE-AUDIT.md` | separă auditul binarului original de modificarea sursă ulterioară |
| `REZUMAT-SURSA-SI-REPARATIE-EGL.md` | acest raport |

## 7. Ce nu poate garanta această corecție

Corecția elimină distrugerea internă eronată a contextului atunci când dispare doar
suprafața. Ea nu poate împiedica Android să omoare întregul proces în background din
cauza memoriei, politicii bateriei sau a lifecycle-ului aplicației gazdă. Dacă moare
procesul, niciun context EGL nu poate supraviețui din interiorul acestei biblioteci.

De asemenea, sursa nu conține Activity-ul principal al launcher-ului jocului, deci o
eventuală terminare explicită a procesului sau a JVM-ului în `onStop` trebuie reparată
în proiectul launcher separat.

## 8. Statut de verificare

- nu s-a produs un APK sau un `libMobileGLPZ.so` nou;
- nu s-a executat un test Android sau pe dispozitiv;
- binarul Android și simbolurile de debug furnizate au rămas nemodificate;
- verificatorul static `verify-mobilegl-pz-v1-stable-base.sh`: **PASS**;
- sintaxa scripturilor de verificare/build: **PASS**;
- manifestul final: **29.855 intrări verificate SHA-256, PASS**;
- s-au păstrat regulile existente pentru teardown explicit și fallback;
- au fost adăugate verificări statice ca ruta de surface release să nu mai ocolească
  păstrarea contextului.

Validarea pe telefon rămâne pentru o etapă ulterioară, când va fi cerută explicit.
