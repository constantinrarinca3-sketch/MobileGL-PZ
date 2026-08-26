# BUG-001 — root cause, investigation and stable fix

## Final status

`RESOLVED` on 2026-08-24 by PZF23D4, then promoted to the authoritative stable
source at the owner's request.

## Original symptom

The affected scene showed several coupled rendering errors:

- roof pieces looked overlaid or selected from rectangular/square regions;
- vegetation looked like flat PNG rectangles instead of cut-out foliage;
- a character could disappear too aggressively when walking behind vegetation,
  through a doorway or behind other cut-out textures;
- the problem was not limited to the legacy "cutaway" name.

Representative content families were `roofs_30_02`, `roofs_02_7`,
`roofs_accents_30_01`, `d_generic`, `d_plants`, `e_newgrass` and `f_bushes`.

## What was ruled out

The investigation established that the following were not the repair:

- Java sprite selection, atlas selection, UVs, queueing and binds;
- compressed upload, RGBA conversion and mip level selection;
- ESSL precision/highp and vertex state;
- per-tile R8 depth upload and R8-to-RGBA8 conversion;
- final depth attachment format and its readback;
- UBO persistent-ring versus alternate upload path;
- scalar `chunkDepth` UBO layout/update, proven by the valid-negative J1R1 relay;
- an isolated clamp of `chunkDepth + depthTexel` in the final compositor.

The D1/D2 clamp path was therefore diagnostic, not the final fix.

## Causal progression

### D3 — positive but incomplete

D3 reproduced the live legacy alpha-test only in the final `chunkShader`
compositor. It materially improved character occlusion: the character no longer
disappeared as severely behind vegetation and doorway textures. The large square
footprints remained.

That result proved the missing legacy alpha-test semantics were causal, while also
showing that applying them only after the producer FBOs had been generated was too
late. Their colour/depth textures already contained contributions from transparent
parts of the source sprite quads.

### D4 — complete producer and compositor repair

D4 preserves the D3 compositor semantic and applies the same live legacy alpha
test to the three exact producer contracts that create the chunk colour/depth
inputs. A failed alpha comparison executes `discard` before the producer
`gl_FragDepth = calcDepthZ` write.

The four exact shader contracts are:

| Shader contract | Runtime classification | Repair |
| --- | --- | --- |
| `chunkShader.frag` | `chunk_composite` | Test final `c * col` alpha before final colour/depth contribution |
| `tileWithDepth.frag` | `depth_tile_producer` | Test final `c.a` before `calcDepthZ` write |
| `opaqueWithDepth.frag` | `depth_tile_producer` | Test final `c.a` before `calcDepthZ` write |
| `seamFix2.frag` | `seam_producer` | Test final `c.a` before `calcDepthZ` write |

The rewrite is fail-closed. It checks the structural shader contract and refuses
near-matches or already-injected variants. It does not globally rewrite shaders.

## Exact semantics

The renderer uploads the actual front-state values on every matching draw:

- `pzf23d4AlphaEnabled`
- `pzf23d4AlphaFunc`
- `pzf23d4AlphaRef`

The injected function implements `GL_NEVER` through `GL_ALWAYS` (`512` through
`519`). No cutoff is guessed or hardcoded. On the decisive scene the live state
was enabled, `GL_GREATER` (`0x204`), reference `0`.

This matters because even a texel with alpha zero must not write producer depth.
Allowing transparent texels to write depth explains both the full rectangular
footprints and the character occlusion failure.

## Source map

- `MobileGL/MG_Util/ShaderTranspiler/ShaderSourceProcessor.cpp`
  - `RewritePZCustomAlphaTestFamilyEarly`
  - exact shader-contract recognition and injected alpha-test/discard code
- `MobileGL/MG_Util/ShaderTranspiler/ShaderSourceProcessor.h`
  - probe and shader-family types
- `MobileGL/MG_Impl/GLImpl/PZCompat/PZCompat.cpp`
  - `PZF23D4PrepareCustomProgramDraw`
  - uploads live legacy alpha state and fails closed by program interface
- `MobileGL/MG_Test/Program/ProgramUtilTest.cpp`
  - exact-contract, idempotence, near-miss rejection and SPIR-V validation
- `CMakeLists.txt`
  - feature dependency and mutual-exclusion gates
- `scripts/build-mobilegl-pz-v1-android.sh`
  - authoritative stable Android configuration with D4 enabled

## Why D4 is the stable solution

The same binary that ran on the device produced all required rewrite/draw evidence
and removed the user-visible defect. The technical and visual observations agree:
the legacy alpha-test must be restored both where the chunk colour/depth textures
are produced and where they are composed.

No Java, atlas, UV, geometry, FBO format or hardcoded alpha-threshold change is
part of the stable repair.

