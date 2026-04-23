# Resident ASM Register Lifetime Shrink

## Context
Small ASM shrink wins often come from reusing an existing register instead of materializing a new 16-bit pair. This is safe only when the full register-pair lifetime is explicit. The `80_ui_runtime.asm` keyboard pass nearly violated this by using `DE` as a table offset after a later CAPS check had reused `D`.

## Pattern
- If a pointer/index uses `DE`, prove both `D` and `E`, not only the byte being changed. A later `add hl,de` observes the stale high byte.
- Prefer changing the scratch byte register instead of clearing the high byte again when the high byte is already proven stable.
- When a loop invariant is exact, replace recomputation with the constant form, for example row scan state that always reaches a known table offset before the final row.
- For bit-0 tests whose source byte is dead after the branch, `rrca` plus `ret c`/`jr c` can replace `bit 0,a` plus branch, but only after confirming no later code needs the unrotated value.

## Guardrails
- Do not use this for bit tests above bit 0 unless the rotate count is still a byte win and the mutated `A` is dead.
- Rebuild after register-lifetime rewrites; assembler success is not enough to prove behavior when stale high bytes are possible.
- Leave a short comment when a byte-pair invariant is non-obvious, such as `D = 0` before a table-section add.
