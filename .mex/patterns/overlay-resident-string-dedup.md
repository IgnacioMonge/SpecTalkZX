# Overlay Resident String Dedup

When an overlay needs a string that already exists in the resident core with identical bytes, export that resident symbol through the overlay ABI and reuse it instead of keeping a second overlay-local copy.

## Why
- Overlay pack size is fixed at `4 x 2048`, so most overlay-only wins recover headroom rather than shrinking the TAP.
- Reusing an already-existing resident constant is still worthwhile because it reduces per-overlay pressure without changing behavior or UI text.
- Do not move overlay-only text into the resident core just to deduplicate it; that grows the TAP and usually loses.

## How
1. Add the resident `extern const char ...[];` to `overlay/overlay_api.h`.
2. Export the matching symbol in `tools/gen_overlay_defs.py` so overlays can link against it.
3. Replace the overlay-local literal only when the text is byte-for-byte identical.
4. Rebuild with `make` and compare individual `SPCTLK*.OVL` sizes, not just the final TAP.

## Example
- `overlay/spectalk_ovl4.c` now reuses resident config keys like `K_SERVER`, `K_PORT`, `K_NICK`, `K_PASS`, `K_NOTIF`.
- `overlay/spectalk_ovl2.c` now reuses the same resident keys for the config screen labels where the displayed text matches exactly.
