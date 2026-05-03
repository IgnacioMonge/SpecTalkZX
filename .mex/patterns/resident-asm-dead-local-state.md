# Resident ASM Dead Local State

When shrinking tiny resident ASM helpers, remove local BSS bytes and side paths as soon as they no longer have in-tree writers, then re-evaluate the public wrappers against the helper's actual return contract.

## Rule
- If a local byte is not `PUBLIC` and no in-tree code writes it, treat any fast path that only reads it as dead; delete the branch and the storage together.
- After removing that dead state, check whether the internal helper already serves every live caller. A public wrapper can often collapse to `call helper / ld l,a / ret`; if all call sites moved to the helper contract, delete the wrapper entirely.
- If the remaining latched flag is guaranteed to stay `0/1`, let the fast path return it directly (`ld l,a` / `ret nz`) instead of branching to a literal `ld l,1`.
- Re-measure with `make`; keep the cleanup only if the full map confirms a real code/BSS win.

## Applied In
- `asm/divmmc_uart.asm` `uartRead`, `_ay_uart_init`, removed `_ay_uart_ready`, removed `_ay_uart_read`
