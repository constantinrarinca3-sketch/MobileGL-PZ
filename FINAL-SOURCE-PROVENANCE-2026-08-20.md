# Proveniența și istoricul sursei finale

Data consolidării inițiale: 2026-08-20  
Actualizare: 2026-08-21  
Stare: **sursă reparată, teste de predicat trecute și build ARM64 reușit;
validarea vizuală pe telefon este în așteptare**

> Amendament 2026-08-21: concluziile istorice despre handoff-ul
> producer→consumer rămân valabile, dar nu mai reprezintă întreaga reparație a
> mapei. Schema 5 adaugă selectorul `UIWorldMap stencil + VBORenderer interface`
> și conversia corectă a fiecărui lot `GL_QUADS` în fan-uri independente. Detaliile
> curente sunt în `MOBILEGL-PZ-V1-STABLE-BASE.md`; acest amendament prevalează
> asupra afirmațiilor vechi că buildul nu fusese executat.

Acest fișier este checkpointul de reluare. Dacă sesiunea se întrerupe, el conține
deciziile, cauza regresiei, modificările păstrate și verificările necesare.
Rezultatele tehnice ale verificării sunt în `STATIC-VERIFICATION-2026-08-20.md`.

## 1. Intrări auditate

| Intrare | SHA-256 | Rol |
|---|---|---|
| `egl.tar-1.zst` | `4c7b14097bee296d25ea2572a2690f2042c30a2db5b596de404b5f29c25aaf76` | baza aleasă; lifecycle EGL reparat |
| `original.tar.zst` | `9a5b04a4a08bb540bf796d668b667751cffcb479111a5926a5c52987788006da` | sursa de control |
| pachetul PZF15/PZF16/R1/R2 | `05ab9d69678e8b1ccc1f40cd07e7683ded4efde57e0757bcd8ccc9e9b7e2de14` | patch-uri, manifeste și loguri de device |

Arhiva EGL și ORIGINAL au aproximativ 29.8k fișiere fiecare. Comparația recursivă
a găsit numai 14 intrări diferite. ORIGINAL nu conține nicio implementare PZF1,
R1 sau R2 care să lipsească din EGL.

## 2. De ce baza EGL a fost aleasă

Baza EGL păstrează aceeași logică PZF1 și același selector R1/R2 ca ORIGINAL,
dar repară două rute reale:

- wrapper-ele window/pbuffer nu mai execută teardown înainte de P28M;
- eliberarea temporară a surface-ului Android păstrează display/config/context și
  generația contextului prin `DirectGLES::ReleaseSurface()`.

În plus, baza EGL elimină monitorul P28H și loggerul one-shot quad4. Copierea
runtime-ului din ORIGINAL peste ea ar reintroduce teardown-ul greșit și telemetria,
fără să adauge vreo funcție de producție lipsă.

Cele 14 intrări diferite au fost: `CMakeLists.txt`, documentul candidate din
ORIGINAL, documentul stable din EGL, `MobileGL/Config.h`, `MobileGL/Init.cpp`,
`BackendObject_DirectGLES.cpp`, `DirectGLES.cpp`, `DirectGLES.h`, `PZCompat.cpp`,
`REZUMAT-SURSA-SI-REPARATIE-EGL.md`, `STABLE-BASE-AUDIT.md`,
`SOURCE-FILES-SHA256.txt`, helperul de build și verificatorul static. Dintre ele,
diferențele runtime sunt strict lifecycle-ul EGL și eliminarea telemetriei;
implementarea PZF1 și decizia R1/R2 erau aceleași în ambele surse.

## 3. PZF1 este implementat real

PZF1 nu este doar un marker. Lanțul verificat este:

1. CMake definește `MOBILEPZ_V1_CANDIDATE=1` pentru targetul final;
2. `EsslBuiltinFunctionNames.h` tratează `clamp`, `max`, `min` ca nume care pot
   umbri builtin-urile când este activ fie canary-ul PZF1, fie V1;
3. `ShaderSourceProcessor.cpp` redenumește lexical numai definițiile/prototipurile
   utilizatorului și apelurile ulterioare în `mg_clamp`, `mg_max`, `mg_min`;
4. preprocessing-ul rulează înainte de parsarea glslang;
5. testele de preprocess acceptă și garda `MOBILEPZ_V1_CANDIDATE`, iar un target
   standalone compilează direct predicatul cu V1 și fără canary-ul istoric.

Canary-ul `MOBILEPZ_PZF1_PZ_MATH_BUILTIN_RENAME=OFF` din helperul de build nu
dezactivează ramura V1; previne doar reactivarea lanțului diagnostic separat.

## 4. Moștenirea PZF15 → PZF16 → R1 → R2

