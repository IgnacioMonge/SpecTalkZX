# BPE-Aware Main Print Fast Path

`main_print()` is part of the BPE-aware resident output path, even when it looks like a simple short-line wrapper.

## Rule

- During any short-line scan, a byte `>=0x80` must force the slow path through `_main_puts()`.
- `_print_line64_fast()` renders literal character bytes; it does not expand BPE tokens.
- Do not normalize high bytes to spaces in `main_print()` just because `_print_line64_fast()` has blank handling for its own padding/invalid-byte contract.
- The former C implementation got this guard from `tools/bpe_build.py` via `patch_spectalk_c`; an ASM rewrite must implement the same behavior explicitly.
- HW symptom when this is wrong: startup/help/status strings show clean gaps or missing words while normal RAM text still looks sane.

## Applies To

- `asm/spectalk_asm/50_main_output.asm` `_main_print`
- `tools/bpe_build.py` `patch_spectalk_c`
- `src/spectalk.c` `main_print()` if it ever returns to C
