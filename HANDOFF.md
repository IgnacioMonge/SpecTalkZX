# HANDOFF

Date: 2026-04-15
Project: `SpecTalkZX dev`
Purpose: quick resume file for continuing this repo from another machine or a fresh Codex session.

## Current state

- Last checked build: `make`
- Result: `exit 0`
- Output: `build/SpecTalkZX.tap`
- Final TAP size: `35589` bytes
- Overlay pack: `SPECTALK.OVL` built clean at `8192` bytes
- Previous noisy line `MISSING SYMBOLS: _fill_dither_rows, _render_globe` is gone

## Fixes done in this pass

1. Overlay load failure now unwinds UI state before showing the error.
   File: `asm/overlay_loader.asm`
   Effect: no more invisible error with `overlay_mode` stuck and cursor hidden after overlay load failure.

2. `overlay_call()` now validates both the entry index and the resolved entry pointer.
   File: `asm/overlay_loader.asm`
   Effect: corrupt overlay tables no longer jump outside the loaded 2K overlay block.

3. Status overlay uptime no longer breaks after `99h` and no longer wraps at `256h`.
   Files: `overlay/spectalk_ovl4.c`, `overlay/overlay_api.h`
   Effect: uptime hours use `uint16_t` plus `u16_to_dec()` instead of a 2-digit `u8` formatter.

4. Overlay defs generation no longer reports false missing symbols, and real failures now stop the build.
   Files: `tools/gen_overlay_defs.py`, `Makefile`
   Effect: `_fill_dither_rows` and `_render_globe` were removed from resident-symbol requirements because they belong to the overlay itself, not the resident binary. `gen_overlay_defs.py` is now wired with `|| exit 1`.

## Files touched in this pass

- `asm/overlay_loader.asm`
- `overlay/spectalk_ovl4.c`
- `overlay/overlay_api.h`
- `tools/gen_overlay_defs.py`
- `Makefile`
- `HANDOFF.md`

## Quick validation

Run:

```sh
make
```

Expected:

- build finishes with `exit 0`
- no `MISSING SYMBOLS` line at the end
- overlay sizes remain within `2048` bytes each
- final TAP reported near `35589` bytes unless unrelated code changed

## Important context

- This repo was already in a dirty worktree. Do not assume the only local modifications are the files listed above.
- If this file is included in a commit, that commit is the handoff bundle for this pass.
- Local Codex skills used during review on this machine included `audit-z80` and `shrink-z80`. They live outside the repo and will not automatically exist on another machine.
- If a future session does not remember this conversation, read this file first, then inspect the files listed in "Files touched in this pass".

## Good next steps if work resumes

1. Run `make` on the new machine and confirm the clean build matches this handoff.
2. If needed, diff the five touched files first before exploring wider repo changes.
3. If Z80 audit work continues, start from overlay safety, stack margin, and resident-vs-overlay symbol boundaries.
