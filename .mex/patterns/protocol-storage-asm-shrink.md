---
name: protocol-storage-asm-shrink
description: Notes for measured resident ASM shrink in `60_protocol_storage.asm`.
---

# Protocol Storage ASM Shrink

- In `_about_pump`, consume the byte budget with `djnz` only after `uartRead`
  returns a byte. Keep `C` as the no-data inter-byte poll budget; do not make
  no-data polls consume the ABOUT byte budget.
- For one-byte UART separators inside `_irc_send_cmd_internal`, prefer direct
  `_ay_uart_send` with `L=byte` over a private NUL-terminated string plus
  `_uart_send_string`, but preserve live pointer registers explicitly unless the
  caller no longer needs them.
- Do not land self-modifying esxDOS command-byte fusion in the SAFE shrink lane.
  `_esx_fread`/`_esx_fwrite` SMC needs explicit opt-in and real divMMC read/write
  hardware validation because it mutates resident code immediately before `rst 8`.
