# MobileGL PZ V1 — final repaired source candidate

Status: sursă consolidată, verificată static și compilată ARM64 la 2026-08-21.
Validarea vizuală pe telefon rămâne separată. Arhiva este candidatul unic pentru
următorul test pe dispozitiv.

Istoricul complet, hash-urile intrărilor și limitele auditului sunt în
`FINAL-SOURCE-PROVENANCE-2026-08-20.md`.

## Reparația World Map din 2026-08-21

Capturile de dispozitiv au confirmat că UI-ul, stencilul și etichetele SDF sunt
active, dar geometria texturată a mapei lipsește. Sursa exactă a jocului arată că
`UIWorldMap` desenează preview-ul Create New World și mapa din joc prin
`VBORenderer`, iar tile-urile/texturile folosesc `GL_QUADS`. Ruta V1 veche
convertea numai un subset `DrawRangeElements(..., count=4)` și lăsa aceste
apeluri să fie respinse de validatorul core.

Schema 5 adaugă o rută fără diagnostic și fără selector global:

- acceptă numai loturi `GL_QUADS` complete, cu număr de vârfuri multiplu de 4;
- cere stencilul exact al `UIWorldMap`: stencil activ, depth oprit,
  `GL_EQUAL`, referință 1;
- cere interfața shaderului `VBORenderer`: `aPosition`, `aColor`,
  `ModelViewProjection` și `userDepth`;
- separă fiecare lot în triangle-fan-uri independente de câte patru vârfuri
  pentru `DrawArrays`, `DrawElements` și `DrawRangeElements`;
- păstrează separat identitatea exactă MAP SDF deja confirmată;
- nu schimbă matrice, stencil, depth, blend, actori sau SpriteRenderer generic.

Testele predicatei acoperă loturile, stencilul, interfața shaderului și refuzul
fail-closed. Buildul ARM64 curat compilează cu PZF16 global oprit.

## Linia de producție păstrată

`P7 + P15 + P21A + P28 + P28M + P28V + PZF1 + PZF16R1 + PZF16R2 + bounded R1 regression repair`

- P15 restaurează starea client vertex-array după `glPopClientAttrib`.
- P21A păstrează regiunile modificate per layer pentru texturi 2D-array.
- P28 leagă memoizarea EBO/VAO și de lifetime-ul obiectului.
- P28M păstrează contextul EGL la schimbarea window/pbuffer.
- P28V invalidează cache-ul VAO nativ după `MakeCurrent`.
- PZF1 redenumește lexical coliziunile PZ cu `clamp`, `max` și `min` înainte de
  glslang. Ramura este activată direct și de `MOBILEPZ_V1_CANDIDATE`; un test
  standalone compilează predicatul chiar cu macro-ul V1 și canary-ul istoric oprit.
- PZF16R1/R2 păstrează conversia exactă
  `DrawRangeElements(GL_QUADS, count=4) -> GL_TRIANGLE_FAN` pentru lumea cu
  depth activ și pentru fereastra TextureCombiner urmărită.

## Regresia PZF16 → R1 și reparația finală

PZF16 traducea global quad-urile exacte și a restaurat lumea/personajul/ceața,
dar pe telefon a produs panouri Building duplicate și bare verticale. R1 a
restrâns conversia la FBO-ul TextureCombiner, iar R2 a redeschis numai ruta world
cu depth activ.

Ramurile rămase nu erau însă „păstrate”: `GL_QUADS` ajungea la validatorul core,
care îl respinge cu `GL_INVALID_ENUM`, deci desenul dispărea. Acesta este
mecanismul regresiei mapei/preview-ului animat din character creation.

Sursa finală nu revine la selectorul global PZF16. Ea adaugă un handoff
producer→consumer cu următorul contract închis:

1. ținta este o textură `Texture2D`, `RGBA8`, nivel 0, maximum 512×512, alocată
   cu date NULL;
2. un quad exact a fost tradus și trimis în fereastra FBO TextureCombiner activă;
3. aceeași atașare este detașată, publicând lifetime-ul exact al texturii;
4. numai primul `DrawRangeElements(GL_QUADS, 4)` depth-disabled care eșantionează
   acea textură pe unitatea 0 poate consuma tokenul și deveni triangle fan;
5. redefinirea stocării, schimbarea atașării și resetarea contextului invalidează
   tokenul; coliziunile tabelului direct produc numai refuz, nu false-positive.

Astfel, atlasurile UI statice care au produs corupția PZF16 nu primesc token.
`DrawArrays` și `DrawElements` nu au fost redeschise fără o semnătură demonstrată.
Nu există query GLES, readback, mutex, contor sau log pe ruta de draw.

## Reparația EGL păstrată

Wrapper-ele nu mai distrug contextul înainte ca P28M să poată păstra schimbarea
de surface. `DirectGLES::ReleaseSurface()` detașează contextul și distruge numai
EGLSurface-ul vechi în intervalul Android fără fereastră; teardown-ul complet
rămâne fallback la eșec și pe ruta explicită de terminare.

## Izolare și optimizare

P28H și instrumentarea PZF2–PZF15 au fost eliminate din runtime-ul V1. CMake
refuză combinarea V1 cu switch-urile diagnostice/legacy, iar helperul de build le
fixează la `OFF`. Trackerul final folosește tabele atomice directe pe ruta caldă;
setul cu mutex este consultat numai la operații reci de clear/population.

Înainte de derivarea Build ID-ului, helperul verifică integral
`SOURCE-FILES-SHA256.txt`. Manifestul se regenerează explicit cu
`scripts/update-source-manifest.sh`; un build nu îl modifică automat.

## Următoarea etapă de test

Helperul Android este `scripts/build-mobilegl-pz-v1-android.sh`. După build se
validează separat: character creation cu mapa/preview-ul animat, world/player/
trees/fog, meniul Building și radial, plus cold start, background/resume scurt și
lung, screen off/on, rotație/recreare și shutdown/relaunch.

Markerul runtime al acestei surse este:

`MOBILEGL_PZ_V1_FINAL_REPAIRED_ACTIVE schema=5`
