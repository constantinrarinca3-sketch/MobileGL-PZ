# MobileGL PZ OPT-LAB — build de pe tabletă

Acest sistem compilează **numai biblioteca `libMobileGLPZ.so`**. Nu reconstruiește și
nu înlocuiește APK-ul ZomDroid.

Compilarea grea rulează pe GitHub. Tableta este folosită doar pentru editarea sursei,
trimiterea modificărilor și descărcarea rezultatului.

## Ce rămâne fix

- arhitectură: `arm64-v8a`;
- Android API: `26`;
- NDK: `27.3.13750724` (r27d);
- CMake: `3.22.1`;
- STL: `c++_static`;
- rezultat: `libMobileGLPZ.so`;
- verificări OPT-LAB V3-022 înainte și după compilare.

Aceste valori vin din scriptul existent
`scripts/build-mobilegl-pz-opt-lab-android.sh`; workflow-ul nu inventează altă
configurație.

## Cum faci fiecare test

1. Modifici codul pentru o singură ipoteză de reparare.
2. Trimiți modificarea în depozitul tău privat GitHub.
3. În pagina depozitului deschizi **Actions**.
4. Alegi **Build MobileGL PZ OPT-LAB**.
5. Apeși **Run workflow**.
6. Alegi varianta:
   - `NOLTO` — recomandat cât repari ploaia, zăpada și quad-urile; compilează mai repede;
   - `THINLTO` — pentru buildul final de testare.
7. După terminare, deschizi rularea și descarci arhiva din secțiunea **Artifacts**.

Dacă buildul este verde, în `result/` găsești:

- `libMobileGLPZ.so` — biblioteca de pus pe dispozitiv;
- `SHA256SUMS.txt` — amprenta buildului;
- `BUILD-INFO.txt` — varianta, commitul, source ID și build ID;
- `VERIFY.txt` și informații ELF — verificările automate.

Dacă buildul este roșu, arhiva conține `build.log`. Acesta este fișierul util
pentru a afla exact ce nu a compilat.

## Configurarea o singură dată

Ai nevoie de:

1. un cont GitHub;
2. un depozit **privat** în care pui această sursă exactă;
3. GitHub Actions activat pentru acel depozit.

Pe tabletă, varianta practică este Termux. Instalezi instrumentele:

```bash
pkg update
pkg install git gh zstd
termux-setup-storage
gh auth login
```

Apoi intri în directorul sursei deja extrase și îl trimiți prima dată pe GitHub:

```bash
cd /calea/catre/MobileGL-PZ-OPT-LAB-V3-022-EXACT-SOURCE-2026-08-25
git init -b main
git config user.name "Numele tau"
git config user.email "emailul-tau"
git add .
git commit -m "Sursa exacta OPT-LAB V3-022"
gh repo create mobilegl-pz-opt-lab --private --source=. --remote=origin --push
```

La următoarele încercări, după ce modifici codul:

```bash
git add MobileGL include CMakeLists.txt
git commit -m "Test reparare ploaie si quad"
git push
```

După `git push`, pornești buildul din fila **Actions** așa cum este descris mai sus.

## Punerea bibliotecii pe dispozitiv

Folosește aceeași procedură Shizuku/rish pe care o foloseai deja și păstrează un
backup al bibliotecii funcționale. Destinația verificată pentru pachetul existent este:

```text
/data/user/0/com.zomdroid.mglpz1/files/dependencies/libs/android-arm64-v8a/libMobileGLPZ.so
```

Nu reinstala și nu înlocui APK-ul doar pentru a testa această bibliotecă.

## Regulă utilă pentru cele două buguri

Testează separat:

- un commit pentru problema ploii/zăpezii;
- un commit pentru problema quad-urilor.

Astfel, dacă o schimbare strică interfața sau imaginea, poți reveni ușor la ultimul
commit bun. Nu activa global repararea veche `PZF16`; sursa o ține intenționat oprită,
deoarece acea variantă a reparat unele quad-uri, dar a corupt interfața.

