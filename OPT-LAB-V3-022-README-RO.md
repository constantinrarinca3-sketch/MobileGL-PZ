# MobileGL-PZ OPT-LAB V3 — 022 TEXTURE FRONTEND PACK

Data: 2026-08-25  
Țintă: Project Zomboid 42.20 prin ZomDroid, Android arm64-v8a

## Verdict

Mai există headroom în MobileGL, dar nu în FBO și nici în `program_sync`
steady-state. Logurile 021 arată mii de apeluri `TEXTURE_UPLOAD`, în timp ce
`021E` nu a găsit niciun upload eligibil pentru ringul PBO. Următoarea țintă
utilă este deci munca CPU din frontendul de texturi: procesare pixel-store,
copiere în shadow și propagarea regiunilor dirty.

`022` este un singur ELF cu cinci optimizări independente. Nu conține mod Java,
patch APK, patch WORLD, drop/reorder de chunkuri sau patch FBO.

## Optimizările 022

| ID | Optimizare | Lucru eliminat | `ALL_SAFE_ON` |
|---|---|---|---|
| `022A` | unpack direct pentru `TexSubImage*` | buffer temporar + a doua copiere în shadow | inclus |
| `022B` | unpack direct pentru `TexImage*` | buffer temporar + a doua copiere în shadow-ul deja alocat | inclus |
| `022C` | latch `GL_GENERATE_MIPMAP` în obiectul texturii | lookup global hash la fiecare upload level 0 | inclus |
| `022D` | fast path pentru dirty-region deja acoperit | reserve/seed/merge fără efect asupra uploadului | inclus |
| `022E` | dirty-region pentru DSA `TextureSubImage2D` | upload full-level când s-a modificat numai un subdreptunghi | inclus |

Toate sunt candidați safe, dar verdictul final de performanță și compatibilitate
se ia pe telefon. `021E` rămâne experimental, implicit oprit și nu este inclus
în matricea 022 deoarece testul anterior a raportat zero hit.

## Setul de control și setul complet

Controlul păstrează numai câștigurile MobileGL deja exercitate:

```text
019,020,021A,021B,021D
```

Setul 022 complet:

```text
019,020,021A,021B,021D,022A,022B,022C,022D,022E
```

Fiecare ID poate fi scos sau adăugat fără alt build:

```text
MOBILEGL_PZ_OPT_SET=019,020,021A,021B,021D,022A
MOBILEGL_PZ_OPT_SET=019,020,021A,021B,021D,022B
MOBILEGL_PZ_OPT_SET=019,020,021A,021B,021D,022C
MOBILEGL_PZ_OPT_SET=019,020,021A,021B,021D,022D
MOBILEGL_PZ_OPT_SET=019,020,021A,021B,021D,022E
```

Selecția se citește o dată la pornirea procesului. Aplicația trebuie oprită
complet și repornită după schimbare.

## Guardrail-uri

- `022A/B` folosesc exact aceeași conversie și aceleași reguli
  `RowLength`, `ImageHeight`, `Skip*`, `Alignment`, `SwapBytes` și `LSBFirst`
  ca ruta veche;
- dacă destinația, BPP-ul sau stride-ul nu corespund, ruta revine automat la
  implementarea originală;
- buildurile de diagnostic PZF9 păstrează ruta contiguă veche, astfel încât
  hashingul de proveniență să rămână comparabil;
- `022C` păstrează starea pe durata de viață a obiectului, deci un nume GL
  reciclat nu poate moșteni accidental valoarea veche;
- `022D` sare numai o actualizare complet conținută într-o regiune deja dirty;
- `022E` modifică doar granularitatea uploadului pending, nu datele texturii;
- `018` rămâne diagnostic și nu se activează în verdictul FPS;
- `021E` rămâne OFF.

Matricea scurtă și cea de atribuire sunt în
`OPT-LAB-V3-022-DEVICE-TEST-RO.md`.
