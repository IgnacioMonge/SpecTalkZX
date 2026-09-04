# Tools

Use `make NO_COLOR=1` for normal builds. Call tools directly only when working on the tool or regenerating its specific asset.

## Build tools

- `bpe_build.py`: prepares compressed source and generates `SPECTALK.DAT`.
- `bpe_compress.py`: compresses text intended for the screen.
- `gen_overlay_defs.py`: generates overlay symbols from the program map.
- `gen_whatsnew.py`: builds the What's New data from the files in `release/`.
- `overlay_atlas_probe.py`: packs the overlay files into `SPECTALK.OVL`.

## Other files

- `readme.txt`: Ikkle-4 font license/credit note.

The remaining files are validation tools or standalone experiments. Normal
builds run the required checks automatically; call an individual tool only
when changing or verifying that tool's output.
