# MobileGL-PZ OPT-LAB V2

Data: 2026-08-24  
Stare: **gata pentru test pe device; nu este încă build stable**

OPT-LAB V2 este un singur MobileGL cu optimizările native selectabile la
pornirea procesului. Excepția inevitabilă este ThinLTO: fiind opțiune de
compilare, pachetul conține două ELF-uri construite din aceeași sursă,
`THINLTO` și `NOLTO`. Nu există un toggle fals pentru LTO.

Vechiul PERF-004 DXT→ETC2 runtime este exclus. `004R` este o implementare nouă:
passthrough S3TC numai dacă driverul anunță exact formatul DXT; altfel revine la
traseul compatibil existent.

## Toggle-uri implementate

| ID | Cale | Profil |
|---|---|---|
| 002 | sync texturi numai pe unitățile sampler folosite de program | safe candidate |
| 003 | compare/bind texturi program-aware | safe candidate; cere 002 |
| 004R | upload S3TC nativ cu fallback verificat | experimental/device |
| 005 | sampler binding program-aware cu memo per program | safe candidate; cere 002+003 |
| 006 | telemetrie waits/finish/readback/UBO | diagnostic, nu FPS |
| 007 | cache persistent pentru program binary GLES | safe candidate |
| 008 | forward exact al invalidate cerut de aplicație | experimental/device |
| 009 | telemetrie staging și texture-upload bytes | diagnostic, nu FPS |
| 010 | buffer ID swap cu pensionare GPU-safe la respecify | experimental/device |
| 011 | mask/cache program-aware pentru image units | safe candidate |
| 012 | mask program-aware pentru SSBO bind/writeback | experimental/device |
| 013 | GC incremental și staggered pentru registrele backend | safe candidate |
| 014 | sync VBO numai pentru atributele citite de program | safe candidate |
| 015 | coadă amortizată O(1) pentru UBO frame marks | safe candidate |
| 016 | cache persistent pentru sursa ESSL finală | safe candidate |
| 017 | shadow corect invalidat pentru bindingul renderbuffer | safe candidate |

Fiecare cale are marker `MGLPZ_OPT_HIT` sau metrică asociată. Un toggle doar
configurat, dar nereatins de workload, este raportat `ACTIVE_NOT_EXERCISED`; nu
este declarat activ pe baza variabilei singure.

## Preseturi

- `ALL_OFF`: toate toggle-urile runtime oprite.
- `CUMULATIVE_ACCEPTED`: 002+003.
- `ALL_SAFE_ON` / `ALL_PERF_ON`: 002,003,005,007,011,013,014,015,016,017.
- `TELEMETRY_ONLY`: 006+009. Nu se folosește pentru verdict FPS.
- `ALL_EXPERIMENTAL_ON`: toate 002–017; util pentru smoke/stability, nu pentru
  atribuirea unui câștig.

Selecția exactă, regulile de precedență și exemplele pentru launcher sunt în
`LAUNCHER-ENV-RO.md`. Auditul fiecărei idei din lista autoritativă este în
`OPTIMIZATION-AUDIT-MATRIX-RO.md`.

## Regula de benchmark

Baseline-ul principal este ELF-ul `THINLTO` cu `ALL_OFF`. Pentru efectul
ThinLTO se compară `NOLTO/ALL_OFF` cu `THINLTO/ALL_OFF`. Apoi se schimbă un
singur ID sau un singur delta de set, pe aceeași rută din zona aglomerată.

006 și 009 se rulează separat când trebuie localizat blocajul. Ele folosesc
numai contoare bounded și nu fac census cu `glGet*`, dar tot sunt instrumentație
și nu intră în profilul `ALL_SAFE_ON`.

