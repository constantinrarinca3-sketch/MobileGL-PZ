# Test device — 022 TEXTURE FRONTEND PACK

Același `.so` conține toate cele cinci optimizări și toate toggle-urile. Nu se
instalează alt build între configurații.

## Reguli fixe

- Benchmark V4 R3 rămâne activ;
- aceeași salvare, rută, sens, zoom, rezoluție, moduri și setări;
- aceeași temperatură inițială pe cât posibil;
- fiecare configurație parcurge traseul de două ori;
- oprire completă și repornire după schimbarea selectorului;
- `018`, `021C` și `021E` rămân oprite;
- notează crash, flicker, texturi corupte, chunkuri negre sau încărcare târzie.

## Faza 1 — verdict rapid, patru trasee

| Config | Selector | Repetări |
|---|---|---|
| `B0` | `019,020,021A,021B,021D` | 2 |
| `F1` | `019,020,021A,021B,021D,022A,022B,022C,022D,022E` | 2 |

Aceasta răspunde rapid dacă pachetul întreg merită. Dacă `F1` este clar mai
bun și stabil, rămâne candidatul de utilizare. Dacă diferența este amestecată
sau apare regresie, se trece la atribuirea individuală de mai jos.

## Faza 2 — atribuirea fără alte builduri

| Config | Selector |
|---|---|
| `A1` | `019,020,021A,021B,021D,022A` |
| `B1` | `019,020,021A,021B,021D,022B` |
| `C1` | `019,020,021A,021B,021D,022C` |
| `D1` | `019,020,021A,021B,021D,022D` |
| `E1` | `019,020,021A,021B,021D,022E` |

Nu trebuie rulate automat toate cinci. Proof-ul decide ordinea:

- `ACTIVE_NOT_EXERCISED`: ID-ul nu a intrat pe ruta testată; nu consuma încă
  două runuri pentru el;
- hit cu `work_avoided=0`: fallback sau lipsă de economie; prioritate mică;
- hit cu `work_avoided>0`: merită cele două runuri individuale;
- dacă `F1` regresează, testează mai întâi ID-ul cu cel mai mare volum de lucru
  și apoi elimină-l din combinație.

## Comenzi

```text
bash 02-SET-OPTIMIZATIONS.sh 019,020,021A,021B,021D
bash 02-SET-OPTIMIZATIONS.sh 019,020,021A,021B,021D,022A,022B,022C,022D,022E
```

După fiecare run:

```text
bash 05-COLLECT-V4-RUN.sh B0-R1 "/storage/emulated/0/Download/DebugLog.txt"
```

Schimbă eticheta pentru fiecare repetare (`B0-R2`, `F1-R1`, `F1-R2` etc.).

## Verdict

Prioritate: worst-window 1% low, p99/p99.9/max, `ge50/ge100`, hitch-uri și
stabilitate vizuală. Average FPS este secundar.

Un ID este păstrat numai dacă:

- proof-ul arată configurația `resolved` corectă și ruta este exercitată;
- câștigul se repetă în zona grea;
- nu produce crash, flicker, corupție ori încărcare întârziată;
- nu sacrifică worst-window 1% low pentru un câștig numai în zone ușoare.

Nivel recomandat pentru analiza logurilor: `XHigh`. `Max` numai pentru crash
nativ greu, corupție vizuală sau proof contradictoriu.
