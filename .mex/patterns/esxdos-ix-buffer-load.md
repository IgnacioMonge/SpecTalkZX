# esxDOS IX Buffer Load

## Context
esxDOS read/write calls use `IX` as the buffer pointer. The assembler supports direct indexed-register loads from absolute addresses.

## Rule
- Prefer `ld ix, (_esx_buf)` over `ld hl, (_esx_buf) / push hl / pop ix` when loading the esxDOS buffer pointer.
- Preserve the surrounding `push ix` / `pop ix` contract unless every caller has been re-audited.
- Rebuild after this change: indexed absolute-load syntax is assembler-specific even though it is accepted by the current z80asm toolchain.

## Applied In
- `asm/spectalk_asm/60_protocol_storage.asm` `_esx_fread()`
