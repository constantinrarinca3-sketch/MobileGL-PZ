# Build and phone installation

## Required toolchain

- Android NDK r27d `27.3.13750724`
- CMake `3.22.1`
- Ninja `1.13`
- Android target: `arm64-v8a`, API `26`, Release

## Build

From the source root:

```bash
export ANDROID_NDK_ROOT=/absolute/path/to/android-ndk-r27d
export PYTHONPATH=/absolute/path/to/build-tools${PYTHONPATH:+:$PYTHONPATH}
export PATH=/absolute/path/to/build-tools/bin:$PATH
bash scripts/build-mobilegl-pz-v1-android.sh /absolute/path/to/build-dir
```

The script first verifies every entry in `SOURCE-FILES-SHA256.txt`, derives the
deterministic build ID from that manifest, enables PZF23D4 and disables the D2/D3
alternatives.

Create the phone-sized library with the NDK strip tool:

```bash
$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip \
  --strip-all /absolute/path/to/build-dir/libMobileGLPZ.so
```

The promoted device package deliberately contains the exact ELF already validated
on the phone:

- size: `14932744` bytes
- SHA-256: `f5b280fac78f2189daeac41d7d6b456747e1e1970754bcf807135ae33af82e8d`
- Build ID: `5a70674ea4cbd78c7b9cba1e048b60c5770e576b`

MobileGL embeds absolute `__FILE__` paths in read-only strings. Consequently, a
clean build made from a differently named absolute source directory has the same
verified source manifest and deterministic Build ID but can have a different ELF
size/SHA after stripping. Use the source-manifest verification and Build ID to
identify such rebuilds; use the stable device package when the exact tested bytes
are required.

## Install on the current phone setup

Keep the installed ZomDroid APK and package `com.zomdroid.mglpz1`. Do not replace
the APK. Use the supplied stable device ZIP and run from its extracted folder:

```bash
bash 01-INSTALL-PZF23D4.sh
```

The script verifies the library SHA-256, backs up the current library, stages the
replacement through Shizuku/rish and verifies the installed bytes at:

`/data/user/0/com.zomdroid.mglpz1/files/dependencies/libs/android-arm64-v8a/libMobileGLPZ.so`

Rollback remains available through:

```bash
bash 03-ROLLBACK-PREINSTALL.sh
```

The collector remains in the package for future regression verification, but a
new BUG-001 test is not required merely to install the promoted stable build.
