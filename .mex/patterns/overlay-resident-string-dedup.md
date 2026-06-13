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
5. Do not reuse BPE-renamed `SB_*` resident strings in overlay code that calls `print_str64()`, `print_char64()`, `st_stricmp()`, or any other non-BPE-aware consumer. Those paths need plain NUL-terminated text.
6. Only reuse `SB_*` from overlays when the receiving function is explicitly BPE-aware and the string is never used for byte-wise comparison.
7. If the resident string does not appear as a public map symbol and cannot be safely aliased, abandon unless exporting it has a measured net win.

## Example
- `overlay/spectalk_ovl4.c` now reuses resident config keys like `K_SERVER`, `K_PORT`, `K_NICK`, `K_PASS`, `K_NOTIF`.
- `overlay/spectalk_ovl5.c` reuses the same resident keys for the config screen labels where the displayed text matches exactly.
- Current accepted plain exports also include `S_ANYKEY`, `K_CLICK`, `K_AUTOJOIN`, and `K_CHANNELS`. Keep them plain `S_/K_` constants, not `SB_*`, because they feed `notif_center()`, `print_str64()`, or config file writers.

## Rejected
- Config overlays must not reuse `SB_ON`, `SB_OFF`, `SB_NOTSET`, or `SB_SMART` for values rendered with `print_str64()`. HW showed missing characters in `(not set)` and `smart`.
- `overlay/spectalk_ovl3.c` must not compare `args` against `SB_OFF` with `st_stricmp()`. Keep the local plain `"off"` string.
- Do not treat overlay-only relief as a TAP win by itself. The packed overlay file stays at fixed 2048-byte blocks; any resident helper/string added for relief must be funded by resident shrink or justified by the specific overlay margin.
