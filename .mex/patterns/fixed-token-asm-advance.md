# Fixed Token ASM Advance

Use when C compares a fixed token and then immediately advances past it:

```c
if (p[0] == 'P' && p[1] == 'I' && p[2] == 'N' && p[3] == 'G') {
    p += 4;
}
```

A local `__z88dk_fastcall ST_NAKED` helper can compare the token and return the advanced pointer in `HL`:

- `HL` starts at the candidate token.
- `DE` points to a local `DEFM` key.
- `B` is the fixed key length.
- On each matched byte, increment both `DE` and `HL`.
- On full match, `ret` with `HL` already one byte past the token.
- On mismatch, return `HL=0`.
- If the old C probe starts after a known separator byte such as `sp[1]`, the helper can `inc hl` before the loop and still return the post-token pointer.

This is useful when the caller probes more than one candidate location but wants the same post-token parameter pointer.

Guardrails:

- The helper must return NULL on mismatch, not a partially advanced pointer.
- Only use when old C also read the same fixed bytes at the same candidate location.
- Keep it local unless several measured call sites justify a shared helper.
- Build before accepting; SDCC may still beat this for one short comparison.
