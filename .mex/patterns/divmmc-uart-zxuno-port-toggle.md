# divMMC ZX-Uno UART Port Toggle

## Context
The divMMC/divTIESUS UART backend talks to the ZX-Uno compatible register interface through two adjacent port pages:

- `ZXUNO_ADDR = $FC3B`
- `ZXUNO_REG = $FD3B`

With `C=$3B`, switching between address select and register/data I/O is just a change to `B`.

## Rule
- After `ld bc,ZXUNO_ADDR`, `inc b` changes the port to `ZXUNO_REG`; if no call or register clobber intervenes, do not reload `ld bc,ZXUNO_REG`.
- After a register/data `in a,(c)` or `out (c),x` with `B=$FD`, use `dec b` to return to `ZXUNO_ADDR` when the next operation is another register select.
- Stores such as `ld (_is_recv),a` do not affect `BC`, so they do not break the `B=$FD` invariant.
- If a received byte must remain in `A` while clearing a memory latch, prefer `ld hl,latch / ld (hl),0` over shuttling through `E`, provided `HL` is not part of the helper's return contract.
- For resident hot RX drains, prefer the internal `uartRead` contract (`CF=1/A=byte`, `CF=0` no byte) over public ready/read wrappers. Once the drain no longer needs them and no C/ASM call sites remain, remove `_ay_uart_ready()` and `_ay_uart_read()` entirely; protect the drain loop counter across `uartRead` and `_rb_push()` because both clobber `BC`.
- Rebuild after UART port rewrites; assembler success does not prove the current backend stayed selected or that overlays still fit.

## Applied In
- `asm/divmmc_uart.asm` `_ay_uart_init()`
- `asm/divmmc_uart.asm` `_ay_uart_send()`
- `asm/divmmc_uart.asm` `uartRead_do_read`
- `asm/divmmc_uart.asm` removed dead `_ay_uart_ready()` / `_ay_uart_read()`
- `asm/spectalk_asm/40_text_numeric_screen.asm` `_uart_drain_to_buffer()`
