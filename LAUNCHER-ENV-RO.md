# Variabile de environment în launcher

Variabilele se citesc o singură dată la inițializarea MobileGL. Oprește complet
aplicația înainte de schimbarea setului.

## Cea mai simplă alegere

În câmpul de environment al launcherului:

```text
MOBILEGL_PZ_OPT_PRESET=ALL_OFF
```

sau:

```text
MOBILEGL_PZ_OPT_PRESET=ALL_SAFE_ON
```

Pentru orice combinație explicită:

```text
MOBILEGL_PZ_OPT_SET=019,020
```

## Toggle individual

Fiecare variabilă acceptă `1/on/true/yes` sau `0/off/false/no`:

```text
MOBILEGL_PZ_OPT_002=1
MOBILEGL_PZ_OPT_003=1
MOBILEGL_PZ_OPT_004R=0
MOBILEGL_PZ_OPT_005=1
MOBILEGL_PZ_OPT_006=0
MOBILEGL_PZ_OPT_007=1
MOBILEGL_PZ_OPT_008=0
MOBILEGL_PZ_OPT_009=0
MOBILEGL_PZ_OPT_010=0
MOBILEGL_PZ_OPT_011=1
MOBILEGL_PZ_OPT_012=0
MOBILEGL_PZ_OPT_013=1
MOBILEGL_PZ_OPT_014=1
MOBILEGL_PZ_OPT_015=1
MOBILEGL_PZ_OPT_016=1
MOBILEGL_PZ_OPT_017=1
MOBILEGL_PZ_OPT_018=0
MOBILEGL_PZ_OPT_019=1
MOBILEGL_PZ_OPT_020=1
```

Precedența este: preset → set explicit → toggle-uri individuale. 003 adaugă
automat 002; 005 adaugă automat 002+003. Orice token/boolean invalid face
configurația fail-closed la `none` și scrie motivul în proof.

`018` este diagnostic eșantionat (1 draw din 64, maximum 2048) și trebuie oprit
în verdictul FPS. `019` și `020` pot fi pornite individual, împreună sau prin
`ALL_SAFE_ON`. Markerul `MGLPZ_OPT_CONFIG schema=3` arată setul rezolvat, iar
`MGLPZ_OPT_HIT`/`MGLPZ_OPT_SUMMARY` dovedesc execuția, nu doar configurarea.

Dacă launcherul nu transmite environment, scriptul din pachet scrie același set
în sidecarul privat `mglpz-opt-set.txt`.

## Reglaje existente utile

```text
MOBILEGL_ASYNC_SHADER_COMPILE_THREADS=2
```

Testează separat 2, 3 și 4; acesta este deja un reglaj MobileGL real, nu un ID
OPT-LAB nou. Cache-urile pot fi mutate numai dacă este necesar:

```text
MOBILEGL_PZ_PROGRAM_CACHE_DIR=/data/user/0/com.zomdroid.mglpz1/files/mglpz-program-cache-v1
MOBILEGL_PZ_SHADER_SOURCE_CACHE_DIR=/data/user/0/com.zomdroid.mglpz1/files/mglpz-essl-cache-v1
MOBILEGL_PZ_PROOF_FILE=/data/user/0/com.zomdroid.mglpz1/files/mglpz-opt-proof.log
```
