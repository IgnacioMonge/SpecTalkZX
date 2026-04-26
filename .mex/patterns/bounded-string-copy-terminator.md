# Bounded String Copy Terminator

## Context
Resident bounded string copies often have two termination cases: source ended early, or the byte budget was exhausted. These cases can share less code only if the destination terminator contract remains explicit.

## Rule
- For `strncpy`-style helpers that always NUL-terminate, it is safe to write `ld (de),a` before `or a` when `A` came directly from `ld a,(hl)`.
- If `A` is NUL, return immediately after the test; the terminator is already in the destination.
- Keep a separate count-exhausted tail that writes `xor a / ld (de),a`, because that path exits after copying a non-NUL byte and still needs a safety terminator.
- Use this only after stack restoration is complete, or when every early `ret` has the same stack shape as the normal exit.

## Applied In
- `asm/spectalk_asm/60_protocol_storage.asm` `_st_copy_n()`
