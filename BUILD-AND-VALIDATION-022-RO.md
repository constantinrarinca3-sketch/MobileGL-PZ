# Build și validare — OPT-LAB V3 022

## Build

```text
export ANDROID_NDK_ROOT=/cale/către/android-ndk-r27d
bash scripts/build-mobilegl-pz-opt-lab-android.sh THINLTO
```

Contract:

- NDK `27.3.13750724`;
- `arm64-v8a`, Android API 26;
- static libc++ în ELF, fără `libc++_shared.so`;
- Release ThinLTO;
- un singur `.so`, selector runtime pentru fiecare ID.

## Verificări

```text
bash scripts/verify-pixelstore-direct-unpack.sh
bash scripts/verify-mobilegl-pz-opt-lab-v3-022.sh /cale/către/libMobileGLPZ.so
```

Rezultatul final din 2026-08-25:

```text
PIXELSTORE_021_022_EQUIVALENCE=PASS
PIXELSTORE_ASAN_UBSAN=PASS
PIXELSTORE_CASES=8
VERIFY_022=PASS
```

Identitate ELF stripped:

```text
name=libMobileGLPZ-OPT-LAB-V3-022-THINLTO.so
sha256=8dc064f0386d01fccab8b94681921fdf9c304ed8995e87e07f1906119728310f
size=14876840
build_id=7eff62cc7ca8ab6fcf82a0f017966b6a6d5ee6e5
source_id=c63202c01d21b897a0802578f383ed2c5f46cdf09fcf86ccf95b6146d3d0d91f
abi=arm64-v8a
soname=libMobileGLPZ.so
load_alignment=0x4000
normalized_dynamic_exports=10411
```

Setul normalizat de exporturi dinamice este identic cu `021`.
