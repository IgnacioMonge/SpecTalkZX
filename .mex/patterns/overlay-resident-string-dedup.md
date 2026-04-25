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
5. If a resident string is BPE-renamed in the main build (`S_*` source -> `SB_*` map symbol), expose the `SB_*` name to overlay code and let `tools/gen_overlay_defs.py` optionally fall back to the original `S_*` symbol for non-BPE maps.
6. If the resident string does not appear as a public map symbol and cannot be safely aliased, abandon unless exporting it has a measured net win.

## Example
- `overlay/spectalk_ovl4.c` now reuses resident config keys like `K_SERVER`, `K_PORT`, `K_NICK`, `K_PASS`, `K_NOTIF`.
- `overlay/spectalk_ovl2.c` now reuses the same resident keys for the config screen labels where the displayed text matches exactly.
- `overlay/spectalk_ovl2.c` reuses `SB_ON`, `SB_OFF`, `SB_NOTSET`, and `SB_SMART` for config values. Measured result: SPCTLK2 drops from `2037B` to `2014B` with no resident-size change.
- `overlay/spectalk_ovl3.c` reuses `SB_OFF` and `SB_MIN` in the cold `!autoaway` command. Measured result: SPCTLK3 drops from `1853B` to `1844B` with no resident-size change.
