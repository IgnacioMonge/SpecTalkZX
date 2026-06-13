---
name: preamble-crt-zero-fill-shrink
description: Measured rules for startup zero-fill code in `00_preamble.asm`.
---

# Preamble CRT Zero Fill

- For startup-only zero fills, prefer compact byte-store loops over `LDIR` when
  the goal is resident CODE size and a tiny boot-time slowdown is acceptable.
- Keep the general BSS clear guarded for zero size before entering the loop.
  Once inside, `BC` is the exact byte count and the loop writes first, then
  decrements/tests `BC`.
- `zero_fill_256` may use `xor a / ld b,a / djnz`: `B=0` gives exactly 256
  iterations, and the routine returns with `A=0`.
- It is safe for the following UDG clear to reuse that returned `A=0`, but only
  if no instruction between the `zero_fill_256` call and the UDG loop modifies
  `A`.
