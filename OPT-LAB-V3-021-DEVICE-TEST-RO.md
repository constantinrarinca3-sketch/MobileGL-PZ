# Matrice device — 021 RENDERER PACK

Scop: atribuirea fiecărei optimizări și apoi validarea combinației, toate pe
același ELF ThinLTO.

## Reguli fixe

- aceeași salvare, rută, direcție, zoom, rezoluție, moduri și setări;
- aceeași stare de alimentare și temperatură inițială apropiată;
- 3 s warm-up, apoi 60–90 s în aceeași zonă aglomerată;
- oprire completă și repornire după fiecare selector;
- Benchmark V4 rămâne activ;
- `018` nu este activ în niciun verdict FPS;
- se colectează proof-ul nativ, DebugLog-ul și raportul Benchmark V4;
- se notează separat crash, flicker, texturi corupte și chunkuri negre.

## Matrice minimă

| Run | Selector | Rol |
|---|---|---|
| `C0` | `019,020` | controlul stabilit de testele anterioare |
| `A1` | `019,020,021A` | efect individual texture shadow epoch |
| `B1` | `019,020,021B` | efect individual sampler shadow epoch |
| `C1` | `019,020,021C` | efect individual normal UBO replay memo |
| `D1` | `019,020,021D` | efect individual SSBO fused worklist |
| `S1` | `019,020,021A,021B,021C,021D` | setul safe combinat |
| `E1` | `019,020,021E` | experiment PBO izolat, implicit OFF |

Ordinea recomandată este `C0 → A1 → B1 → C1 → D1 → S1 → E1`. Dacă timpul
este limitat, se rulează mai întâi `C0`, `S1`, apoi `E1`; atribuirea A–D se
completează numai dacă S1 câștigă sau apare o regresie.

## Comenzi

Cu sidecar:

```text
bash 02-SET-OPTIMIZATIONS.sh 019,020
bash 02-SET-OPTIMIZATIONS.sh 019,020,021A
bash 02-SET-OPTIMIZATIONS.sh 019,020,021B
bash 02-SET-OPTIMIZATIONS.sh 019,020,021C
bash 02-SET-OPTIMIZATIONS.sh 019,020,021D
bash 02-SET-OPTIMIZATIONS.sh 019,020,021A,021B,021C,021D
bash 02-SET-OPTIMIZATIONS.sh 019,020,021E
```

Echivalentele se pot pune în `MOBILEGL_PZ_OPT_SET` în launcher. Environment-ul
launcherului are prioritate față de sidecar.

## Verdict

Prioritate: worst-window 1% low, p99/p99.9/max, `ge50/ge100`, hitch-uri și
stabilitatea vizuală. Average FPS este secundar.

Un ID este păstrat numai dacă:

- câștigul se repetă în zona grea;
- proof-ul arată `resolved` corect și hit pe calea lui;
- nu introduce crash/flicker/corupție sau loading întârziat;
- nu înrăutățește worst-window 1% low pentru un câștig numai în zone ușoare.

Pentru `E1`, cere în plus:

- evenimente `pbo_upload_staged` și eventual fallbackuri bounded;
- zero așteptări adăugate și zero `glFinish` atribuit 021E;
- reducerea repetabilă a spike-urilor `TEXTURE_UPLOAD`;
- fără texturi negre/corupte după zoom, condus rapid și revenire în zone deja
  încărcate.

Dacă `E1` este instabil, se oprește doar `021E`; nu se aruncă A–D și nu se face
alt build.

## Nivel de analiză a logurilor

`XHigh` este suficient pentru matricea normală. `Max` se folosește numai pentru
crash greu, corupție vizuală sau discrepanță de proof care nu poate fi explicată
din logul standard.
