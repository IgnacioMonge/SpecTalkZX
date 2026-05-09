# Fixed Token ASM Match

Use for one-off C comparisons such as:

```c
p[0] == 'N' && p[1] == 'E' && ...
```

If the input pointer is already valid for the compared token length and the compare is exact, a local `__z88dk_fastcall ST_NAKED` helper can be smaller:

- `HL` is the candidate pointer.
- `DE` points to a local `DEFM` key.
- `B` is fixed key length.
- Loop: `ld a,(de)`, `cp (hl)`, `jr nz,no`, `inc de`, `inc hl`, `djnz loop`.
- Return boolean in `L`.

Guardrails:

- Do not use on unbounded raw server text unless prior parsing guarantees the bytes are present or readable.
- Keep it local to the file unless a second measured call site proves shared ABI/export cost is worthwhile.
- Rebuild before accepting; SDCC sometimes emits smaller C for shorter two- or three-byte tests.
