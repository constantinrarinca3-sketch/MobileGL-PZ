# PZF23D4 device validation

Capture: `PZF23D4-CAPTURE-20260824-022635.zip`  
Collection time: 2026-08-24 02:26:35–02:26:38 +0300  
Owner visual verdict: `FIXED`

## Identity

- Build: `PZF23D4_MAP_FIX_V2_V1_EXACT_CUSTOM_SHADER_ALPHA_TEST_FAMILY`
- Build ID: `5a70674ea4cbd78c7b9cba1e048b60c5770e576b`
- expected/active library SHA-256:
  `f5b280fac78f2189daeac41d7d6b456747e1e1970754bcf807135ae33af82e8d`
- source manifest SHA-256:
  `189bc8383867202ce134c9e7da1602b0cd9250f130b9d97beed1a64b2887e619`
- raw log: `170691` bytes, SHA-256
  `00a6d3b5a862e004af4e8dad6ef54f680e846276cd62f94d13527f6e2f2642df`
- report SHA-256:
  `ee9393d2b5a3ed1e3c39db608ff23b3d08a08bc0608415cd674d7838d8d3e031`

All capture checksums pass. The raw stream is not truncated and the session and
library identities match.

## Contractual gates

| Gate | Result |
| --- | --- |
| `DATA_COLLECTED` | `YES` |
| `INSTRUMENTATION_ACTIVE` | `YES` |
| `REQUIRED_SHADER_FAMILY_REWRITTEN` | `YES` |
| `CHUNK_DRAW_OBSERVED` | `YES` |
| `PRODUCER_DRAW_OBSERVED` | `YES` |
| `PRODUCER_ALPHA_SEMANTIC_ACTIVE` | `YES` |
| `DEVICE_TEST_EXECUTED` | `YES` |
| `BUG_REPRODUCED` | `NO` |

## Rewrite proof

There are four valid and complete rewrite records, one for every required
contract:

- `chunk_composite`: 1
- `tile_with_depth`: 1
- `opaque_with_depth`: 1
- `seam_fix_2`: 1

Two near-miss records are present for structurally related shaders. They are
expected fail-closed refusals and do not replace or invalidate the four exact
required rewrites.

## Draw proof

All `192/192` target-draw records are complete and valid:

- `chunk_composite`: 64
- `depth_tile_producer`: 64
- `seam_producer`: 64

For every family, the live alpha semantic was active. Recorded values were:

- `alpha_enabled=1`
- `alpha_func=0x204` (`GL_GREATER`)
- `alpha_ref=0`

The collector conclusion was
`PRODUCER_AND_COMPOSITOR_ALPHA_EMULATION_EXECUTED`.

## Visual result

The large rectangular/square roof and foliage footprints visible in the original
and D3 images are absent in D4. Character visibility through cut-out vegetation
and doorway textures is also corrected. The technical execution evidence and the
owner's visual observation therefore support closure of BUG-001.

The original collector wording `CANDIDATE_FIXED_PENDING_REPEAT_AND_RAW_REVIEW`
was conservative. The raw review has since been completed: all decisive records
are coherent, there is no stale D2/D3 active marker, and no relevant shader
compile/link failure was found. This promotion records the owner's explicit
decision to make D4 the new stable base.

