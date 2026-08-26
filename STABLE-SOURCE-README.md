# MobileGL-PZ stable source — BUG-001 fixed

Promoted: 2026-08-24  
Status: `AUTHORITATIVE_STABLE_SOURCE`  
Fix identity: `PZF23D4 exact custom-shader legacy alpha-test family`

This tree is the former authoritative `MAP FIX V2` source with the device-validated
BUG-001 repair integrated. The executable source is byte-identical to the PZF23D4
tree that produced the tested phone library. The files added under `docs/` are the
stable-promotion record and do not alter the compiled implementation.

## What was fixed

Project Zomboid custom chunk shaders were not reproducing the live legacy OpenGL
alpha-test semantics in the final chunk compositor and in the colour/depth FBO
producers. Transparent sprite texels could therefore write colour/depth for the
whole sprite quad. The visible effects were rectangular/square roof and vegetation
footprints plus incorrect character occlusion.

The fix:

- reads the live `alpha_enabled`, `alpha_func` and `alpha_ref` state;
- injects the exact legacy comparison into four fail-closed shader contracts;
- discards failed fragments before producer `gl_FragDepth` writes;
- has no hardcoded alpha cutoff;
- leaves the experimental frag-depth clamp disabled.

See `docs/BUG-001-ROOT-CAUSE-AND-FIX.md` for the full causal chain and code map.

## Validated implementation

The following original PZF23D4 phone library is the promoted stable binary:

- file: `libMobileGLPZ-PZF23D4.so`
- size: `14932744` bytes
- SHA-256: `f5b280fac78f2189daeac41d7d6b456747e1e1970754bcf807135ae33af82e8d`
- Build ID: `5a70674ea4cbd78c7b9cba1e048b60c5770e576b`

Device validation proved all four exact rewrites and 192/192 complete target draws.
The owner verdict was `FIXED`, and the original square footprints were absent.
See `docs/PZF23D4-DEVICE-VALIDATION.md`.

## Build

Use `scripts/build-mobilegl-pz-v1-android.sh` with Android NDK r27d
`27.3.13750724`. The script verifies `SOURCE-FILES-SHA256.txt`, enables only D4
and builds `arm64-v8a`, API 26, Release. Full commands and packaging instructions
are in `docs/BUILD-AND-PHONE-INSTALL.md`.

## Evidence included in this source archive

`docs/evidence/` contains:

- the original BUG-001 image;
- the D3 partial improvement image;
- the D4 fixed full image and zoom;
- the complete PZF23D4 device-capture ZIP;
- the decisive report and its capture checksum manifest.

## Remaining cosmetic observation

A very thin pale fringe can be seen on several broad vegetation leaves near the
veranda. It follows the leaf edge rather than the sprite quad and is not a return
of BUG-001. It is recorded separately in `docs/KNOWN-MINOR-ALPHA-FRINGE.md`.

