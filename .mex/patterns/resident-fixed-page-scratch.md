# Resident Fixed-Page Scratch

Resident ASM can use bytes in the documented printer-buffer tail as transient scratch when the value is live only inside a no-esxDOS/no-overlay window.

## Rule
- Reuse only the explicitly free `$5BE7-$5BFF` tail bytes, and document the exact live range at the use site.
- For wrap scans, a stored `start` sentinel can replace an index register: initialize the scratch pointer to `start`, update it on each later candidate, and treat `scratch == start` as "none usable".
- Fixed-page stack cursors such as `bpe_rstack=$5BD5..$5BE5` may use low-byte comparisons instead of 16-bit `sbc hl,rr` when all writers are bounded to that same page, but compare against assembler-time constants derived beside the base definition, not repeated raw low-byte literals in the hot path.
- Rebuild after these changes; the byte savings often bring nearby `jp`/`jr` decisions back to the edge of range.

## Applied In
- `asm/spectalk_asm/50_main_output.asm`: `main_print_wrapped_ram()` uses `$5BE7-$5BE8` for `mpwr_last_space`, and `_main_puts()` checks `bpe_rsp` against `bpe_rstack_lo`/`bpe_rstack_top_lo` low-byte bounds.
