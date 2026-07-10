# Tools

Use `make NO_COLOR=1` for normal builds. Call tools directly only when working on the tool or regenerating its specific asset.

## Build-Live

- `bpe_build.py`: build orchestrator; copies sources, applies BPE-safe rewrites, runs compression, generates `SPECTALK.DAT`, and patches generated offsets.
- `bpe_compress.py`: BPE analyzer/compressor for screen-only strings; UART, config, file paths, and direct-render strings must stay plain ASCII.
- `gen_overlay_defs.py`: reads the resident `.map` and emits `overlay_defs.asm` for the overlay ABI.
- `gen_whatsnew.py`: converts `release/logo.png`, `release/changes.txt`, and `release/version.txt` into `overlay/whatsnew_data.h`.
- `overlay_atlas_probe.py`: packs fixed 2K `SPCTLK*.OVL` blocks into the variable-length STOA `SPECTALK.OVL` atlas.

## Other Files

- `readme.txt`: Ikkle-4 font license/credit note.

Lab/prototype tools are not part of the supported regeneration path. Before promoting any lab result, document the measured build effect in `.mex/ROUTER.md` and move only the needed code into the build-live path.
