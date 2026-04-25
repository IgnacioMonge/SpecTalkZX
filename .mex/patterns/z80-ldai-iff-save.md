# Z80 `ld a,i` IFF Save

`ld a,i` can be used to copy IFF2 into P/V, but it has a known interrupt acceptance edge case on real Z80s. Do not use it as a casual IFF save/restore shortcut in code that can instead prove a fixed interrupt-state contract.

## Rule
- If a routine is a true generic helper, preserve the caller's interrupt state with a method whose edge cases are acceptable for the target hardware, or keep the old contract.
- If every callsite proves a fixed interrupt state, document that callsite contract in the routine and use explicit `di`/`ei`.
- Re-audit all callsites before converting a generic helper to fixed `di`/`ei`.

## Applied
- `_cls_fast()` is startup-only: the only callsite is `init_screen()`, reached during boot after ROM-enabled interrupts.
- It now uses fixed `di`/`ei` around the stack clear instead of `ld a,i` plus P/V restore.
- Measured result: normal build drops from `35674B` to `35667B` trimmed, with TAP `35747B` and BSS slack `468B`.