| Etapă | Rezultat relevant |
|---|---|
| PZF15 | diagnostic; a demonstrat `FRONTEND_DRAW_NO_NATIVE_SUBMIT` pentru quad4 |
| PZF16 | traduce global quad4 din Arrays/Elements/Range; restaurează imaginea, dar corupe UI |
| PZF16R1 | păstrează numai Range în fereastra FBO țintă; elimină corupția, introduce regresii |
| PZF16R2 | redeschide Range în afara țintei numai cu depth activ; restul rămâne respins |

Logurile arată PZF16 peste 65.536 de conversii rapide pe FBO 2 și corupția
Building. R1 arată aceleași apeluri ca `OUTSIDE_TARGET_WINDOW action=preserve_original`.
R2 arată `OUTSIDE_TARGET_DEPTH_DISABLED action=preserve_original`.

În cod, „preserve_original” nu este o conservare vizuală: validatorul
`IsAcceptedPrimitiveMode` nu acceptă `GL_QUADS`, înregistrează `GL_INVALID_ENUM`
și nu ajunge la submit nativ. Aceasta explică exact de ce o rută prezentă în PZF16
poate dispărea în R1 și rămâne absentă în R2.

Pachetele nu includ arborii FULL_SOURCE declarați ca sibling pentru fiecare
etapă; genealogia exactă s-a verificat din patch-uri, manifeste, scripturi și
loguri. Această limită este documentată, nu ascunsă.

## 5. Reparația bounded pentru mapa/preview-ul animat

Nu există în logurile primite o captură delimitată exclusiv la character creation,
deci program/FBO/hash-uri numerice nu au fost hardcodate. Ele sunt dinamice și prea
generale. Revenirea la PZF16 global a fost exclusă deoarece corupția UI este deja
dovedită pe telefon.

Reparația implementează handoff-ul producer→consumer în
`PZV1QuadTargetTracker.h`, `FramebufferObject.cpp` și `PZCompat.cpp`:

- trackerul acceptat identifică o țintă NULL-allocated RGBA8 2D de maximum 512×512;
- un submit quad4 tradus marchează fereastra drept producătoare;
- detach-ul exact publică lifetime-ul texturii într-un tabel atomic direct;
- primul quad4 Range, depth-off, din afara țintei poate traduce numai dacă unitatea
  0 are exact acel lifetime; tokenul este consumat atomic o singură dată;
- reallocation, upload definit, invalidare și reset de context șterg tokenul;
- o coliziune de tabel poate doar să refuze o conversie legitimă, niciodată să
  accepte o textură diferită.

Aceasta este cea mai îngustă corecție derivabilă din dovezile existente. Ea
repară control-flow-ul sursei, însă verdictul vizual pentru mapa animată se obține
numai în etapa de device test.

Dacă acel ecran construiește mapa direct dintr-un atlas încărcat de CPU, nu din
textura unui render target publicat, selectorul va refuza intenționat ruta. În acel
caz este necesară captura delimitată a ecranului pentru o a doua semnătură; sursa
nu maschează acest risc printr-o reactivare globală.

## 6. Ce a fost eliminat sau nu a fost readăugat

- P28H health/present/draw telemetry;
- PZF2 și PZF3–PZF15 ca instrumentare runtime;
- PZF16 global;
- conversia V1 pentru `DrawArrays` și `DrawElements` fără semnătură demonstrată;
- readback-uri, query-uri GLES, event counters și loguri per-draw;
- identificatori dinamici hardcodați pentru program, FBO sau textură.

Fișierele istorice guarded rămân în arbore pentru proveniență și experimente
standalone, dar CMake refuză activarea lor împreună cu V1.

## 7. Modificări finale față de baza EGL

- selector producer→consumer și invalidările sale;
- test unitar pentru single-use, mismatch, redefinition și coliziune fail-closed;
- există un test standalone care dovedește activarea PZF1 prin macro-ul V1;
- marker runtime `schema=4` și identitate `FINAL-REPAIRED`;
- verificator static extins pentru noul contract și absența mutexului pe draw;
- helperul Android verifică manifestul înainte de Build ID;
- script explicit, determinist, pentru regenerarea manifestului;
- documentația consolidată de proveniență și testare.

## 8. Verificat acum vs. rămas pentru etapa următoare

Verificat în această etapă:

- integritatea arhivelor de intrare;
- comparația EGL vs ORIGINAL și auditul celor 14 diferențe;
- genealogia patch-urilor și cauza control-flow a regresiei;
- invariantele sursei, sintaxa scripturilor și manifestul SHA-256;
- teste structurale ale selectorului prezente în arbore;
- absența telemetriei P28H/quad-route din runtime-ul V1.

Neexecutat intenționat:

- build Android/ELF;
- teste unitare compilate;
- APK sau instalare;
- test vizual pe telefon;
- performanță și lifecycle real după noul build.

Următorul verdict trebuie să compare explicit character creation, Building/radial
și world, nu doar faptul că jocul pornește.
